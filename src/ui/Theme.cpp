#include "ui/Theme.h"

#include "LatticeAssets.h"

namespace lattice::theme
{

namespace
{
    struct Faces
    {
        juce::Typeface::Ptr mono, monoBold, sans, sansSemi, sansBold;

        Faces()
        {
            auto load = [] (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); };
            mono     = load (LatticeAssets::JetBrainsMono400_ttf, LatticeAssets::JetBrainsMono400_ttfSize);
            monoBold = load (LatticeAssets::JetBrainsMono700_ttf, LatticeAssets::JetBrainsMono700_ttfSize);
            sans     = load (LatticeAssets::Sora400_ttf, LatticeAssets::Sora400_ttfSize);
            sansSemi = load (LatticeAssets::Sora600_ttf, LatticeAssets::Sora600_ttfSize);
            sansBold = load (LatticeAssets::Sora700_ttf, LatticeAssets::Sora700_ttfSize);
        }
    };

    Faces& faces()
    {
        static Faces f;
        return f;
    }

    juce::Font make (const juce::Typeface::Ptr& tf, float size)
    {
        return juce::Font (juce::FontOptions (tf).withPointHeight (size));
    }
}

juce::Colour trackColour (int t)
{
    static const juce::uint32 cols[] = { 0xFFFF7A45, 0xFFF2C14E, 0xFF5FD4C4, 0xFF6F9BFF,
                                         0xFFC08BFF, 0xFFFF6FA8, 0xFF8BD86A, 0xFFD9B38C };
    return juce::Colour (cols[(size_t) (t & 7)]);
}

juce::Font mono (float size, bool bold) { return make (bold ? faces().monoBold : faces().mono, size); }

juce::Font sans (float size, int weight)
{
    auto& f = faces();
    return make (weight >= 700 ? f.sansBold : weight >= 600 ? f.sansSemi : f.sans, size);
}

float textWidth (const juce::Font& f, const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (f, s);
}

void fillPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (panel);
    g.fillRoundedRectangle (r, radius);
}

void drawPill (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    g.setColour (c);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
}

// ---------------------------------------------------------------------------------------------

LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::PopupMenu::backgroundColourId, raised);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, playRow);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, dim);
    setColour (juce::TextButton::buttonColourId, raised);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, bg);
    setColour (juce::TextEditor::backgroundColourId, bg);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, playRow);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::TooltipWindow::backgroundColourId, raised);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ScrollBar::thumbColourId, ghost);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    // Anything not explicitly styled (popup menus, tooltips, text editors) uses Sora.
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return f.isBold() ? faces().sansSemi : faces().sans;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto colour = s.findColour (juce::Slider::trackColourId);
    const float cy = (float) y + (float) h * 0.5f;
    const auto bar = juce::Rectangle<float> ((float) x, cy - 2.0f, (float) w, 4.0f);
    g.setColour (line);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (bar.withRight (pos), 2.0f);
    g.setColour (s.isMouseOverOrDragging() ? juce::Colours::white : text);
    g.fillEllipse (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat();
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (down) c = c.brighter (0.15f);
    else if (over) c = c.brighter (0.07f);
    g.setColour (c);
    g.fillRoundedRectangle (r, juce::jmin (8.0f, r.getHeight() * 0.5f));
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton& b, int)
{
    return sans (12.5f, b.getToggleState() ? 600 : 400);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    g.setFont (sans (12.5f));
    g.setColour (dim);
    g.drawText (b.getButtonText(), r.withTrimmedRight (44.0f), juce::Justification::centredLeft, true);

    auto sw = juce::Rectangle<float> (r.getRight() - 34.0f, r.getCentreY() - 9.0f, 34.0f, 18.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? accent : (over ? ghost : line));
    g.fillRoundedRectangle (sw, 9.0f);
    g.setColour (on ? bg : text);
    g.fillEllipse (on ? sw.getRight() - 16.0f : sw.getX() + 2.0f, sw.getY() + 2.0f, 14.0f, 14.0f);
}

juce::Font LookAndFeel::getPopupMenuFont() { return sans (13.0f); }

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (raised);
    g.setColour (line);
    g.drawRect (0, 0, w, h);
}

} // namespace lattice::theme
