#include "core/Document.h"
#include "core/Generators.h"
#include "core/Sequencer.h"
#include "core/Serializer.h"

#include <cstdio>
#include <vector>

using namespace lattice;

static int failures = 0, checks = 0;
#define CHECK(cond) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

struct NoteEvent { int sample, channel, note; bool on; };

static std::vector<NoteEvent> run (Sequencer& seq, const Song& song, TransportInfo tr, int blocks, int blockSize, std::vector<float>* peaks = nullptr)
{
    std::vector<NoteEvent> events;
    juce::AudioBuffer<float> audio (2, blockSize);
    juce::MidiBuffer in, out;
    const double ppqPerSample = tr.bpm / 60.0 / tr.sampleRate;
    for (int b = 0; b < blocks; ++b)
    {
        out.clear();
        tr.numSamples = blockSize;
        seq.process (song, tr, audio, in, out);
        for (const auto m : out)
            if (m.getMessage().isNoteOnOrOff())
                events.push_back ({ b * blockSize + m.samplePosition, m.getMessage().getChannel(), m.getMessage().getNoteNumber(), m.getMessage().isNoteOn() });
        if (peaks) peaks->push_back (audio.getMagnitude (0, blockSize));
        tr.ppq += blockSize * ppqPerSample;
    }
    return events;
}

static Song makeMidiSong()
{
    Song s = *Song::createDefault();
    auto p = std::make_shared<Pattern>();
    p->numRows = 16;
    p->at (0, 0).note = 60;
    p->at (0, 4).note = 64;
    p->at (0, 8).note = kNoteOff;
    s.patterns = { p };
    s.order = { 0 };
    s.selectedOrder = 0;
    for (auto& t : s.tracks) { t.output = OutputKind::Midi; t.length = 0; t.humanize = 0; }
    return s;
}

