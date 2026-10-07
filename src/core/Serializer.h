#pragma once

#include "core/Model.h"

#include <juce_data_structures/juce_data_structures.h>

namespace lattice
{

constexpr int kStateVersion = 1;

juce::ValueTree songToTree (const Song& song);

/** Returns nullptr if the tree is not a Lattice song. Missing fields fall back to defaults. */
std::shared_ptr<Song> songFromTree (const juce::ValueTree& tree);

} // namespace lattice
