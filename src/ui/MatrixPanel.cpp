#include "ui/MatrixPanel.h"

namespace lattice
{

using namespace theme;

namespace
{
    constexpr float kRowH = 21.0f, kHeader = 34.0f, kFooter = 64.0f;
    const char* buttonLabels[] = { "+", "x2", "=", "-" };
    const char* buttonTips[] = { "Nouveau pattern vide après la sélection",
                                 "Dupliquer : copie indépendante du pattern",
                                 "Alias : rejouer le même pattern (les modifications se répercutent)",
                                 "Retirer cette entrée de la séquence" };
}

MatrixPanel::MatrixPanel (Context& c) : Panel (c)
{
    startTimerHz (15);
}

void MatrixPanel::timerCallback()
{
    const int p = ctx.proc.seq.playhead.active.load() ? ctx.proc.seq.playhead.order.load() : -1;
    if (p != lastPlaying)
    {
        lastPlaying = p;
        repaint();
    }
}

juce::Rectangle<float> MatrixPanel::matrixCard() const
{
    auto r = getLocalBounds().toFloat();
    const float h = juce::jmin (r.getHeight() * 0.68f, kHeader + kFooter + kRowH * 18.0f);
    return r.removeFromTop (h);
}

juce::Rectangle<float> MatrixPanel::triggerCard() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (matrixCard().getHeight() + 10.0f);
    return r;
}

int MatrixPanel::visibleEntries() const
{
    return juce::jmax (1, (int) ((matrixCard().getHeight() - kHeader - kFooter) / kRowH));
}

juce::Rectangle<float> MatrixPanel::entryRow (int i) const
{
    auto c = matrixCard().reduced (12.0f, 0.0f);
    return { c.getX(), matrixCard().getY() + kHeader + (float) i * kRowH, c.getWidth(), kRowH - 2.0f };
}

juce::Rectangle<float> MatrixPanel::buttonRect (int i) const
{
    auto c = matrixCard().reduced (12.0f, 0.0f);
    const float w = (c.getWidth() - 3 * 6.0f) / 4.0f;
    return { c.getX() + (float) i * (w + 6.0f), matrixCard().getBottom() - 38.0f, w, 26.0f };
}

void MatrixPanel::paint (juce::Graphics& g)
{
    auto s = ctx.song();
    const int playing = ctx.proc.seq.playhead.active.load() ? ctx.proc.seq.playhead.order.load() : -1;

    // ---- matrix card ------------------------------------------------------------------------
    auto card = matrixCard();
    fillPanel (g, card);
    g.setColour (text);
    g.setFont (sans (12.5f, 600));
    g.drawText ("Matrice", card.reduced (12.0f, 0.0f).withHeight (kHeader), juce::Justification::centredLeft);
    g.setColour (faint);
    g.setFont (mono (10.5f));
    g.drawText (juce::String (s->numOrders()) + U (" entrées"), card.reduced (12.0f, 0.0f).withHeight (kHeader), juce::Justification::centredRight);

    scroll = juce::jlimit (0, juce::jmax (0, s->numOrders() - visibleEntries()), scroll);
    for (int vi = 0; vi < visibleEntries(); ++vi)
    {
        const int e = vi + scroll;
        if (e >= s->numOrders()) break;
        auto r = entryRow (vi);
        const bool sel = e == s->selectedOrder;
        if (sel)
        {
            g.setColour (playRow);
            g.fillRoundedRectangle (r, 4.0f);
        }
        g.setFont (mono (11.5f, sel));
        g.setColour (e == playing ? accent : (sel ? juce::Colours::white : faint));
        g.drawText (hex2 (e), r.withWidth (22.0f).withTrimmedLeft (4.0f), juce::Justification::centredLeft);

        const auto* p = s->patternForOrder (e);
        if (p == nullptr) continue;
        float x = r.getX() + 26.0f;
        for (int t = 0; t < kNumTracks; ++t)
        {
            const int d = densityLevel (*p, t);
            const float alpha[] = { 0.09f, 0.38f, 0.68f, 1.0f };
            g.setColour (trackColour (t).withAlpha (alpha[d]));
            g.fillRoundedRectangle (x, r.getCentreY() - 6.0f, 12.0f, 12.0f, 2.0f);
            x += 15.0f;
        }
        if (e == playing)
        {
            g.setColour (accent);
            g.fillEllipse (r.getRight() - 6.0f, r.getCentreY() - 2.5f, 5.0f, 5.0f);
        }
    }

    // info line about the selected entry
    {
        const int pi = ctx.patternIndex();
        int uses = 0;
        for (int o : s->order) uses += o == pi;
        juce::String info = "Pat " + juce::String (hex2 (pi)) + U (" · ") + juce::String (s->patterns[(size_t) pi]->name);
        if (uses > 1) info << U (" · ") << uses << " alias";
        g.setColour (dim);
        g.setFont (sans (11.0f));
        g.drawText (info, juce::Rectangle<float> (card.getX() + 12.0f, card.getBottom() - kFooter + 2.0f, card.getWidth() - 24.0f, 20.0f),
                    juce::Justification::centredLeft, true);
    }

    for (int i = 0; i < 4; ++i)
    {
        auto b = buttonRect (i);
        g.setColour (raised);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (text);
        g.setFont (mono (13.0f, true));
        g.drawText (buttonLabels[i], b, juce::Justification::centred);
    }

    // ---- trigger card ---------------------------------------------------------------------------
    auto tc = triggerCard();
    if (tc.getHeight() < 60.0f) return;
    fillPanel (g, tc);
    auto inner = tc.reduced (12.0f, 0.0f);
    const bool trig = s->mode == PlayMode::Trigger;
    g.setColour (text);
    g.setFont (sans (12.5f, 600));
    g.drawText (U ("Déclencheurs"), inner.withHeight (kHeader), juce::Justification::centredLeft);
    if (! trig)
    {
        g.setColour (faint);
        g.setFont (sans (10.5f));
        g.drawText (U ("actif en mode Trigger"), inner.withHeight (kHeader).translated (0.0f, 16.0f), juce::Justification::centredLeft);
    }

    float y = tc.getY() + kHeader + (trig ? 0.0f : 16.0f);
    for (int e = 0; e < s->numOrders() && y + 22.0f < tc.getBottom() - 6.0f; ++e, y += 24.0f)
    {
        const int note = s->triggerBaseNote + e;
        if (note > 127) break;
        auto row = juce::Rectangle<float> (inner.getX(), y, inner.getWidth(), 20.0f);
        auto key = row.withWidth (38.0f);
        g.setColour (raised);
        g.fillRoundedRectangle (key, 4.0f);
        g.setColour (trig ? text : dim);
        g.setFont (mono (11.5f));
        g.drawText (noteToString ((uint8_t) note), key, juce::Justification::centred);
        g.setColour (trig && e == playing ? accent : dim);
        g.setFont (sans (11.5f));
        g.drawText ("Pat " + juce::String (hex2 (s->order[(size_t) e])), row, juce::Justification::centredRight);
    }
}

