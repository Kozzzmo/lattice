#include "ui/Inspector.h"

namespace lattice
{

using namespace theme;

// ---- ParamRow ---------------------------------------------------------------------------------------

ParamRow::ParamRow (juce::String l, double min, double max, double st, std::function<juce::String (double)> f)
    : label (std::move (l)), step (st), format (std::move (f))
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (min, max, st);
    slider.setColour (juce::Slider::trackColourId, accent);
    slider.setMouseDragSensitivity (300);
    slider.setVelocityBasedMode (false);
    slider.onValueChange = [this] { repaint(); if (onChange) onChange (slider.getValue()); };
    addAndMakeVisible (slider);
}

void ParamRow::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedLeft (96).withTrimmedRight (52));
}

void ParamRow::paint (juce::Graphics& g)
{
    g.setFont (sans (12.5f));
    g.setColour (dim);
    g.drawText (label, getLocalBounds().withWidth (92), juce::Justification::centredLeft, true);
    g.setFont (mono (12.0f));
    g.setColour (text);
    g.drawText (format (slider.getValue()), getLocalBounds().removeFromRight (48), juce::Justification::centredRight);
}

// ---- WaveView: what the instrument's oscillator looks like ------------------------------------------

class WaveView : public juce::Component
{
public:
    explicit WaveView (Context& c) : ctx (c) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (bg);
        g.fillRoundedRectangle (r, 6.0f);
        const auto& ins = ctx.song()->instruments[(size_t) ctx.instrumentIndex()];
        auto area = r.reduced (10.0f, 12.0f);
        juce::Path p;
        juce::Random rnd (7);
        const int n = 200;
        float noise = 0.0f;
        for (int i = 0; i <= n; ++i)
        {
            const float x = (float) i / (float) n;
            const float ph = std::fmod (x * 3.0f, 1.0f);
            float s = 0.0f;
            switch (ins.wave)
            {
                case Waveform::Pulse:    s = ph < ins.duty ? 1.0f : -1.0f; break;
                case Waveform::Saw:      s = 2.0f * ph - 1.0f; break;
                case Waveform::Triangle: s = 4.0f * std::abs (ph - 0.5f) - 1.0f; break;
                case Waveform::Noise:    if (i % 3 == 0) noise = rnd.nextFloat() * 2.0f - 1.0f; s = noise; break;
            }
            if (ins.crushBits < 16)
            {
                const float levels = (float) ((1 << juce::jmax (2, ins.crushBits)) / 2);
                s = std::round (s * levels) / levels;
            }
            const juce::Point<float> pt (area.getX() + x * area.getWidth(), area.getCentreY() - s * area.getHeight() * 0.5f);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (trackColour (ctx.ui().track));
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }
private:
    Context& ctx;
};

// ---- EuclidView: the rhythm as a circle, hits joined into a polygon ---------------------------------

class EuclidView : public juce::Component
{
public:
    explicit EuclidView (Context& c) : ctx (c) {}
    int playStep = -1;

