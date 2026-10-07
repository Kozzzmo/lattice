#include "ui/LanePanel.h"
#include "ui/PatternGrid.h"

namespace lattice
{

using namespace theme;

LanePanel::LanePanel (Context& c) : Panel (c)
{
    startTimerHz (30);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void LanePanel::timerCallback()
{
    auto& ph = ctx.proc.seq.playhead;
    const int p = ph.active.load() && ph.order.load() == ctx.song()->selectedOrder ? ph.rows[ctx.ui().track].load() : -1;
    if (p != lastPlay) { lastPlay = p; repaint(); }
}

juce::Rectangle<float> LanePanel::plotArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (26.0f);
    r.removeFromBottom (8.0f);
    r.removeFromLeft ((float) PatternGrid::kNumW);
    r.removeFromRight (10.0f);
    return r;
}

void LanePanel::paint (juce::Graphics& g)
{
    auto song = ctx.song();
    const auto& pat = ctx.pattern();
    const int t = ctx.ui().track;
    const auto col = trackColour (t);

    g.setColour (line);
    g.fillRect (0, 0, getWidth(), 1);

    auto head = getLocalBounds().toFloat().withHeight (26.0f).withTrimmedLeft (10.0f).withTrimmedRight (10.0f);
    g.setFont (mono (11.0f, true));
    g.setColour (col);
    const juce::String name = song->tracks[(size_t) t].name;
    g.drawText (name, head, juce::Justification::centredLeft);
    head.removeFromLeft (textWidth (mono (11.0f, true), name) + 10.0f);
    g.setFont (mono (11.0f));
    g.setColour (dim);
    g.drawText (U ("Volume · colonne Vo"), head, juce::Justification::centredLeft);
    g.drawText (U ("glisser : dessiner · clic droit : effacer"), head, juce::Justification::centredRight);

    auto area = plotArea();
    const int n = pat.numRows;
    const float slot = area.getWidth() / (float) n;

    // beat grid
    for (int r = 0; r < n; r += juce::jmax (1, song->lpb))
    {
        g.setColour (r % (song->lpb * 4) == 0 ? line.brighter (0.15f) : line);
        g.fillRect (area.getX() + (float) r * slot, area.getY(), 1.0f, area.getHeight());
    }

    if (lastPlay >= 0)
    {
        g.setColour (playRow);
        g.fillRect (area.getX() + (float) lastPlay * slot, area.getY(), juce::jmax (2.0f, slot), area.getHeight());
    }

    juce::Path curve, fill;
    bool started = false;
    juce::Array<juce::Point<float>> dots;
    for (int r = 0; r < n; ++r)
    {
        const auto& c = pat.at (t, r);
        if (! c.hasNote() && c.volume == kNone) continue;
        const int v = c.volume != kNone ? c.volume : 100;
        const juce::Point<float> p (area.getX() + ((float) r + 0.5f) * slot, area.getBottom() - area.getHeight() * (float) v / 127.0f);
        if (! started) { curve.startNewSubPath (p); fill.startNewSubPath (p.x, area.getBottom()); fill.lineTo (p); started = true; }
        else { curve.lineTo (p); fill.lineTo (p); }
        dots.add (p);
    }
    if (started)
    {
        fill.lineTo (dots.getLast().x, area.getBottom());
        fill.closeSubPath();
        g.setColour (col.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (auto& d : dots)
        {
            g.setColour (panel);
            g.fillEllipse (d.x - 3.5f, d.y - 3.5f, 7.0f, 7.0f);
            g.setColour (col);
            g.drawEllipse (d.x - 3.5f, d.y - 3.5f, 7.0f, 7.0f, 2.0f);
        }
    }
    else
    {
        g.setColour (faint);
        g.setFont (sans (12.0f));
        g.drawText (U ("Aucune note sur cette piste — tape des notes dans la grille"), area, juce::Justification::centred);
    }
}

void LanePanel::drawAt (juce::Point<float> p, bool erase)
{
    auto area = plotArea();
    const int n = ctx.pattern().numRows;
    const int row = juce::jlimit (0, n - 1, (int) ((p.x - area.getX()) / (area.getWidth() / (float) n)));
    const int v = juce::jlimit (0, 127, juce::roundToInt ((area.getBottom() - p.y) / area.getHeight() * 127.0f));
    const int t = ctx.ui().track;

    // interpolate between drag samples so fast strokes leave no gaps
    const int from = lastRow < 0 ? row : lastRow;
    const int lo = juce::jmin (from, row), hi = juce::jmax (from, row);
    lastRow = row;

    ctx.editPattern ([=] (Pattern& pat)
    {
        for (int r = lo; r <= hi; ++r)
        {
            auto& c = pat.at (t, r);
            if (erase) c.volume = kNone;
            else if (c.hasNote() || r == row) c.volume = (uint8_t) v;
        }
    }, "lane" + std::to_string (strokeId));
}

void LanePanel::mouseDown (const juce::MouseEvent& e)
{
    ++strokeId;
    lastRow = -1;
    drawAt (e.position, e.mods.isPopupMenu());
}

void LanePanel::mouseDrag (const juce::MouseEvent& e)
{
    drawAt (e.position, e.mods.isPopupMenu());
}

} // namespace lattice
