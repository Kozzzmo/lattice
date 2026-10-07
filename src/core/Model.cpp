#include "core/Model.h"

#include <cstdio>

namespace lattice
{

const Pattern* Song::patternForOrder (int orderIndex) const noexcept
{
    if (orderIndex < 0 || orderIndex >= (int) order.size())
        return nullptr;
    const int p = order[(size_t) orderIndex];
    if (p < 0 || p >= (int) patterns.size())
        return nullptr;
    return patterns[(size_t) p].get();
}

bool Song::anySolo() const noexcept
{
    for (auto& t : tracks)
        if (t.solo)
            return true;
    return false;
}

bool Song::trackAudible (int t) const noexcept
{
    const auto& tr = tracks[(size_t) t];
    if (tr.mute)
        return false;
    return ! anySolo() || tr.solo;
}

int Song::trackLength (int t, const Pattern& p) const noexcept
{
    const int len = tracks[(size_t) t].length;
    if (len <= 0 || len > p.numRows)
        return p.numRows;
    return len;
}

std::string noteToString (uint8_t note)
{
    if (note == kNoteEmpty) return "---";
    if (note == kNoteOff)   return "OFF";
    static const char* names[] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
    const int octave = note / 12 - 1;
    if (octave < 0) return "???";
    return std::string (names[note % 12]) + std::to_string (octave);
}

std::string hex2 (int v)
{
    char buf[4];
    std::snprintf (buf, sizeof (buf), "%02X", v & 0xFF);
    return buf;
}

uint32_t hashMix (uint32_t a, uint32_t b) noexcept
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 16; h *= 0x85EBCA6Bu;
    h ^= h >> 13; h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

float hashToUnit (uint32_t h) noexcept
{
    return (float) (h >> 8) * (1.0f / 16777216.0f);
}

namespace
{
    uint8_t n (const char* s) // "C-3" -> midi
    {
        static const char* names[] = { "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-" };
        for (int i = 0; i < 12; ++i)
            if (s[0] == names[i][0] && s[1] == names[i][1])
                return (uint8_t) ((s[2] - '0' + 1) * 12 + i);
        return kNoteEmpty;
    }