    void paint (juce::Graphics& g) override
    {
        auto song = ctx.song();
        const int t = ctx.ui().track;
        const auto& e = song->euclid[(size_t) t];
        const auto rhythm = euclideanRhythm (e.steps, e.hits, e.rotation);
        const auto col = trackColour (t);
        auto c = getLocalBounds().toFloat().getCentre();
        const float radius = juce::jmin ((float) getWidth(), (float) getHeight()) * 0.5f - 14.0f;

        g.setColour (line);
        g.drawEllipse (c.x - radius, c.y - radius, radius * 2, radius * 2, 1.0f);

        juce::Path poly;
        bool first = true;
        const int n = (int) rhythm.size();
        auto at = [&] (int s)
        {
            const float a = (float) s / (float) n * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::halfPi;
            return juce::Point<float> (c.x + radius * std::cos (a), c.y + radius * std::sin (a));
        };
        for (int s = 0; s < n; ++s)
            if (rhythm[(size_t) s])
            {
                if (first) { poly.startNewSubPath (at (s)); first = false; }
                else poly.lineTo (at (s));
            }
        if (! first)
        {
            poly.closeSubPath();
            g.setColour (col.withAlpha (0.10f));
            g.fillPath (poly);
            g.setColour (col.withAlpha (0.5f));
            g.strokePath (poly, juce::PathStrokeType (1.0f));
        }

        for (int s = 0; s < n; ++s)
        {
            const auto p = at (s);
            const bool hit = rhythm[(size_t) s], now = s == playStep;
            const float r = hit ? (now ? 9.0f : 7.0f) : 4.0f;
            g.setColour (hit ? (now ? juce::Colours::white : col) : panel);
            g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
            g.setColour (now ? juce::Colours::white : hit ? col : ghost);
            g.drawEllipse (p.x - r, p.y - r, r * 2, r * 2, 2.0f);
        }

        g.setColour (text);
        g.setFont (sans (26.0f, 700));
        g.drawText (juce::String (e.hits) + "/" + juce::String (e.steps), getLocalBounds().toFloat().withSizeKeepingCentre (140.0f, 34.0f).translated (0, -8),
                    juce::Justification::centred);
        g.setColour (dim);
        g.setFont (mono (11.0f));
        g.drawText ("rot " + juce::String (e.rotation >= 0 ? "+" : "") + juce::String (e.rotation),
                    getLocalBounds().toFloat().withSizeKeepingCentre (140.0f, 16.0f).translated (0, 16), juce::Justification::centred);
    }
private:
    Context& ctx;
};

// ---- Inspector ----------------------------------------------------------------------------------------

namespace
{
    const char* tabNames[] = { "Instrument", "G\xc3\xa9n\xc3\xa9ratif", "FX" };
    const char* waveNames[] = { "Pulse", "Tri", "Scie", "Bruit" };

    juce::String ms (double s) { return s < 1.0 ? juce::String (juce::roundToInt (s * 1000.0)) + " ms" : juce::String (s, 2) + " s"; }
    juce::String pct (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; }
}

