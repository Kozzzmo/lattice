#include "plugin/PluginEditor.h"

namespace lattice
{

using namespace theme;

LatticeEditor::LatticeEditor (LatticeProcessor& p)
    : AudioProcessorEditor (p), proc (p), ctx { p, {} },
      topBar (ctx), matrix (ctx), grid (ctx), lane (ctx), inspector (ctx)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setLookAndFeel (&lnf);

    ctx.uiChanged = [this] { refreshAll(); };
    proc.doc.onChange = [safe = juce::Component::SafePointer<LatticeEditor> (this)]
    {
        if (safe != nullptr) safe->refreshAll();
    };

    for (juce::Component* c : { (juce::Component*) &topBar, (juce::Component*) &matrix, (juce::Component*) &grid,
                                (juce::Component*) &lane, (juce::Component*) &inspector })
        addAndMakeVisible (c);

    setResizable (true, true);
    setResizeLimits (1180, 720, 2600, 1600);
    setSize (1440, 880);
    setWantsKeyboardFocus (false);
    startTimerHz (10);

    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PatternGrid> (&grid)]
    {
        if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
    });
}

LatticeEditor::~LatticeEditor()
{
    proc.doc.onChange = nullptr;
    setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void LatticeEditor::timerCallback()
{
    const bool f = grid.hasKeyboardFocus (true);
    if (f != lastFocus)
    {
        lastFocus = f;
        repaint (footerArea());
    }
}

void LatticeEditor::refreshAll()
{
    topBar.refresh();
    matrix.refresh();
    grid.refresh();
    lane.refresh();
    inspector.refresh();
    repaint (footerArea());
}

juce::Rectangle<int> LatticeEditor::footerArea() const
{
    return getLocalBounds().removeFromBottom (28);
}

void LatticeEditor::resized()
{
    auto r = getLocalBounds();
    topBar.setBounds (r.removeFromTop (52));
    r.removeFromBottom (28);
    r.reduce (10, 10);

    matrix.setBounds (r.removeFromLeft (178));
    r.removeFromLeft (10);

    // grid card takes whole tracks; the inspector gets what is left (at least 290)
    const int gridW = juce::jlimit (PatternGrid::kNumW + PatternGrid::kTrackW * 3,
                                    PatternGrid::kNumW + PatternGrid::kTrackW * kNumTracks,
                                    PatternGrid::kNumW + ((r.getWidth() - 300 - PatternGrid::kNumW) / PatternGrid::kTrackW) * PatternGrid::kTrackW) + 6;
    gridCard = r.removeFromLeft (gridW);
    r.removeFromLeft (10);
    inspectorCard = r;

    auto gc = gridCard;
    lane.setBounds (gc.removeFromBottom (juce::jlimit (90, 140, gc.getHeight() / 6)));
    grid.setBounds (gc);
    inspector.setBounds (inspectorCard);
}

juce::Rectangle<float> LatticeEditor::footerItem (int i) const
{
    auto f = footerArea().toFloat().withTrimmedLeft (18.0f);
    const float widths[] = { 112.0f, 76.0f, 58.0f, 100.0f, 96.0f };
    for (int k = 0; k < i; ++k) f.removeFromLeft (widths[k] + 22.0f);
    return f.withWidth (widths[i]);
}

void LatticeEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    fillPanel (g, gridCard.toFloat());
    fillPanel (g, inspectorCard.toFloat());

    // footer
    const auto& u = proc.ui;
    auto song = proc.doc.song();
    auto item = [&] (int i, const juce::String& label, const juce::String& value, juce::Colour vc)
    {
        auto r = footerItem (i);
        g.setFont (sans (11.0f));
        g.setColour (dim);
        g.drawText (label, r, juce::Justification::centredLeft);
        g.setFont (mono (11.0f, true));
        g.setColour (vc);
        g.drawText (value, r.withTrimmedLeft (textWidth (sans (11.0f), label) + 6.0f), juce::Justification::centredLeft);
    };
    item (0, "Clavier", u.layout == KeyLayout::Azerty ? "AZERTY" : "QWERTY", text);
    item (1, "Octave", juce::String (u.octave), text);
    item (2, "Pas", juce::String (u.editStep), text);
    item (3, "Focus", grid.hasKeyboardFocus (true) ? "Grille" : U ("—"), grid.hasKeyboardFocus (true) ? accent : faint);
    item (4, "Suivre", u.follow ? "oui" : "non", u.follow ? text : faint);

    const auto& tr = song->tracks[(size_t) u.track];
    const auto& c = ctx.pattern().at (u.track, juce::jmin (u.row, ctx.pattern().numRows - 1));
    juce::String right;
    right << hex2 (u.row) << U (" · ") << juce::String (tr.name) << U (" → ")
          << (tr.output == OutputKind::Internal ? juce::String ("Int ") + juce::String (hex2 (tr.instrument)) : "MIDI " + juce::String (tr.midiChannel));
    if (c.hasNote())
        right << U (" · ") << noteToString (c.note) << " " << (c.instrument == kNone ? ".." : juce::String (hex2 (c.instrument)))
              << " " << (c.volume == kNone ? ".." : juce::String (hex2 (c.volume)));
    g.setFont (mono (11.0f));
    g.setColour (dim);
    g.drawText (right, footerArea().withTrimmedRight (18), juce::Justification::centredRight);
}

void LatticeEditor::mouseDown (const juce::MouseEvent& e)
{
    auto& u = proc.ui;
    const auto p = e.position;
    if (footerItem (0).contains (p))
    {
        u.layout = u.layout == KeyLayout::Azerty ? KeyLayout::Qwerty : KeyLayout::Azerty;
        refreshAll();
    }
    else if (footerItem (1).contains (p) || footerItem (2).contains (p))
    {
        const bool oct = footerItem (1).contains (p);
        juce::PopupMenu m;
        m.addSectionHeader (oct ? "Octave (F1-F8)" : U ("Pas d'édition"));
        for (int v = 0; v <= (oct ? 8 : 16); ++v)
            m.addItem (v + 1, juce::String (v), true, (oct ? u.octave : u.editStep) == v);
        m.showMenuAsync ({}, [this, oct] (int r)
        {
            if (r <= 0) return;
            (oct ? proc.ui.octave : proc.ui.editStep) = r - 1;
            refreshAll();
            grid.grabKeyboardFocus();
        });
        return;
    }
    else if (footerItem (4).contains (p))
    {
        u.follow = ! u.follow;
        refreshAll();
    }
    grid.grabKeyboardFocus();
}

} // namespace lattice
