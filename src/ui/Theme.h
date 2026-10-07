#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lattice::theme
{

// ---- Graphite palette (from the validated mock-up "B") -------------------------------------
inline const juce::Colour bg        { 0xFF0F1113 };
inline const juce::Colour panel     { 0xFF15181B };
inline const juce::Colour raised    { 0xFF1F2327 };
inline const juce::Colour line      { 0xFF22262B };
inline const juce::Colour text      { 0xFFE4E7EA };
inline const juce::Colour dim       { 0xFF8A9299 };
inline const juce::Colour faint     { 0xFF6C747B };
inline const juce::Colour empty     { 0xFF353B41 };
inline const juce::Colour ghost     { 0xFF4F575E };
inline const juce::Colour beatRow   { 0xFF191C20 };
inline const juce::Colour playRow   { 0xFF26323C };
inline const juce::Colour selection { 0xFF22374A };
inline const juce::Colour accent    { 0xFF5FD4C4 }; // teal: active, generative
inline const juce::Colour info      { 0xFF8FD3FF }; // fx column, MIDI badges
inline const juce::Colour prob      { 0xFFE8C268 };
inline const juce::Colour insCol    { 0xFF9AA3AB };
inline const juce::Colour volCol    { 0xFFC9CED3 };
inline const juce::Colour danger    { 0xFFFF6B6B };

juce::Colour trackColour (int track);

/** UTF-8 literal to juce::String (plain const char* literals are treated as ASCII by JUCE). */
inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

// ---- type -----------------------------------------------------------------------------------
juce::Font mono (float size, bool bold = false);
juce::Font sans (float size, int weight = 400); // 400, 600, 700

float textWidth (const juce::Font& f, const juce::String& s);

// ---- drawing helpers ------------------------------------------------------------------------
void fillPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 8.0f);
void drawPill (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fillColour);

/** The app wide look: sliders, buttons, popups, tooltips in Graphite. */
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
};

} // namespace lattice::theme
