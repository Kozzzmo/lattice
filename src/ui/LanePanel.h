#pragma once

#include "ui/Context.h"

namespace lattice
{

/** A drawable curve over the volume column of the cursor track (one undo step per stroke). */
class LanePanel : public Panel, private juce::Timer
{
public:
    explicit LanePanel (Context&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> plotArea() const;
    void drawAt (juce::Point<float>, bool erase);

    int strokeId = 0;
    int lastRow = -1, lastPlay = -1;
};

} // namespace lattice
