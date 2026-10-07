#pragma once

#include "ui/Context.h"

namespace lattice
{

class TopBar : public Panel, private juce::Timer
{
public:
    explicit TopBar (Context&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void layout();
    void renamePattern();

    juce::Rectangle<float> logoR, syncR, modeR[3], patChipR, patNameR, previewR, timingR, posR, cpuR;
    std::unique_ptr<juce::TextEditor> nameEditor;
    juce::String lastPos;
};

} // namespace lattice