Inspector::Inspector (Context& c) : Panel (c)
{
    // ---- instrument tab -----------------------------------------------------------------------
    for (auto* b : { &prevIns, &nextIns })
    {
        b->setColour (juce::TextButton::buttonColourId, raised);
        addChildComponent (*b);
    }
    prevIns.onClick = [this] { const int t = ctx.ui().track; ctx.doc().modify ([t] (Song& s) { auto& i = s.tracks[(size_t) t].instrument; i = (i + kNumInstruments - 1) % kNumInstruments; }); };
    nextIns.onClick = [this] { const int t = ctx.ui().track; ctx.doc().modify ([t] (Song& s) { auto& i = s.tracks[(size_t) t].instrument; i = (i + 1) % kNumInstruments; }); };

    for (int w = 0; w < 4; ++w)
    {
        auto& b = waveButtons[w];
        b.setButtonText (waveNames[w]);
        b.setColour (juce::TextButton::buttonColourId, raised);
        b.setColour (juce::TextButton::buttonOnColourId, playRow);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        b.setColour (juce::TextButton::textColourOffId, dim);
        b.onClick = [this, w] { editInstrument ([w] (Instrument& i) { i.wave = (Waveform) w; }, {}); };
        addChildComponent (b);
    }
    waveView = std::make_unique<WaveView> (ctx);
    addChildComponent (*waveView);

    auto addIns = [this] (const char* label, double mn, double mx, double st, std::function<juce::String (double)> fmt,
                          std::function<void (Instrument&, double)> set)
    {
        auto row = std::make_unique<ParamRow> (U (label), mn, mx, st, std::move (fmt));
        const std::string key = std::string ("ins-") + label;
        row->onChange = [this, set, key] (double v) { editInstrument ([set, v] (Instrument& i) { set (i, v); }, key); };
        addChildComponent (*row);
        insRows.push_back (std::move (row));
    };
    addIns ("Rapport cycl.", 0.05, 0.95, 0.01, pct, [] (Instrument& i, double v) { i.duty = (float) v; });
    addIns ("Attaque", 0.0, 2.0, 0.001, ms, [] (Instrument& i, double v) { i.attack = (float) v; });
    addIns ("D\xc3\xa9" "clin", 0.005, 3.0, 0.001, ms, [] (Instrument& i, double v) { i.decay = (float) v; });
    addIns ("Maintien", 0.0, 1.0, 0.01, pct, [] (Instrument& i, double v) { i.sustain = (float) v; });
    addIns ("Rel\xc3\xa2" "che", 0.005, 4.0, 0.001, ms, [] (Instrument& i, double v) { i.release = (float) v; });
    addIns ("Balayage", -36.0, 36.0, 1.0, [] (double v) { return (v > 0 ? "+" : "") + juce::String ((int) v) + " st"; },
            [] (Instrument& i, double v) { i.sweep = (float) v; });
    addIns ("Dur\xc3\xa9" "e balay.", 0.005, 1.0, 0.001, ms, [] (Instrument& i, double v) { i.sweepTime = (float) v; });
    addIns ("R\xc3\xa9solution", 2.0, 16.0, 1.0, [] (double v) { return v >= 16 ? juce::String ("off") : juce::String ((int) v) + " bits"; },
            [] (Instrument& i, double v) { i.crushBits = (int) v; });
    addIns ("Glide", 0.0, 1.0, 0.001, ms, [] (Instrument& i, double v) { i.glide = (float) v; });
    addIns ("Volume", 0.0, 1.0, 0.01, pct, [] (Instrument& i, double v) { i.gain = (float) v; });
    for (auto& r : insRows) r->slider.setSkewFactor (1.0);
    for (int i : { 1, 2, 4, 6, 8 }) insRows[(size_t) i]->slider.setSkewFactorFromMidPoint (insRows[(size_t) i]->slider.getMaximum() * 0.12);

    // ---- generative tab ------------------------------------------------------------------------
    euclidView = std::make_unique<EuclidView> (ctx);
    addChildComponent (*euclidView);

    auto addGen = [this] (const char* label, double mn, double mx, std::function<juce::String (double)> fmt, std::function<void (Song&, int, int)> set)
    {
        auto row = std::make_unique<ParamRow> (U (label), mn, mx, 1.0, std::move (fmt));
        const std::string key = std::string ("gen-") + label;
        row->onChange = [this, set, key] (double v)
        {
            const int t = ctx.ui().track;
            ctx.doc().modify ([set, t, v] (Song& s) { set (s, t, (int) v); }, key + std::to_string (t));
        };
        addChildComponent (*row);
        genRows.push_back (std::move (row));
    };
    auto plain = [] (double v) { return juce::String ((int) v); };
    addGen ("Pas", 1, 32, plain, [] (Song& s, int t, int v) { auto& e = s.euclid[(size_t) t]; e.steps = v; e.hits = juce::jmin (e.hits, v); e.rotation = juce::jmin (e.rotation, v - 1); });
    addGen ("Coups", 0, 32, plain, [] (Song& s, int t, int v) { auto& e = s.euclid[(size_t) t]; e.hits = juce::jmin (v, e.steps); });
    addGen ("Rotation", 0, 31, [] (double v) { return "+" + juce::String ((int) v); }, [] (Song& s, int t, int v) { auto& e = s.euclid[(size_t) t]; e.rotation = juce::jmin (v, e.steps - 1); });
    addGen ("Probabilit\xc3\xa9", 0, 100, [] (double v) { return juce::String ((int) v) + " %"; }, [] (Song& s, int t, int v) { s.euclid[(size_t) t].prob = v; });
    addGen ("Humanize", 0, 64, [] (double v) { return U ("\xc2\xb1") + juce::String ((int) v); }, [] (Song& s, int t, int v) { s.tracks[(size_t) t].humanize = v; });
    addGen ("Long. piste", 0, kMaxRows, [] (double v) { return v <= 0 ? juce::String ("auto") : juce::String ((int) v); },
            [] (Song& s, int t, int v) { s.tracks[(size_t) t].length = v; });

    varyToggle.onClick = [this] { const bool v = varyToggle.getToggleState(); ctx.doc().modify ([v] (Song& s) { s.varyEachLoop = v; }); };
    addChildComponent (varyToggle);

    writeButton.setColour (juce::TextButton::buttonColourId, accent);
    writeButton.setColour (juce::TextButton::textColourOffId, bg);
    writeButton.onClick = [this] { writeEuclid(); };
    addChildComponent (writeButton);

    reseedButton.setColour (juce::TextButton::buttonColourId, raised);
    reseedButton.setTooltip (U ("Nouvelle graine : rejoue autrement les probabilités"));
    reseedButton.onClick = [this]
    {
        const auto seed = (uint32_t) juce::Random::getSystemRandom().nextInt (0x10000);
        ctx.doc().modify ([seed] (Song& s) { s.seed = seed; });
    };
    addChildComponent (reseedButton);

    showTab (ctx.ui().inspectorTab);
    refresh();
    startTimerHz (30);
}

