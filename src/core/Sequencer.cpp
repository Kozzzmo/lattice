#include "core/Sequencer.h"

#include <algorithm>
#include <cmath>

namespace lattice
{

void Sequencer::prepare (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    for (auto& v : voices)
        v.prepare (sr);
    reset();
}

void Sequencer::reset()
{
    for (auto& v : voices)
        v.kill();
    state = {};
    wasRunning = false;
    triggerOrder = -1;
    liveNote = -1;
    playhead.active.store (false);
}

void Sequencer::audition (int track, int note, int instrument) noexcept
{
    const int w = auditionWrite.load (std::memory_order_relaxed);
    auditionQueue[(size_t) (w % (int) auditionQueue.size())] = { track, note, instrument };
    auditionWrite.store (w + 1, std::memory_order_release);
}

bool Sequencer::locate (const Song& song, int64_t tick, int fixedOrder, bool followSong, Position& out)
{
    if (tick < 0)
        return false;

    const int tpl = std::max (1, song.tpl);
    const int64_t rows = tick / tpl;
    out.tickInRow = (int) (tick % tpl);
    out.rowsSinceStart = rows;

    if (followSong)
    {
        int64_t total = 0;
        for (int i = 0; i < song.numOrders(); ++i)
            if (auto* p = song.patternForOrder (i))
                total += p->numRows;
        if (total <= 0)
            return false;

        out.loopCount = rows / total;
        int64_t pos = rows % total;
        for (int i = 0; i < song.numOrders(); ++i)
        {
            auto* p = song.patternForOrder (i);
            if (p == nullptr)
                continue;
            if (pos < p->numRows)
            {
                out.order = i;
                out.rowInPattern = (int) pos;
                out.pattern = p;
                return true;
            }
            pos -= p->numRows;
        }
        return false;
    }

    auto* p = song.patternForOrder (fixedOrder);
    if (p == nullptr || p->numRows <= 0)
        return false;
    out.order = fixedOrder;
    out.pattern = p;
    out.rowInPattern = (int) (rows % p->numRows);
    out.loopCount = rows / p->numRows;
    return true;
}

bool Sequencer::shouldPlay (const Song& song, const Cell& cell, int order, int row, int track, int64_t loopCount)
{
    const int prob = cell.prob == kNone ? 100 : cell.prob;
    if (prob >= 100) return true;
    if (prob <= 0)   return false;
    const uint32_t loopKey = song.varyEachLoop ? (uint32_t) loopCount : 0u;
    const uint32_t h = hashMix (hashMix (hashMix (song.seed, (uint32_t) order), (uint32_t) (row * 31 + track)), loopKey);
    return hashToUnit (h) * 100.0f < (float) prob;
}

void Sequencer::process (const Song& song, const TransportInfo& transport,
                         juce::AudioBuffer<float>& audio, const juce::MidiBuffer& midiIn, juce::MidiBuffer& midiOut)
{
    const int n = audio.getNumSamples();
    audio.clear();

    const int tpl = std::max (1, song.tpl);
    const int tpb = std::max (1, song.lpb) * tpl;
    const double bpm = transport.bpm > 1.0 ? transport.bpm : 120.0;
    const double ticksPerSample = bpm / 60.0 * tpb / sr;

    playhead.bpm.store (bpm);
    playhead.hostPlaying.store (transport.hostPlaying);
    playhead.ppq.store (transport.ppq);

    // ---- editor auditions ------------------------------------------------------------
    const int w = auditionWrite.load (std::memory_order_acquire);
    while (auditionRead != w)
    {
        auto a = auditionQueue[(size_t) (auditionRead % (int) auditionQueue.size())];
        ++auditionRead;
        if (a.track < 0 || a.track >= kNumTracks)
            continue;
        auto& st = state[(size_t) a.track];
        if (a.instrument >= 0 && a.instrument < kNumInstruments)
            st.instrument = a.instrument;
        if (a.note == kNoteOff) { releaseNote (song, a.track, 0, midiOut); continue; }
        playNote (song, a.track, a.note, 100.0f / 127.0f, false, 0, midiOut);
        st.auditionNote = a.note;
        st.auditionSamples = (int) (0.3 * sr);
    }
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& st = state[(size_t) t];
        if (st.auditionSamples > 0)
        {
            st.auditionSamples -= n;
            if (st.auditionSamples <= 0 && st.note == st.auditionNote)
                releaseNote (song, t, std::min (n - 1, std::max (0, n + st.auditionSamples)), midiOut);
        }
    }

