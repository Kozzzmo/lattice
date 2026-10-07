#include "core/Serializer.h"

#include <algorithm>
#include <cstring>

namespace lattice
{

namespace
{
    const juce::Identifier idSong ("LatticeSong"), idVersion ("version"), idLpb ("lpb"), idTpl ("tpl"),
        idMode ("mode"), idSelected ("selected"), idSeed ("seed"), idVary ("vary"), idTrigBase ("triggerBase"),
        idPattern ("Pattern"), idName ("name"), idRows ("rows"), idCells ("cells"), idOrder ("order"),
        idTrack ("Track"), idOutput ("output"), idChannel ("channel"), idInstrument ("instrument"),
        idLength ("length"), idHumanize ("humanize"), idMute ("mute"), idSolo ("solo"), idPan ("pan"),
        idInst ("Instrument"), idWave ("wave"), idDuty ("duty"), idA ("a"), idD ("d"), idS ("s"), idR ("r"),
        idSweep ("sweep"), idSweepTime ("sweepTime"), idCrush ("crush"), idGlide ("glide"), idGain ("gain"),
        idEuclid ("euclid"), idTracks ("Tracks"), idInstruments ("Instruments"), idPatterns ("Patterns");

    juce::String joinInts (const std::vector<int>& v)
    {
        juce::StringArray a;
        for (int x : v) a.add (juce::String (x));
        return a.joinIntoString (",");
    }
}

juce::ValueTree songToTree (const Song& song)
{
    juce::ValueTree root (idSong);
    root.setProperty (idVersion, kStateVersion, nullptr);
    root.setProperty (idLpb, song.lpb, nullptr);
    root.setProperty (idTpl, song.tpl, nullptr);
    root.setProperty (idMode, (int) song.mode, nullptr);
    root.setProperty (idSelected, song.selectedOrder, nullptr);
    root.setProperty (idSeed, (juce::int64) song.seed, nullptr);
    root.setProperty (idVary, song.varyEachLoop, nullptr);
    root.setProperty (idTrigBase, song.triggerBaseNote, nullptr);
    root.setProperty (idOrder, joinInts (song.order), nullptr);

    juce::ValueTree pats (idPatterns);
    for (auto& p : song.patterns)
    {
        juce::ValueTree pt (idPattern);
        pt.setProperty (idName, juce::String (p->name), nullptr);
        pt.setProperty (idRows, p->numRows, nullptr);
        juce::MemoryBlock mb;
        for (int t = 0; t < kNumTracks; ++t)
            mb.append (p->cells[(size_t) t].data(), sizeof (Cell) * (size_t) p->numRows);
        pt.setProperty (idCells, mb.toBase64Encoding(), nullptr);
        pats.appendChild (pt, nullptr);
    }
    root.appendChild (pats, nullptr);

    juce::ValueTree tracks (idTracks);
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& tr = song.tracks[(size_t) t];
        auto& e = song.euclid[(size_t) t];
        juce::ValueTree tt (idTrack);
        tt.setProperty (idName, juce::String (tr.name), nullptr);
        tt.setProperty (idOutput, (int) tr.output, nullptr);
        tt.setProperty (idChannel, tr.midiChannel, nullptr);
        tt.setProperty (idInstrument, tr.instrument, nullptr);
        tt.setProperty (idLength, tr.length, nullptr);
        tt.setProperty (idHumanize, tr.humanize, nullptr);
        tt.setProperty (idMute, tr.mute, nullptr);
        tt.setProperty (idSolo, tr.solo, nullptr);
        tt.setProperty (idPan, tr.pan, nullptr);
        tt.setProperty (idEuclid, joinInts ({ e.steps, e.hits, e.rotation, e.prob }), nullptr);
        tracks.appendChild (tt, nullptr);
    }
    root.appendChild (tracks, nullptr);

    juce::ValueTree insts (idInstruments);
    for (auto& in : song.instruments)
    {
        juce::ValueTree it (idInst);
        it.setProperty (idName, juce::String (in.name), nullptr);
        it.setProperty (idWave, (int) in.wave, nullptr);
        it.setProperty (idDuty, in.duty, nullptr);
        it.setProperty (idA, in.attack, nullptr);
        it.setProperty (idD, in.decay, nullptr);
        it.setProperty (idS, in.sustain, nullptr);
        it.setProperty (idR, in.release, nullptr);
        it.setProperty (idSweep, in.sweep, nullptr);
        it.setProperty (idSweepTime, in.sweepTime, nullptr);
        it.setProperty (idCrush, in.crushBits, nullptr);
        it.setProperty (idGlide, in.glide, nullptr);
        it.setProperty (idGain, in.gain, nullptr);
        insts.appendChild (it, nullptr);
    }
    root.appendChild (insts, nullptr);
    return root;
}

