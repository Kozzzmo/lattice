#include "core/Generators.h"

#include <algorithm>

namespace lattice
{

std::vector<bool> euclideanRhythm (int steps, int hits, int rotation)
{
    steps = std::clamp (steps, 1, kMaxRows);
    hits = std::clamp (hits, 0, steps);
    std::vector<bool> base ((size_t) steps), out ((size_t) steps);
    for (int i = 0; i < steps; ++i)
        base[(size_t) i] = (i * hits) % steps < hits;
    rotation = ((rotation % steps) + steps) % steps;
    for (int i = 0; i < steps; ++i)
        out[(size_t) ((i + rotation) % steps)] = base[(size_t) i];
    return out;
}

void writeEuclid (Pattern& p, int track, int length, const EuclidSettings& e,
                  uint8_t note, uint8_t instrument, uint8_t volume)
{
    if (track < 0 || track >= kNumTracks)
        return;
    length = std::clamp (length, 1, p.numRows);
    const auto rhythm = euclideanRhythm (e.steps, e.hits, e.rotation);
    for (int r = 0; r < length; ++r)
    {
        Cell c;
        if (rhythm[(size_t) (r % (int) rhythm.size())])
        {
            c.note = note;
            c.instrument = instrument;
            c.volume = volume;
            c.prob = e.prob >= 100 ? kNone : (uint8_t) std::clamp (e.prob, 0, 100);
        }
        p.at (track, r) = c;
    }
}

int densityLevel (const Pattern& p, int track)
{
    int count = 0;
    for (int r = 0; r < p.numRows; ++r)
        if (p.at (track, r).hasNote())
            ++count;
    if (count == 0) return 0;
    const float ratio = (float) count / (float) p.numRows;
    if (ratio < 0.12f) return 1;
    if (ratio < 0.3f)  return 2;
    return 3;
}

int pianoKeyOffset (char32_t c, KeyLayout layout)
{
    if (c >= U'A' && c <= U'Z')
        c = c - U'A' + U'a';

    // FastTracker layout: lower row = octave, upper row = octave + 1
    static const char32_t qwerty[] = U"zsxdcvgbhnjm,l.;/q2w3er5t6y7ui9o0p[=]";
    static const char32_t azerty[] = U"wsxdcvgbhnj,;l:m!aéz\"er(t-yèuiçoàp^=$";
    static const int offsets[]     = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
                                       12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31 };

    const char32_t* table = layout == KeyLayout::Azerty ? azerty : qwerty;
    for (int i = 0; table[i] != 0; ++i)
        if (table[i] == c)
            return offsets[i];
    return -1;
}

int hexDigitForKey (char32_t c, KeyLayout layout)
{
    if (c >= U'0' && c <= U'9') return (int) (c - U'0');
    if (c >= U'a' && c <= U'f') return (int) (c - U'a') + 10;
    if (c >= U'A' && c <= U'F') return (int) (c - U'A') + 10;
    if (layout == KeyLayout::Azerty)
    {
        static const char32_t row[] = U"à&é\"'(-è_ç"; // 0..9 without shift
        for (int i = 0; row[i] != 0; ++i)
            if (row[i] == c)
                return i;
    }
    return -1;
}

bool isNoteOffKey (char32_t c, KeyLayout layout)
{
    if (layout == KeyLayout::Azerty)
        return c == U'<' || c == U'>' || c == U'&' || c == U'1';
    return c == U'`' || c == U'\\' || c == U'1';
}

} // namespace lattice