int main()
{
    // ---- text helpers ------------------------------------------------------------------------
    CHECK (noteToString (60) == "C-4");
    CHECK (noteToString (49) == "C#3");
    CHECK (noteToString (kNoteOff) == "OFF");
    CHECK (hex2 (0x7f) == "7F");

    // ---- keyboard ------------------------------------------------------------------------------
    CHECK (pianoKeyOffset (U'w', KeyLayout::Azerty) == 0);
    CHECK (pianoKeyOffset (U'a', KeyLayout::Azerty) == 12);
    CHECK (pianoKeyOffset (U'é', KeyLayout::Azerty) == 13);
    CHECK (pianoKeyOffset (U'z', KeyLayout::Qwerty) == 0);
    CHECK (pianoKeyOffset (U'q', KeyLayout::Qwerty) == 12);
    CHECK (hexDigitForKey (U'é', KeyLayout::Azerty) == 2);
    CHECK (hexDigitForKey (U'à', KeyLayout::Azerty) == 0);
    CHECK (hexDigitForKey (U'c', KeyLayout::Azerty) == 12);
    CHECK (isNoteOffKey (U'<', KeyLayout::Azerty));

    // ---- euclid ----------------------------------------------------------------------------------
    {
        auto r = euclideanRhythm (8, 3, 0);
        int hits = 0; for (bool b : r) hits += b;
        CHECK (hits == 3);
        CHECK (r[0]);
        auto rr = euclideanRhythm (8, 3, 1);
        CHECK (rr[1] && ! rr[0]);
        auto r7 = euclideanRhythm (16, 7, 2);
        hits = 0; for (bool b : r7) hits += b;
        CHECK (hits == 7);
    }

    // ---- locate --------------------------------------------------------------------------------
    {
        Song s = *Song::createDefault();      // patterns 32 rows, order {0,1,1}
        Sequencer::Position pos;
        CHECK (Sequencer::locate (s, 0, 1, false, pos) && pos.rowInPattern == 0);
        CHECK (Sequencer::locate (s, 6 * 33, 1, false, pos) && pos.rowInPattern == 1 && pos.loopCount == 1);
        CHECK (Sequencer::locate (s, 6 * 40, 0, true, pos) && pos.order == 1 && pos.rowInPattern == 8);
        CHECK (Sequencer::locate (s, 6 * 96 + 3, 0, true, pos) && pos.order == 0 && pos.tickInRow == 3 && pos.loopCount == 1);
    }

    // ---- probability: deterministic, and roughly right on average -----------------------------
    {
        Song s = *Song::createDefault();
        Cell c; c.note = 60; c.prob = 30;
        int played = 0;
        for (int r = 0; r < 2000; ++r)
            played += Sequencer::shouldPlay (s, c, 0, r, 3, 0);
        CHECK (played > 450 && played < 750);
        CHECK (Sequencer::shouldPlay (s, c, 0, 5, 1, 0) == Sequencer::shouldPlay (s, c, 0, 5, 1, 9)); // fixed seed
        s.varyEachLoop = true;
        int differs = 0;
        for (int r = 0; r < 64; ++r)
            differs += Sequencer::shouldPlay (s, c, 0, r, 1, 0) != Sequencer::shouldPlay (s, c, 0, r, 1, 1);
        CHECK (differs > 5);
    }

    // ---- sequencer timing: MIDI notes land on the exact sample ---------------------------------
    {
        Song s = makeMidiSong();
        Sequencer seq;
        seq.prepare (48000.0, 512);
        TransportInfo tr; tr.hostPlaying = true; tr.bpm = 120.0; tr.sampleRate = 48000.0;
        auto ev = run (seq, s, tr, 100, 512); // 51200 samples
        // one row = 1/4 beat = 6000 samples at 120 BPM
        std::vector<NoteEvent> ch1;
        for (auto& e : ev) if (e.channel == 1) ch1.push_back (e);
        CHECK (ch1.size() >= 4);
        if (ch1.size() >= 4)
        {
            CHECK (ch1[0].on && ch1[0].note == 60 && ch1[0].sample == 0);
            CHECK (! ch1[1].on && ch1[1].note == 60 && ch1[1].sample == 24000);
            CHECK (ch1[2].on && ch1[2].note == 64 && ch1[2].sample == 24000);
            CHECK (! ch1[3].on && ch1[3].note == 64);
        }

        // starting mid-way (Ableton loop at beat 1) must hit row 4 exactly at its start
        Sequencer seq2; seq2.prepare (48000.0, 512);
        TransportInfo tr2 = tr; tr2.ppq = 1.0;
        auto ev2 = run (seq2, s, tr2, 4, 512);
        CHECK (! ev2.empty() && ev2[0].note == 64 && ev2[0].sample == 0);
    }

    // ---- polyrhythm: a 3 row track inside a 16 row pattern ---------------------------------------
    {
        Song s = makeMidiSong();
        auto p = std::make_shared<Pattern> (*s.patterns[0]);
        p->at (1, 0).note = 72;
        s.patterns[0] = p;
        s.tracks[1].length = 3;
        Sequencer seq; seq.prepare (48000.0, 500);
        TransportInfo tr; tr.hostPlaying = true; tr.bpm = 120.0; tr.sampleRate = 48000.0;
        auto ev = run (seq, s, tr, 96, 500);   // 48000 samples = 8 rows
        std::vector<int> onsets;
        for (auto& e : ev) if (e.channel == 2 && e.on) onsets.push_back (e.sample);
        CHECK (onsets.size() == 3);
        if (onsets.size() == 3)
            CHECK (onsets[0] == 0 && onsets[1] == 18000 && onsets[2] == 36000);
    }

    // ---- internal synth makes sound and stops ----------------------------------------------------
    {
        Song s = *Song::createDefault();
        Sequencer seq; seq.prepare (44100.0, 256);
        TransportInfo tr; tr.hostPlaying = true; tr.bpm = 124.0; tr.sampleRate = 44100.0;
        std::vector<float> peaks;
        run (seq, s, tr, 200, 256, &peaks);
        float mx = 0; for (float p : peaks) mx = std::max (mx, p);
        CHECK (mx > 0.05f && mx <= 1.0f);
        tr.hostPlaying = false;
        std::vector<float> after;
        run (seq, s, tr, 400, 256, &after);
        CHECK (after.back() < 1.0e-4f);
    }

    // ---- trigger mode: a MIDI note starts an order entry on the free clock -----------------------
    {
        Song s = makeMidiSong();
        s.mode = PlayMode::Trigger;
        Sequencer seq; seq.prepare (48000.0, 512);
        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer in, out;
        in.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 100);
        TransportInfo tr; tr.hostPlaying = false; tr.bpm = 120.0; tr.sampleRate = 48000.0; tr.numSamples = 512;
        seq.process (s, tr, audio, in, out);
        bool found = false;
        for (const auto m : out)
            if (m.getMessage().isNoteOn() && m.getMessage().getNoteNumber() == 60)
                found = std::abs (m.samplePosition - 100) <= 1;
        CHECK (found);
    }

    // ---- serialisation round trip ------------------------------------------------------------------
    {
        auto s = Song::createDefault();
        s->tracks[4].output = OutputKind::Midi;
        s->tracks[4].midiChannel = 7;
        s->instruments[3].crushBits = 5;
        auto tree = songToTree (*s);
        auto xml = tree.toXmlString();
        auto back = songFromTree (juce::ValueTree::fromXml (xml));
        CHECK (back != nullptr);
        if (back)
        {
            CHECK (back->patterns.size() == s->patterns.size());
            CHECK (back->order == s->order);
            CHECK (back->tracks[4].output == OutputKind::Midi && back->tracks[4].midiChannel == 7);
            CHECK (back->tracks[2].length == 12);
            CHECK (back->instruments[3].crushBits == 5);
            CHECK (back->patterns[1]->at (4, 12) == s->patterns[1]->at (4, 12));
            CHECK (back->patterns[1]->at (4, 12).fxCmd == 'V');
            CHECK (back->euclid[2].hits == 7);
        }
        CHECK (songFromTree (juce::ValueTree ("Other")) == nullptr);
    }

    // ---- document: copy on write and undo -------------------------------------------------------
    {
        Document d;
        auto before = d.song();
        d.modifyPattern (1, [] (Pattern& p) { p.at (0, 1).note = 61; });
        CHECK (d.song()->patterns[1]->at (0, 1).note == 61);
        CHECK (before->patterns[1]->at (0, 1).note == kNoteEmpty);
        CHECK (d.song()->patterns[0] == before->patterns[0]); // untouched pattern shared
        d.modify ([] (Song& s) { s.lpb = 8; }, "lpb");
        d.modify ([] (Song& s) { s.lpb = 6; }, "lpb");
        CHECK (d.undo() && d.song()->lpb == 4);
        CHECK (d.undo() && d.song()->patterns[1]->at (0, 1).note == kNoteEmpty);
        CHECK (d.redo() && d.song()->patterns[1]->at (0, 1).note == 61);
        CHECK (d.audioSnapshot() == d.song());
    }

    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