    // ---- incoming MIDI: pattern triggers or live playing ---------------------------------
    for (const auto meta : midiIn)
    {
        const auto m = meta.getMessage();
        const int s = std::clamp (meta.samplePosition, 0, std::max (0, n - 1));
        const int entry = m.isNoteOnOrOff() ? m.getNoteNumber() - song.triggerBaseNote : -1;

        if (song.mode == PlayMode::Trigger && entry >= 0 && entry < song.numOrders())
        {
            if (m.isNoteOn())
            {
                allNotesOff (song, s, midiOut);
                triggerOrder = entry;
                triggerNote = m.getNoteNumber();
                triggerStartTick = freeTick + s * ticksPerSample;
            }
            else if (m.getNoteNumber() == triggerNote)
            {
                triggerOrder = -1;
                triggerNote = -1;
                allNotesOff (song, s, midiOut);
            }
            continue;
        }

        const int lt = std::clamp (liveTrack.load(), 0, kNumTracks - 1);
        if (m.isNoteOn())
        {
            playNote (song, lt, m.getNoteNumber(), m.getFloatVelocity(), false, s, midiOut);
            liveNote = m.getNoteNumber();
        }
        else if (m.isNoteOff() && m.getNoteNumber() == liveNote)
        {
            releaseNote (song, lt, s, midiOut);
            liveNote = -1;
        }
    }

    // ---- choose the clock ------------------------------------------------------------------
    const bool preview = previewRequest.load() && ! transport.hostPlaying;
    if (preview && ! previewWasPlaying)
    {
        allNotesOff (song, 0, midiOut);
        previewStartTick = freeTick;
    }
    previewWasPlaying = preview;

    bool running = false, follow = false;
    int fixedOrder = song.selectedOrder;
    double startTick = 0.0;

    if (preview)
    {
        running = true;
        startTick = freeTick - previewStartTick;
    }
    else if (song.mode == PlayMode::Trigger)
    {
        running = triggerOrder >= 0;
        fixedOrder = triggerOrder;
        startTick = freeTick - triggerStartTick;
    }
    else if (transport.hostPlaying)
    {
        running = true;
        follow = song.mode == PlayMode::Song;
        startTick = transport.ppq * tpb;
    }

    if (! running && wasRunning)
        allNotesOff (song, 0, midiOut);
    if (! running)
        lastTick = std::numeric_limits<int64_t>::min();
    wasRunning = running;
    playhead.active.store (running);

    // ---- walk the ticks of this block -----------------------------------------------------
    int cursor = 0;
    if (running)
    {
        // Continue from the last tick when the clock is continuous, so floating point jitter at
        // block edges can never play a row twice; re-sync on jumps (host loops, relocation).
        const bool continuous = lastTick != std::numeric_limits<int64_t>::min()
                                && std::abs (startTick - expectedStartTick) < 0.5;
        auto k = continuous ? lastTick + 1 : (int64_t) std::ceil (startTick - 1.0e-6);
        for (;;)
        {
            const double offset = ((double) k - startTick) / ticksPerSample;
            const int s = std::max (0, (int) std::floor (offset + 1.0e-4));
            if (s >= n)
                break;
            renderTo (audio, cursor, s);
            cursor = s;
            processTick (song, k, fixedOrder, follow, s, midiOut);
            lastTick = k;
            ++k;
        }
        if (lastTick == std::numeric_limits<int64_t>::min())
            lastTick = k - 1; // no tick in this (short) block: remember where we are
        expectedStartTick = startTick + n * ticksPerSample;
    }
    renderTo (audio, cursor, n);
    freeTick += n * ticksPerSample;

    // gentle master saturation keeps summed chip voices from hard clipping
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
    {
        auto* d = audio.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = std::tanh (d[i]);
    }
}

void Sequencer::renderTo (juce::AudioBuffer<float>& audio, int from, int to)
{
    if (to <= from || audio.getNumChannels() == 0)
        return;
    float* l = audio.getWritePointer (0) + from;
    float* r = audio.getNumChannels() > 1 ? audio.getWritePointer (1) + from : l;
    for (auto& v : voices)
        v.render (l, r, to - from);
}

