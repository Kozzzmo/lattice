// Headless harness: plays the plugin offline, renders audio to WAV and the editor to PNG.
// usage: LatticeShot out.png [blocks] [out.wav] [tab]
#include "plugin/PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File png (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "shot.png"));
    const int blocks = argc > 2 ? std::atoi (argv[2]) : 200;
    const juce::String wavPath = argc > 3 ? juce::String (argv[3]) : juce::String();
    const int tab = argc > 4 ? std::atoi (argv[4]) : -1;

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    auto& lp = dynamic_cast<lattice::LatticeProcessor&> (*proc);
    const double sr = 48000.0;
    const int bs = 512;
    proc->setPlayConfigDetails (0, 2, sr, bs);
    proc->prepareToPlay (sr, bs);
    lp.seq.setPreviewPlaying (blocks > 0);
    if (tab >= 0) lp.ui.inspectorTab = tab;
    lp.ui.track = 2;
    lp.ui.row = 6;

    std::unique_ptr<juce::AudioFormatWriter> writer;
    if (wavPath.isNotEmpty())
    {
        juce::WavAudioFormat wav;
        auto* out = new juce::FileOutputStream (juce::File (wavPath));
        out->setPosition (0);
        out->truncate();
        writer.reset (wav.createWriterFor (out, sr, 2, 16, {}, 0));
    }

    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    double sumSq = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        proc->processBlock (buf, midi);
        peak = juce::jmax (peak, buf.getMagnitude (0, bs));
        sumSq += (double) buf.getRMSLevel (0, 0, bs) * buf.getRMSLevel (0, 0, bs);
        if (writer) writer->writeFromAudioSampleBuffer (buf, 0, bs);
    }
    writer.reset();
    if (blocks > 0)
        std::printf ("peak %.3f  rms %.3f  (%d blocks)\n", peak, std::sqrt (sumSq / blocks), blocks);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
    ed->setSize (1440, 880);
    for (int i = 0; i < 3; ++i) { juce::Timer::callPendingTimersSynchronously(); juce::Thread::sleep (40); }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    png.deleteFile();
    juce::FileOutputStream os (png);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", png.getFullPathName().toRawUTF8());
    ed.reset();
    return 0;
}
