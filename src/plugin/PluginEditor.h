#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/Context.h"
#include "ui/Inspector.h"
#include "ui/LanePanel.h"
#include "ui/MatrixPanel.h"
#include "ui/PatternGrid.h"
#include "ui/TopBar.h"

namespace lattice
{

class LatticeEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LatticeEditor (LatticeProcessor&);
    ~LatticeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void refreshAll();
    juce::Rectangle<int> footerArea() const;
    juce::Rectangle<float> footerItem (int i) const;

    LatticeProcessor& proc;
    theme::LookAndFeel lnf;
    Context ctx;
    TopBar topBar;
    MatrixPanel matrix;
    PatternGrid grid;
    LanePanel lane;
    Inspector inspector;
    juce::TooltipWindow tooltips { this, 500 };

    juce::Rectangle<int> gridCard, inspectorCard;
    bool lastFocus = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatticeEditor)
};

} // namespace lattice