void Sequencer::processTick (const Song& song, int64_t tick, int fixedOrder, bool followSong, int s, juce::MidiBuffer& out)
{
    Position pos;
    if (! locate (song, tick, fixedOrder, followSong, pos))
        return;

    playhead.order.store (pos.order);
    playhead.row.store (pos.rowInPattern);

    for (int t = 0; t < kNumTracks; ++t)
    {
        const int len = song.trackLength (t, *pos.pattern);
        int row = pos.rowInPattern;
        if (len != pos.pattern->numRows)
            row = (int) ((followSong ? (int64_t) pos.rowInPattern : pos.rowsSinceStart) % len);
        playhead.rows[t].store (row);

        if (pos.tickInRow == 0)
            startRow (song, t, pos.pattern->at (t, row), pos, row, s, out);
        tickEffects (song, t, pos.tickInRow, s, out);
    }
}

void Sequencer::startRow (const Song& song, int t, const Cell& cell, const Position& pos, int row, int s, juce::MidiBuffer& out)
{
    auto& st = state[(size_t) t];

    if (st.fxCmd == 'A' && cell.fxCmd != 'A' && st.arpOffset != 0)
    {
        st.arpOffset = 0;
        voices[(size_t) t].setPitchOffset (0.0f);
    }
    if (st.fxCmd == 'V' && cell.fxCmd != 'V')
        voices[(size_t) t].setVibrato (0.0f, 0.0f);

    st.fxCmd = cell.fxCmd;
    st.fxVal = cell.fxVal;
    st.cutTick = -1;
    st.delayTick = -1;

    const bool allowed = shouldPlay (song, cell, pos.order, row, t, pos.loopCount);
    const uint32_t humanKey = hashMix (hashMix (song.seed ^ 0x5bd1e995u, (uint32_t) pos.rowsSinceStart), (uint32_t) t);

    if (cell.fxCmd == 'D' && cell.fxVal > 0 && cell.fxVal < song.tpl)
    {
        st.delayed = cell;
        st.delayedAllowed = allowed;
        st.delayedKey = humanKey;
        st.delayTick = cell.fxVal;
        return;
    }
    applyCell (song, t, cell, allowed, humanKey, s, out);
}

void Sequencer::applyCell (const Song& song, int t, const Cell& cell, bool allowed, uint32_t humanKey, int s, juce::MidiBuffer& out)
{
    auto& st = state[(size_t) t];
    const auto& tr = song.tracks[(size_t) t];
    auto& voice = voices[(size_t) t];

    if (cell.instrument != kNone)
        st.instrument = cell.instrument;

    if (cell.note == kNoteOff)
    {
        releaseNote (song, t, s, out);
    }
    else if (cell.hasNote() && allowed && song.trackAudible (t))
    {
        float vel = cell.volume != kNone ? (float) cell.volume / 127.0f : 100.0f / 127.0f;
        if (tr.humanize > 0)
        {
            const float u = hashToUnit (hashMix (humanKey, 0xABCDu));
            vel += (u * 2.0f - 1.0f) * (float) tr.humanize / 127.0f;
        }
        vel = std::clamp (vel, 0.02f, 1.0f);
        playNote (song, t, cell.note, vel, cell.fxCmd == 'G', s, out);
    }
    else if (! cell.hasNote() && cell.volume != kNone && st.note >= 0)
    {
        const float v = (float) cell.volume / 127.0f;
        voice.setVolume (st.velocity > 0.01f ? std::min (1.0f, v / st.velocity) : v);
        if (tr.output == OutputKind::Midi && st.midiNote >= 0)
            out.addEvent (juce::MidiMessage::aftertouchChange (st.midiChannel, st.midiNote, (int) cell.volume), s);
    }

    switch (cell.fxCmd)
    {
        case 'C':
            st.cutTick = cell.fxVal;
            if (cell.fxVal == 0)
                releaseNote (song, t, s, out);
            break;
        case 'P':
            voice.setPan (std::clamp (((float) cell.fxVal - 128.0f) / 127.0f, -1.0f, 1.0f));
            break;
        case 'V':
            voice.setVibrato (0.5f + (float) (cell.fxVal >> 4) * 0.75f, (float) (cell.fxVal & 0x0F) / 8.0f);
            break;
        default:
            break;
    }
}