void MatrixPanel::mouseMove (const juce::MouseEvent& e)
{
    for (int i = 0; i < 4; ++i)
        if (buttonRect (i).contains (e.position))
        {
            setTooltip (U (buttonTips[i]));
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            return;
        }
    setTooltip (triggerCard().withHeight (kHeader).contains (e.position) ? U ("Cliquer pour choisir la note de départ") : "");
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void MatrixPanel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    scroll += w.deltaY < 0 ? 1 : -1;
    repaint();
}

void MatrixPanel::mouseDown (const juce::MouseEvent& e)
{
    auto s = ctx.song();
    for (int i = 0; i < 4; ++i)
        if (buttonRect (i).contains (e.position))
            return buttonAction (i);

    for (int vi = 0; vi < visibleEntries(); ++vi)
        if (entryRow (vi).contains (e.position))
        {
            const int entry = vi + scroll;
            if (entry >= s->numOrders()) return;
            if (e.mods.isPopupMenu()) return entryMenu (entry);
            ctx.doc().modifyQuiet ([entry] (Song& so) { so.selectedOrder = entry; });
            ctx.ui().hasSelection = false;
            ctx.uiChanged();
            return;
        }

    if (triggerCard().withHeight (kHeader).contains (e.position))
    {
        juce::PopupMenu m;
        m.addSectionHeader (U ("Note qui déclenche l'entrée 00"));
        for (int n = 24; n <= 84; n += 12)
            m.addItem (n + 1, noteToString ((uint8_t) n), true, n == s->triggerBaseNote);
        m.showMenuAsync ({}, [this] (int r) { if (r > 0) ctx.doc().modify ([r] (Song& so) { so.triggerBaseNote = r - 1; }); });
    }
}

void MatrixPanel::entryMenu (int entry)
{
    auto s = ctx.song();
    juce::PopupMenu pats;
    for (int p = 0; p < (int) s->patterns.size(); ++p)
        pats.addItem (1000 + p, hex2 (p) + "  " + s->patterns[(size_t) p]->name, true, s->order[(size_t) entry] == p);
    juce::PopupMenu m;
    m.addSubMenu (U ("Jouer le pattern…"), pats);
    m.addItem (1, U ("Retirer l'entrée"), s->numOrders() > 1);
    m.showMenuAsync ({}, [this, entry] (int r)
    {
        if (r >= 1000)
            ctx.doc().modify ([entry, r] (Song& so) { so.order[(size_t) entry] = r - 1000; });
        else if (r == 1)
        {
            ctx.doc().modify ([entry] (Song& so)
            {
                so.order.erase (so.order.begin() + entry);
                so.selectedOrder = juce::jlimit (0, so.numOrders() - 1, so.selectedOrder);
            });
        }
    });
}

void MatrixPanel::buttonAction (int i)
{
    auto s = ctx.song();
    if ((i == 0 || i == 1) && (int) s->patterns.size() >= kMaxPatterns) return;
    if (i != 3 && s->numOrders() >= kMaxOrder) return;

    ctx.doc().modify ([i] (Song& so)
    {
        const int sel = so.selectedOrder;
        const int pat = so.order[(size_t) sel];
        switch (i)
        {
            case 0:
            {
                auto p = std::make_shared<Pattern>();
                p->numRows = so.patterns[(size_t) pat]->numRows;
                p->name = "Pattern " + hex2 ((int) so.patterns.size());
                so.patterns.push_back (p);
                so.order.insert (so.order.begin() + sel + 1, (int) so.patterns.size() - 1);
                so.selectedOrder = sel + 1;
                break;
            }
            case 1:
            {
                auto p = std::make_shared<Pattern> (*so.patterns[(size_t) pat]);
                p->name = p->name.substr (0, 20) + " b";
                so.patterns.push_back (p);
                so.order.insert (so.order.begin() + sel + 1, (int) so.patterns.size() - 1);
                so.selectedOrder = sel + 1;
                break;
            }
            case 2:
                so.order.insert (so.order.begin() + sel + 1, pat);
                so.selectedOrder = sel + 1;
                break;
            case 3:
                if (so.numOrders() > 1)
                {
                    so.order.erase (so.order.begin() + sel);
                    so.selectedOrder = juce::jlimit (0, so.numOrders() - 1, sel);
                }
                break;
            default: break;
        }
    });
    ctx.ui().hasSelection = false;
    ctx.uiChanged();
}

} // namespace lattice
