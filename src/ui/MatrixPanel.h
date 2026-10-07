#pragma once

#include "ui/Context.h"

namespace lattice
{

/** Left column: the order list as a coloured pattern matrix, plus the MIDI trigger map. */
class MatrixPanel : public Panel, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit MatrixPanel (Context&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> matrixCard() const;
    juce::Rectangle<float> triggerCard() const;
    int visibleEntries() const;
    juce::Rectangle<float> entryRow (int visibleIndex) const;
    juce::Rectangle<float> buttonRect (int i) const;
    void buttonAction (int i);
    void entryMenu (int entry);

    int scroll = 0;
    int lastPlaying = -1;
};

} // namespace lattice