std::shared_ptr<Song> songFromTree (const juce::ValueTree& root)
{
    if (! root.hasType (idSong))
        return nullptr;

    auto song = std::make_shared<Song>();
    song->lpb = std::clamp ((int) root.getProperty (idLpb, 4), 1, 32);
    song->tpl = std::clamp ((int) root.getProperty (idTpl, 6), 1, 32);
    song->mode = (PlayMode) std::clamp ((int) root.getProperty (idMode, 1), 0, 2);
    song->seed = (uint32_t) (juce::int64) root.getProperty (idSeed, 0x3F);
    song->varyEachLoop = (bool) root.getProperty (idVary, false);
    song->triggerBaseNote = std::clamp ((int) root.getProperty (idTrigBase, 48), 0, 127);

    for (auto pt : root.getChildWithName (idPatterns))
    {
        auto p = std::make_shared<Pattern>();
        p->name = pt.getProperty (idName, "Pattern").toString().toStdString();
        p->numRows = std::clamp ((int) pt.getProperty (idRows, kDefaultRows), 1, kMaxRows);
        juce::MemoryBlock mb;
        mb.fromBase64Encoding (pt.getProperty (idCells).toString());
        const size_t perTrack = sizeof (Cell) * (size_t) p->numRows;
        for (int t = 0; t < kNumTracks; ++t)
            if (mb.getSize() >= perTrack * (size_t) (t + 1))
                std::memcpy (p->cells[(size_t) t].data(), static_cast<const char*> (mb.getData()) + perTrack * (size_t) t, perTrack);
        song->patterns.push_back (p);
        if ((int) song->patterns.size() >= kMaxPatterns)
            break;
    }
    if (song->patterns.empty())
        song->patterns.push_back (std::make_shared<Pattern>());

    for (auto& s : juce::StringArray::fromTokens (root.getProperty (idOrder).toString(), ",", ""))
        if (s.isNotEmpty())
            song->order.push_back (std::clamp (s.getIntValue(), 0, (int) song->patterns.size() - 1));
    if (song->order.empty())
        song->order.push_back (0);
    song->selectedOrder = std::clamp ((int) root.getProperty (idSelected, 0), 0, song->numOrders() - 1);

    int t = 0;
    for (auto tt : root.getChildWithName (idTracks))
    {
        if (t >= kNumTracks) break;
        auto& tr = song->tracks[(size_t) t];
        tr.name = tt.getProperty (idName, "Track").toString().toStdString();
        tr.output = (OutputKind) std::clamp ((int) tt.getProperty (idOutput, 0), 0, 1);
        tr.midiChannel = std::clamp ((int) tt.getProperty (idChannel, 1), 1, 16);
        tr.instrument = std::clamp ((int) tt.getProperty (idInstrument, t), 0, kNumInstruments - 1);
        tr.length = std::clamp ((int) tt.getProperty (idLength, 0), 0, kMaxRows);
        tr.humanize = std::clamp ((int) tt.getProperty (idHumanize, 0), 0, 64);
        tr.mute = tt.getProperty (idMute, false);
        tr.solo = tt.getProperty (idSolo, false);
        tr.pan = (float) tt.getProperty (idPan, 0.0f);
        auto e = juce::StringArray::fromTokens (tt.getProperty (idEuclid).toString(), ",", "");
        if (e.size() == 4)
            song->euclid[(size_t) t] = { e[0].getIntValue(), e[1].getIntValue(), e[2].getIntValue(), e[3].getIntValue() };
        ++t;
    }

    int i = 0;
    for (auto it : root.getChildWithName (idInstruments))
    {
        if (i >= kNumInstruments) break;
        auto& in = song->instruments[(size_t) i++];
        in.name = it.getProperty (idName, "Init").toString().toStdString();
        in.wave = (Waveform) std::clamp ((int) it.getProperty (idWave, 0), 0, 3);
        in.duty = (float) it.getProperty (idDuty, 0.5f);
        in.attack = (float) it.getProperty (idA, 0.002f);
        in.decay = (float) it.getProperty (idD, 0.15f);
        in.sustain = (float) it.getProperty (idS, 0.6f);
        in.release = (float) it.getProperty (idR, 0.12f);
        in.sweep = (float) it.getProperty (idSweep, 0.0f);
        in.sweepTime = (float) it.getProperty (idSweepTime, 0.05f);
        in.crushBits = std::clamp ((int) it.getProperty (idCrush, 16), 2, 16);
        in.glide = (float) it.getProperty (idGlide, 0.0f);
        in.gain = (float) it.getProperty (idGain, 0.8f);
    }
    return song;
}

} // namespace lattice