void Sequencer::tickEffects (const Song& song, int t, int tick, int s, juce::MidiBuffer& out)
{
    auto& st = state[(size_t) t];

    if (st.delayTick >= 0 && tick == st.delayTick)
    {
        st.delayTick = -1;
        applyCell (song, t, st.delayed, st.delayedAllowed, st.delayedKey, s, out);
    }
    if (st.cutTick > 0 && tick == st.cutTick)
        releaseNote (song, t, s, out);

    if (st.note < 0)
        return;

    const auto& tr = song.tracks[(size_t) t];
    if (st.fxCmd == 'A')
    {
        const int offs[3] = { 0, st.fxVal >> 4, st.fxVal & 0x0F };
        const int o = offs[tick % 3];
        if (o != st.arpOffset)
        {
            st.arpOffset = o;
            if (tr.output == OutputKind::Internal)
            {
                voices[(size_t) t].setPitchOffset ((float) o);
            }
            else
            {
                const int newNote = std::clamp (st.note + o, 0, 127);
                if (st.midiNote >= 0)
                    out.addEvent (juce::MidiMessage::noteOff (st.midiChannel, st.midiNote), s);
                out.addEvent (juce::MidiMessage::noteOn (st.midiChannel, newNote, (juce::uint8) std::clamp ((int) (st.velocity * 127.0f), 1, 127)), s);
                st.midiNote = newNote;
            }
        }
    }
    else if (st.fxCmd == 'R')
    {
        const int r = st.fxVal & 0x0F ? st.fxVal & 0x0F : st.fxVal >> 4;
        if (r > 0 && tick > 0 && tick % r == 0)
            playNote (song, t, st.note, st.velocity, false, s, out);
    }
}

void Sequencer::playNote (const Song& song, int t, int note, float vel, bool legato, int s, juce::MidiBuffer& out)
{
    auto& st = state[(size_t) t];
    const auto& tr = song.tracks[(size_t) t];
    note = std::clamp (note, 0, 127);

    if (tr.output == OutputKind::Internal)
    {
        if (st.midiNote >= 0) // output was switched while a MIDI note was held
        {
            out.addEvent (juce::MidiMessage::noteOff (st.midiChannel, st.midiNote), s);
            st.midiNote = -1;
        }
        const int ii = std::clamp (st.instrument >= 0 ? st.instrument : tr.instrument, 0, kNumInstruments - 1);
        voices[(size_t) t].noteOn (note, vel, song.instruments[(size_t) ii], legato);
        voices[(size_t) t].setPan (tr.pan);
    }
    else
    {
        voices[(size_t) t].noteOff();
        const int ch = std::clamp (tr.midiChannel, 1, 16);
        const int prev = st.midiNote, prevCh = st.midiChannel;
        const auto velocity = (juce::uint8) std::clamp ((int) std::lround (vel * 127.0f), 1, 127);

        if (prev >= 0 && ! legato)
            out.addEvent (juce::MidiMessage::noteOff (prevCh, prev), s);
        out.addEvent (juce::MidiMessage::noteOn (ch, note, velocity), s);
        if (prev >= 0 && legato && ! (prev == note && prevCh == ch))
            out.addEvent (juce::MidiMessage::noteOff (prevCh, prev), s);

        st.midiNote = note;
        st.midiChannel = ch;
    }

    st.note = note;
    st.velocity = vel;
    st.arpOffset = 0;
}

void Sequencer::releaseNote (const Song&, int t, int s, juce::MidiBuffer& out)
{
    auto& st = state[(size_t) t];
    voices[(size_t) t].noteOff();
    if (st.midiNote >= 0)
        out.addEvent (juce::MidiMessage::noteOff (st.midiChannel, st.midiNote), s);
    st.midiNote = -1;
    st.note = -1;
}

void Sequencer::allNotesOff (const Song& song, int s, juce::MidiBuffer& out)
{
    for (int t = 0; t < kNumTracks; ++t)
    {
        releaseNote (song, t, s, out);
        auto& st = state[(size_t) t];
        st.fxCmd = 0;
        st.cutTick = st.delayTick = -1;
        st.arpOffset = 0;
        voices[(size_t) t].setPitchOffset (0.0f);
        voices[(size_t) t].setVibrato (0.0f, 0.0f);
    }
}

} // namespace lattice