    void put (Pattern& p, int track, int row, uint8_t note, int ins, int vol = -1, int prob = -1, char fx = 0, int fxv = 0)
    {
        auto& c = p.at (track, row);
        c.note = note;
        c.instrument = ins < 0 ? kNone : (uint8_t) ins;
        c.volume = vol < 0 ? kNone : (uint8_t) vol;
        c.prob = prob < 0 ? kNone : (uint8_t) prob;
        c.fxCmd = (uint8_t) fx;
        c.fxVal = (uint8_t) fxv;
    }
}

std::shared_ptr<Song> Song::createDefault()
{
    auto song = std::make_shared<Song>();

    const char* trackNames[kNumTracks] = { "Kick", "Snare", "Hats", "Bass", "Lead", "Pad", "Perc", "Noise" };
    for (int t = 0; t < kNumTracks; ++t)
    {
        song->tracks[(size_t) t].name = trackNames[t];
        song->tracks[(size_t) t].instrument = t;
        song->tracks[(size_t) t].midiChannel = t + 1;
    }
    song->tracks[2].length = 12;
    song->tracks[2].humanize = 10;
    song->euclid[2] = { 16, 7, 2, 80 };

    auto& I = song->instruments;
    I[0] = { "Kick",  Waveform::Triangle, 0.5f, 0.001f, 0.22f, 0.0f, 0.08f, 30.0f, 0.045f, 16, 0.0f, 1.0f };
    I[1] = { "Snare", Waveform::Noise,    0.5f, 0.001f, 0.14f, 0.0f, 0.10f,  0.0f, 0.05f, 12, 0.0f, 0.6f };
    I[2] = { "Hats",  Waveform::Noise,    0.5f, 0.001f, 0.035f, 0.0f, 0.03f, 0.0f, 0.05f, 16, 0.0f, 0.35f };
    I[3] = { "Bass",  Waveform::Pulse,    0.25f, 0.002f, 0.18f, 0.55f, 0.06f, 0.0f, 0.05f, 6, 0.03f, 0.55f };
    I[4] = { "Lead",  Waveform::Saw,      0.5f, 0.004f, 0.3f, 0.5f, 0.2f, 0.0f, 0.05f, 16, 0.05f, 0.32f };
    I[5] = { "Pad",   Waveform::Triangle, 0.5f, 0.35f, 0.6f, 0.7f, 0.9f, 0.0f, 0.05f, 16, 0.0f, 0.4f };
    I[6] = { "Perc",  Waveform::Pulse,    0.125f, 0.001f, 0.09f, 0.0f, 0.05f, 12.0f, 0.02f, 16, 0.0f, 0.4f };
    I[7] = { "Noise", Waveform::Noise,    0.5f, 0.2f, 0.5f, 0.3f, 0.6f, 0.0f, 0.05f, 4, 0.0f, 0.25f };
    for (int i = 8; i < kNumInstruments; ++i)
        I[(size_t) i].name = "Init " + hex2 (i);

    // Pattern 00 : intro
    auto intro = std::make_shared<Pattern>();
    intro->name = "Intro";
    intro->numRows = 32;
    for (int r = 0; r < 32; r += 8) put (*intro, 0, r, n ("C-3"), 0, 0x6A);
    for (int r = 0; r < 32; r += 2) put (*intro, 2, r, n ("F#4"), 2, r % 4 == 0 ? 0x50 : 0x30, r % 4 == 2 ? 70 : -1);
    put (*intro, 5, 0, n ("A-3"), 5, 0x40, -1, 'A', 0x47);
    put (*intro, 5, 16, n ("F-3"), 5, 0x40, -1, 'A', 0x37);

    // Pattern 01 : the "Drop A" pattern shown in the mock-up
    auto drop = std::make_shared<Pattern>();
    drop->name = "Drop A";
    drop->numRows = 32;
    for (int r = 0; r < 32; ++r)
    {
        if (r % 4 == 0) put (*drop, 0, r, n ("C-3"), 0, 0x7F);
        if (r == 14 || r == 30) put (*drop, 0, r, n ("C-3"), 0, 0x48, 50);
        if (r % 8 == 4) put (*drop, 1, r, n ("D-3"), 1, 0x70, -1, r == 28 ? 'R' : 0, r == 28 ? 0x03 : 0);
        if (r == 15 || r == 31) put (*drop, 1, r, n ("D-3"), 1, 0x2C, 25);
        if (r % 2 == 0) put (*drop, 2, r, n ("F#4"), 2, r % 4 == 0 ? 0x5C : 0x34, r % 4 == 2 ? 80 : -1, r == 10 ? 'D' : 0, r == 10 ? 0x02 : 0);
    }
    const struct { int r; const char* note; char fx; int v; } bass[] = {
        { 0, "A-1", 0, 0 }, { 3, "A-1", 0, 0 }, { 6, "C-2", 0, 0 }, { 8, "A-1", 0, 0 }, { 11, "G-1", 0, 0 }, { 14, "E-2", 'G', 0x08 },
        { 16, "A-1", 0, 0 }, { 19, "A-1", 0, 0 }, { 22, "C-2", 0, 0 }, { 24, "D-2", 0, 0 }, { 27, "C-2", 0, 0 }, { 30, "G-1", 0, 0 } };
    for (auto& b : bass) put (*drop, 3, b.r, n (b.note), 3, 0x6A, -1, b.fx, b.v);
    put (*drop, 3, 2, kNoteOff, -1); put (*drop, 3, 18, kNoteOff, -1);
    put (*drop, 4, 0, n ("E-4"), 4, 0x50, -1, 'A', 0x37);
    put (*drop, 4, 6, n ("G-4"), 4, 0x50);
    put (*drop, 4, 10, kNoteOff, -1);
    put (*drop, 4, 12, n ("A-4"), 4, 0x50, -1, 'V', 0x46);
    put (*drop, 4, 20, n ("C-5"), 4, 0x50);
    put (*drop, 4, 24, kNoteOff, -1);
    put (*drop, 4, 26, n ("B-4"), 4, 0x50, 66);
    put (*drop, 5, 0, n ("A-3"), 5, 0x40, -1, 'A', 0x47);
    put (*drop, 5, 16, n ("F-3"), 5, 0x40, -1, 'A', 0x37);
    put (*drop, 5, 31, kNoteOff, -1);

    song->patterns = { intro, drop };
    song->order = { 0, 1, 1 };
    song->selectedOrder = 1;
    return song;
}

} // namespace lattice
