#include "ui/TopBar.h"

namespace lattice
{

using namespace theme;

namespace { const char* modeNames[] = { "Song", "Loop", "Trigger" }; }

TopBar::TopBar (Context& c) : Panel (c)
{
    startTimerHz (20);
}

void TopBar::timerCallback()
{
    repaint();
}

void TopBar::layout()
{
    auto s = ctx.song();
    const float h = (float) getHeight();
    const float cy = h * 0.5f;
    float x = 18.0f;

    logoR = { x, cy - 12.0f, 22.0f + 10.0f + textWidth (sans (17.0f, 700), "Lattice"), 24.0f };
    x = logoR.getRight() + 18.0f;

    const bool host = ctx.proc.seq.playhead.hostPlaying.load();
    const juce::String bpm = juce::String (ctx.proc.seq.playhead.bpm.load(), 2);
    const float syncW = 12 + 8 + 10 + textWidth (sans (12.5f), host ? U ("Synchro hôte") : U ("Hôte à l'arrêt"))
                        + 10 + textWidth (mono (12.5f, true), bpm) + 6 + textWidth (sans (12.5f), "BPM") + 12;
    syncR = { x, cy - 14.0f, syncW, 28.0f };
    x = syncR.getRight() + 18.0f;

    float mx = x + 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float w = textWidth (sans (12.5f, 600), modeNames[i]) + 28.0f;
        modeR[i] = { mx, cy - 11.0f, w, 22.0f };
        mx += w + 2.0f;
    }
    x = mx + 1.0f + 18.0f;

    const auto& pat = ctx.pattern();
    patChipR = { x + textWidth (sans (12.5f), "Pattern") + 8.0f, cy - 10.0f, 30.0f, 20.0f };
    patNameR = { patChipR.getRight() + 8.0f, cy - 12.0f, juce::jmax (40.0f, textWidth (sans (12.5f, 600), pat.name) + 4.0f), 24.0f };
    previewR = { patNameR.getRight() + 16.0f, cy - 14.0f, 90.0f, 28.0f };

    float rx = (float) getWidth() - 18.0f;
    cpuR = { rx - 64.0f, cy - 10.0f, 64.0f, 20.0f };
    rx = cpuR.getX() - 14.0f;
    posR = { rx - 92.0f, cy - 12.0f, 92.0f, 24.0f };
    rx = posR.getX() - 14.0f;
    const juce::String timing = "LPB " + juce::String (s->lpb) + U (" · TPL ") + juce::String (s->tpl);
    const float tw = textWidth (mono (12.0f), timing);
    timingR = { rx - tw - 12.0f, cy - 12.0f, tw + 12.0f, 24.0f };
}

void TopBar::paint (juce::Graphics& g)
{
    layout();
    auto s = ctx.song();
    auto& ph = ctx.proc.seq.playhead;

    g.setColour (line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    // logo: four rounded squares in track colours
    {
        auto r = logoR.withWidth (22.0f);
        const juce::Colour cols[] = { trackColour (0), trackColour (2), trackColour (3), trackColour (4) };
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (cols[i]);
            g.fillRoundedRectangle (r.getX() + (float) (i % 2) * 11.0f, r.getY() + 1.0f + (float) (i / 2) * 11.0f, 9.0f, 9.0f, 2.0f);
        }
        g.setColour (text);
        g.setFont (sans (17.0f, 700));
        g.drawText ("Lattice", logoR.withTrimmedLeft (32.0f), juce::Justification::centredLeft);
    }

    // host sync pill
    {
        const bool host = ph.hostPlaying.load();
        drawPill (g, syncR, panel);
        auto r = syncR.reduced (12.0f, 0.0f);
        g.setColour (host ? accent : faint);
        g.fillEllipse (r.getX(), syncR.getCentreY() - 4.0f, 8.0f, 8.0f);
        r.removeFromLeft (18.0f);
        g.setFont (sans (12.5f));
        g.setColour (dim);
        const juce::String label = host ? U ("Synchro hôte") : U ("Hôte à l'arrêt");
        g.drawText (label, r, juce::Justification::centredLeft);
        r.removeFromLeft (textWidth (sans (12.5f), label) + 10.0f);
        const juce::String bpm = juce::String (ph.bpm.load(), 2);
        g.setColour (text);
        g.setFont (mono (12.5f, true));
        g.drawText (bpm, r, juce::Justification::centredLeft);
        r.removeFromLeft (textWidth (mono (12.5f, true), bpm) + 6.0f);
        g.setColour (dim);
        g.setFont (sans (12.5f));
        g.drawText ("BPM", r, juce::Justification::centredLeft);
    }

    // mode segmented control
    {
        auto whole = modeR[0].getUnion (modeR[2]).expanded (3.0f);
        drawPill (g, whole, panel);
        for (int i = 0; i < 3; ++i)
        {
            const bool on = (int) s->mode == i;
            if (on) drawPill (g, modeR[i], text);
            g.setColour (on ? bg : dim);
            g.setFont (sans (12.5f, on ? 600 : 400));
            g.drawText (modeNames[i], modeR[i], juce::Justification::centred);
        }
    }

    // pattern chip + name
    {
        g.setFont (sans (12.5f));
        g.setColour (dim);
        g.drawText ("Pattern", juce::Rectangle<float> (patChipR.getX() - 70.0f, patChipR.getY(), 62.0f, patChipR.getHeight()),
                    juce::Justification::centredRight);
        g.setColour (playRow);
        g.fillRoundedRectangle (patChipR, 4.0f);
        g.setColour (info);
        g.setFont (mono (12.5f, true));
        g.drawText (juce::String (hex2 (ctx.patternIndex())), patChipR, juce::Justification::centred);
        if (nameEditor == nullptr)
        {
            g.setColour (text);
            g.setFont (sans (12.5f, 600));
            g.drawText (ctx.pattern().name, patNameR, juce::Justification::centredLeft);
        }
    }

    // preview play button (internal clock when the host is stopped)
    {
        const bool playing = ctx.proc.seq.isPreviewPlaying();
        drawPill (g, previewR, playing ? accent : raised);
        auto icon = previewR.withWidth (28.0f).withTrimmedLeft (12.0f).withSizeKeepingCentre (10.0f, 10.0f);
        g.setColour (playing ? bg : text);
        if (playing)
            g.fillRect (icon);
        else
        {
            juce::Path p;
            p.addTriangle (icon.getX(), icon.getY(), icon.getX(), icon.getBottom(), icon.getRight(), icon.getCentreY());
            g.fillPath (p);
        }
        g.setFont (sans (12.5f, 600));
        g.drawText (playing ? "Stop" : U ("Aperçu"), previewR.withTrimmedLeft (30.0f), juce::Justification::centredLeft);
    }

    // timing, position, cpu
    g.setFont (mono (12.0f));
    g.setColour (dim);
    g.drawText ("LPB " + juce::String (s->lpb) + U (" · TPL ") + juce::String (s->tpl), timingR, juce::Justification::centred);

    juce::String pos;
    if (ph.hostPlaying.load())
    {
        const double ppq = juce::jmax (0.0, ph.ppq.load());
        const int bar = (int) (ppq / 4.0) + 1;
        const int beat = (int) std::fmod (ppq, 4.0) + 1;
        const int six = (int) std::fmod (ppq * 4.0, 4.0) + 1;
        pos << bar << "." << beat << "." << six;
    }
    else if (ph.active.load())
        pos << hex2 (ph.order.load()) << ":" << hex2 (ph.row.load());
    else
        pos = U ("—");
    g.setColour (text);
    g.setFont (mono (17.0f, true));
    g.drawText (pos, posR, juce::Justification::centredRight);

    g.setFont (mono (12.0f));
    g.setColour (dim);
    g.drawText ("CPU " + juce::String (juce::roundToInt (ctx.proc.cpuLoad() * 100.0f)) + "%", cpuR, juce::Justification::centredRight);
}