Inspector::~Inspector() = default;

void Inspector::timerCallback()
{
    auto& ph = ctx.proc.seq.playhead;
    const int t = ctx.ui().track;
    int step = -1;
    if (ph.active.load() && ph.order.load() == ctx.song()->selectedOrder)
        step = ph.rows[t].load() % juce::jmax (1, ctx.song()->euclid[(size_t) t].steps);
    if (step != lastPlayStep)
    {
        lastPlayStep = step;
        euclidView->playStep = step;
        euclidView->repaint();
    }
}

juce::Rectangle<float> Inspector::tabRect (int i) const
{
    auto r = getLocalBounds().toFloat().reduced (14.0f).withHeight (32.0f).reduced (3.0f);
    const float w = r.getWidth() / 3.0f;
    return { r.getX() + w * (float) i, r.getY(), w, r.getHeight() };
}

void Inspector::showTab (int tab)
{
    ctx.ui().inspectorTab = tab;
    const bool ins = tab == 0, gen = tab == 1;
    prevIns.setVisible (ins); nextIns.setVisible (ins);
    for (auto& b : waveButtons) b.setVisible (ins);
    waveView->setVisible (ins);
    for (auto& r : insRows) r->setVisible (ins);
    euclidView->setVisible (gen);
    for (auto& r : genRows) r->setVisible (gen);
    varyToggle.setVisible (gen);
    writeButton.setVisible (gen);
    reseedButton.setVisible (gen);
    resized();
    repaint();
}

void Inspector::resized()
{
    auto r = getLocalBounds().reduced (14);
    r.removeFromTop (32 + 12); // tabs

    // instrument
    {
        auto a = r;
        auto head = a.removeFromTop (34);
        nextIns.setBounds (head.removeFromRight (28).withSizeKeepingCentre (28, 26));
        head.removeFromRight (4);
        prevIns.setBounds (head.removeFromRight (28).withSizeKeepingCentre (28, 26));
        a.removeFromTop (8);
        auto waves = a.removeFromTop (28);
        const int w = (waves.getWidth() - 12) / 4;
        for (int i = 0; i < 4; ++i)
            waveButtons[i].setBounds (waves.getX() + i * (w + 4), waves.getY(), w, 28);
        a.removeFromTop (10);
        waveView->setBounds (a.removeFromTop (64));
        a.removeFromTop (12);
        for (auto& row : insRows)
            row->setBounds (a.removeFromTop (27));
    }

    // generative
    {
        auto a = r;
        a.removeFromTop (30);
        euclidView->setBounds (a.removeFromTop (juce::jmin (210, juce::jmax (150, a.getHeight() - 330))));
        a.removeFromTop (6);
        for (auto& row : genRows)
            row->setBounds (a.removeFromTop (27));
        a.removeFromTop (6);
        varyToggle.setBounds (a.removeFromTop (28));
        auto bottom = r.removeFromBottom (40);
        reseedButton.setBounds (bottom.removeFromRight (70));
        bottom.removeFromRight (8);
        writeButton.setBounds (bottom);
    }
}

