#pragma once

#include "core/Document.h"
#include "core/Generators.h"
#include "core/Sequencer.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace lattice
{

/** Editor state that is not part of the song (but is remembered with the project). */
struct UiState
{
    int octave = 4;
    int editStep = 1;
    KeyLayout layout = KeyLayout::Azerty;
    bool follow = true;
    int inspectorTab = 1; // 0 instrument, 1 generative, 2 fx

    // cursor: field 0 note, 1-2 instrument, 3-4 volume, 5-7 probability, 8 fx command, 9-10 fx value
    int track = 0, row = 0, field = 0;

    bool hasSelection = false;
    int selTrack0 = 0, selRow0 = 0, selTrack1 = 0, selRow1 = 0;
};

struct Clipboard
{
    int tracks = 0, rows = 0;
    std::vector<Cell> cells; // track-major
};

class LatticeProcessor final : public juce::AudioProcessor
{
public:
    LatticeProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Lattice"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    Document doc;
    Sequencer seq;
    UiState ui;
    Clipboard clipboard;

    float cpuLoad() const noexcept { return (float) loadMeasurer.getLoadAsProportion(); }

private:
    juce::AudioProcessLoadMeasurer loadMeasurer;
    juce::MidiBuffer midiOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatticeProcessor)
};

} // namespace lattice
