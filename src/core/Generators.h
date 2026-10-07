#pragma once

#include "core/Model.h"

#include <vector>

namespace lattice
{

/** Bjorklund-style even distribution of `hits` onsets over `steps`, rotated right. */
std::vector<bool> euclideanRhythm (int steps, int hits, int rotation);

/** Writes the euclidean rhythm into rows [0, length) of a track, clearing the rows in between. */
void writeEuclid (Pattern& p, int track, int length, const EuclidSettings& e,
                  uint8_t note, uint8_t instrument, uint8_t volume);

/** Number of note events per track, bucketed 0..3 for the matrix mini-map. */
int densityLevel (const Pattern& p, int track);

// ---- keyboard layouts ------------------------------------------------------------------

enum class KeyLayout { Azerty = 0, Qwerty };

/** Semitone offset (0..28) of a "piano" key relative to the current octave, or -1. */
int pianoKeyOffset (char32_t c, KeyLayout layout);

/** Hex digit for a key, accepting the unshifted AZERTY number row (&é"'(-è_çà). -1 if none. */
int hexDigitForKey (char32_t c, KeyLayout layout);

bool isNoteOffKey (char32_t c, KeyLayout layout);

} // namespace lattice