void Inspector::refresh()
{
    auto song = ctx.song();
    const int t = ctx.ui().track;
    const auto& ins = song->instruments[(size_t) ctx.instrumentIndex()];
    for (int w = 0; w < 4; ++w)
        waveButtons[w].setToggleState ((int) ins.wave == w, juce::dontSendNotification);
    const double vals[] = { ins.duty, ins.attack, ins.decay, ins.sustain, ins.release, ins.sweep, ins.sweepTime, (double) ins.crushBits, ins.glide, ins.gain };
    for (size_t i = 0; i < insRows.size(); ++i)
        insRows[i]->setValue (vals[i]);
    insRows[0]->setEnabled (ins.wave == Waveform::Pulse);
    insRows[0]->setAlpha (ins.wave == Waveform::Pulse ? 1.0f : 0.4f);

    const auto& e = song->euclid[(size_t) t];
    genRows[1]->setRange (0, e.steps);
    genRows[2]->setRange (0, juce::jmax (1, e.steps - 1));
    genRows[5]->setRange (0, ctx.pattern().numRows);
    const double g[] = { (double) e.steps, (double) e.hits, (double) e.rotation, (double) e.prob, (double) song->tracks[(size_t) t].humanize,
                         (double) song->tracks[(size_t) t].length };
    for (size_t i = 0; i < genRows.size(); ++i)
    {
        genRows[i]->slider.setColour (juce::Slider::trackColourId, trackColour (t));
        genRows[i]->setValue (g[i]);
    }
    for (auto& r : insRows) r->slider.setColour (juce::Slider::trackColourId, trackColour (t));
    varyToggle.setToggleState (song->varyEachLoop, juce::dontSendNotification);
    reseedButton.setButtonText (juce::String::toHexString ((int) song->seed).toUpperCase().paddedLeft ('0', 2));

    waveView->repaint();
    euclidView->repaint();
    repaint();
}

void Inspector::editInstrument (const std::function<void (Instrument&)>& fn, const std::string& key)
{
    const int i = ctx.instrumentIndex();
    ctx.doc().modify ([fn, i] (Song& s) { fn (s.instruments[(size_t) i]); }, key.empty() ? std::string() : key + std::to_string (i));
}

void Inspector::writeEuclid()
{
    const int t = ctx.ui().track;
    auto song = ctx.song();
    const auto& pat = ctx.pattern();
    uint8_t note = 60;
    const auto& cur = pat.at (t, ctx.ui().row);
    if (cur.hasNote()) note = cur.note;
    else
        for (int r = 0; r < pat.numRows; ++r)
            if (pat.at (t, r).hasNote()) { note = pat.at (t, r).note; break; }
    const auto ins = (uint8_t) song->tracks[(size_t) t].instrument;
    const auto e = song->euclid[(size_t) t];
    const int len = song->trackLength (t, pat);
    ctx.editPattern ([=] (Pattern& p) { lattice::writeEuclid (p, t, len, e, note, ins, kNone); });
}

void Inspector::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
        if (tabRect (i).contains (e.position))
        {
            showTab (i);
            return;
        }
}