void TopBar::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const bool hot = modeR[0].getUnion (modeR[2]).contains (p) || previewR.contains (p) || timingR.contains (p)
                     || patNameR.contains (p);
    setMouseCursor (hot ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void TopBar::mouseDown (const juce::MouseEvent& e)
{
    layout();
    const auto p = e.position;

    for (int i = 0; i < 3; ++i)
        if (modeR[i].contains (p))
        {
            ctx.doc().modify ([i] (Song& s) { s.mode = (PlayMode) i; }, "mode");
            return;
        }

    if (previewR.contains (p))
    {
        ctx.proc.seq.setPreviewPlaying (! ctx.proc.seq.isPreviewPlaying());
        repaint();
        return;
    }

    if (timingR.contains (p))
    {
        auto s = ctx.song();
        juce::PopupMenu lpb, tpl, m;
        for (int v : { 1, 2, 3, 4, 6, 8, 12, 16 })
            lpb.addItem (100 + v, juce::String (v) + " lignes par temps", true, s->lpb == v);
        for (int v : { 1, 2, 3, 4, 6, 8, 12, 16, 24 })
            tpl.addItem (200 + v, juce::String (v) + " ticks par ligne", true, s->tpl == v);
        m.addSectionHeader (U ("Résolution"));
        m.addSubMenu ("LPB : " + juce::String (s->lpb), lpb);
        m.addSubMenu ("TPL : " + juce::String (s->tpl), tpl);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (timingR.toNearestInt())),
                         [this] (int r)
                         {
                             if (r >= 200)      ctx.doc().modify ([r] (Song& so) { so.tpl = r - 200; });
                             else if (r >= 100) ctx.doc().modify ([r] (Song& so) { so.lpb = r - 100; });
                         });
    }
}

void TopBar::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (patNameR.contains (e.position))
        renamePattern();
}

void TopBar::renamePattern()
{
    nameEditor = std::make_unique<juce::TextEditor>();
    nameEditor->setFont (sans (12.5f, 600));
    nameEditor->setText (ctx.pattern().name, false);
    nameEditor->setBounds (patNameR.withWidth (160.0f).toNearestInt());
    nameEditor->setJustification (juce::Justification::centredLeft);
    nameEditor->setIndents (6, 3);
    addAndMakeVisible (*nameEditor);
    nameEditor->selectAll();
    nameEditor->grabKeyboardFocus();

    auto done = std::make_shared<bool> (false);
    auto finish = [this, done] (bool commit)
    {
        if (nameEditor == nullptr || *done) return;
        *done = true;
        const auto name = nameEditor->getText().trim().substring (0, 24).toStdString();
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<TopBar> (this)] { if (safe) safe->nameEditor.reset(); if (safe) safe->repaint(); });
        if (commit && ! name.empty())
            ctx.editPattern ([name] (Pattern& p) { p.name = name; });
    };
    nameEditor->onReturnKey = [finish] { finish (true); };
    nameEditor->onEscapeKey = [finish] { finish (false); };
    nameEditor->onFocusLost = [finish] { finish (true); };
}

} // namespace lattice
