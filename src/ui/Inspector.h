#pragma once

#include "ui/Context.h"

namespace lattice
{

/** One labelled slider row: label · bar · value. */
class ParamRow : public juce::Component
{
public:
    ParamRow (juce::String label, double min, double max, double step, std::function<juce::String (double)> format);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setValue (double v) { slider.setValue (v, juce::dontSendNotification); repaint(); }
    void setRange (double min, double max) { slider.setRange (min, max, step); }

    std::function<void (double)> onChange;
    juce::Slider slider;

private:
    juce::String label;
    double step;
    std::function<juce::String (double)> format;
};

class EuclidView;
class WaveView;

class Inspector : public Panel, private juce::Timer
{
public:
    explicit Inspector (Context&);
    ~Inspector() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> tabRect (int i) const;
    void showTab (int tab);
    void editInstrument (const std::function<void (Instrument&)>& fn, const std::string& key);
    void writeEuclid();

    // instrument tab
    juce::TextButton prevIns { "<" }, nextIns { ">" };
    juce::TextButton waveButtons[4];
    std::unique_ptr<WaveView> waveView;
    std::vector<std::unique_ptr<ParamRow>> insRows;

    // generative tab
    std::unique_ptr<EuclidView> euclidView;
    std::vector<std::unique_ptr<ParamRow>> genRows;
    juce::ToggleButton varyToggle { juce::String (juce::CharPointer_UTF8 ("Varier \xc3\xa0 chaque boucle")) };
    juce::TextButton writeButton { juce::String (juce::CharPointer_UTF8 ("\xc3\x89" "crire dans le pattern")) };
    juce::TextButton reseedButton { "Graine" };

    int lastPlayStep = -1;
};

} // namespace lattice