void Inspector::paint (juce::Graphics& g)
{
    auto song = ctx.song();
    const int t = ctx.ui().track;
    const int tab = ctx.ui().inspectorTab;

    // tabs
    auto all = tabRect (0).getUnion (tabRect (2)).expanded (3.0f);
    g.setColour (bg);
    g.fillRoundedRectangle (all, 8.0f);
    for (int i = 0; i < 3; ++i)
    {
        const bool on = i == tab;
        if (on)
        {
            g.setColour (playRow);
            g.fillRoundedRectangle (tabRect (i), 6.0f);
        }
        g.setColour (on ? juce::Colours::white : dim);
        g.setFont (sans (12.5f, on ? 600 : 400));
        g.drawText (U (tabNames[i]), tabRect (i), juce::Justification::centred);
    }

    auto r = getLocalBounds().toFloat().reduced (14.0f);
    r.removeFromTop (44.0f);
    const auto col = trackColour (t);

    if (tab == 0)
    {
        const int ii = ctx.instrumentIndex();
        auto head = r.removeFromTop (34.0f).withTrimmedRight (66.0f);
        g.setColour (text);
        g.setFont (sans (15.0f, 600));
        g.drawText (juce::String (hex2 (ii)) + U (" \xc2\xb7 ") + juce::String (song->instruments[(size_t) ii].name), head.withTrimmedBottom (12.0f),
                    juce::Justification::centredLeft, true);
        g.setColour (dim);
        g.setFont (sans (11.0f));
        g.drawText (U ("Synthé interne · piste ") + juce::String (song->tracks[(size_t) t].name), head.withTrimmedTop (20.0f),
                    juce::Justification::centredLeft, true);
    }
    else if (tab == 1)
    {
        auto head = r.removeFromTop (26.0f);
        g.setColour (col);
        g.fillRoundedRectangle (head.getX(), head.getCentreY() - 5.0f, 10.0f, 10.0f, 3.0f);
        g.setColour (text);
        g.setFont (sans (15.0f, 600));
        g.drawText (juce::String (song->tracks[(size_t) t].name) + U (" · Euclidien"), head.withTrimmedLeft (18.0f), juce::Justification::centredLeft);
    }
    else
    {
        struct Item { const char* code; const char* desc; };
        const Item fx[] = {
            { "Axy", "Arp\xc3\xa8ge : note, +x, +y demi-tons \xc3\xa0 chaque tick" },
            { "Cxx", "Coupe la note au tick xx" },
            { "Dxx", "Retarde la ligne de xx ticks" },
            { "Gxx", "Glisse vers la note (legato)" },
            { "Pxx", "Panoramique : 00 gauche, 80 centre, FF droite" },
            { "Rxy", "Redéclenche la note tous les y ticks" },
            { "Vxy", "Vibrato : vitesse x, profondeur y" },
        };
        const Item cols[] = {
            { "Prb", "Probabilit\xc3\xa9 de jouer la note (000-100 %)" },
            { "Vo",  "V\xc3\xa9locit\xc3\xa9 00-7F, ou volume si la ligne n'a pas de note" },
            { "L",   "Longueur de piste : clic sur l'en-t\xc3\xaate" },
        };
        auto section = [&] (const char* title)
        {
            g.setColour (text);
            g.setFont (sans (13.0f, 600));
            g.drawText (U (title), r.removeFromTop (24.0f), juce::Justification::centredLeft);
        };
        auto item = [&] (const Item& it, juce::Colour c)
        {
            auto row = r.removeFromTop (34.0f);
            g.setColour (c);
            g.setFont (mono (12.0f, true));
            g.drawText (U (it.code), row.removeFromLeft (40.0f).withHeight (18.0f), juce::Justification::centredLeft);
            g.setColour (dim);
            g.setFont (sans (11.5f));
            g.drawFittedText (U (it.desc), row.toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
        };
        section ("Commandes d'effet");
        for (auto& i : fx) item (i, info);
        r.removeFromTop (6.0f);
        section ("Colonnes");
        for (auto& i : cols) item (i, prob);
        r.removeFromTop (6.0f);
        section ("Clavier");
        g.setFont (sans (11.5f));
        g.setColour (dim);
        g.drawFittedText (U ("F1-F8 octave · Espace aperçu · Suppr efface · Inser / Retour arrière décale · "
                             "Ctrl+↑↓ transpose · Ctrl+I interpole · Ctrl+Z annule · < note OFF"),
                          r.removeFromTop (70.0f).toNearestInt(), juce::Justification::topLeft, 4, 1.0f);
    }
}

} // namespace lattice
