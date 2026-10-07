#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"
#include "core/Serializer.h"

namespace lattice
{

LatticeProcessor::LatticeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool LatticeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void LatticeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    seq.prepare (sampleRate, samplesPerBlock);
    loadMeasurer.reset (sampleRate, samplesPerBlock);
    midiOut.ensureSize (4096);
}

void LatticeProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::AudioProcessLoadMeasurer::ScopedTimer timer (loadMeasurer, audio.getNumSamples());
    juce::ScopedNoDenormals noDenormals;

    TransportInfo tr;
    tr.sampleRate = getSampleRate();
    tr.numSamples = audio.getNumSamples();
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            tr.hostPlaying = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) tr.ppq = *ppq;
            if (auto bpm = pos->getBpm()) tr.bpm = *bpm;
        }
    }

    const auto song = doc.audioSnapshot();
    midiOut.clear();
    seq.process (*song, tr, audio, midi, midiOut);
    midi.swapWith (midiOut);
}

void LatticeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = songToTree (*doc.song());
    tree.setProperty ("uiOctave", ui.octave, nullptr);
    tree.setProperty ("uiStep", ui.editStep, nullptr);
    tree.setProperty ("uiLayout", (int) ui.layout, nullptr);
    tree.setProperty ("uiFollow", ui.follow, nullptr);
    tree.setProperty ("uiTab", ui.inspectorTab, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void LatticeProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    auto song = songFromTree (tree);
    if (song == nullptr)
        return;

    ui.octave = juce::jlimit (0, 8, (int) tree.getProperty ("uiOctave", 4));
    ui.editStep = juce::jlimit (0, 16, (int) tree.getProperty ("uiStep", 1));
    ui.layout = (KeyLayout) juce::jlimit (0, 1, (int) tree.getProperty ("uiLayout", 0));
    ui.follow = tree.getProperty ("uiFollow", true);
    ui.inspectorTab = juce::jlimit (0, 2, (int) tree.getProperty ("uiTab", 1));
    ui.track = ui.row = ui.field = 0;
    ui.hasSelection = false;

    auto apply = [this, song]() mutable { doc.replace (std::move (song)); };
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && ! juce::MessageManager::getInstance()->isThisTheMessageThread())
        juce::MessageManager::callAsync (apply);
    else
        apply();
}

juce::AudioProcessorEditor* LatticeProcessor::createEditor()
{
    return new LatticeEditor (*this);
}

} // namespace lattice

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new lattice::LatticeProcessor();
}
