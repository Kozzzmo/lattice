#include "ui/PatternGrid.h"

namespace lattice
{

using namespace theme;

namespace
{
    constexpr float kPad = 10.0f, kGap = 6.0f;

    juce::String cellText (const Cell& c, int col)
    {
        switch (col)
        {
            case 0: return noteToString (c.note);
            case 1: return c.instrument == kNone ? ".." : juce::String (hex2 (c.instrument));
            case 2: return c.volume == kNone ? ".." : juce::String (hex2 (c.volume));
            case 3: return c.prob == kNone ? "..." : juce::String (c.prob).paddedLeft ('0', 3);
            case 4: return c.fxCmd == 0 ? "..." : juce::String::charToString ((juce::juce_wchar) c.fxCmd) + juce::String (hex2 (c.fxVal));
            default: return {};
        }
    }

    bool colEmpty (const Cell& c, int col)
    {
        switch (col)
        {
            case 0: return c.note == kNoteEmpty;
            case 1: return c.instrument == kNone;
            case 2: return c.volume == kNone;
            case 3: return c.prob == kNone;
            case 4: return c.fxCmd == 0;
            default: return true;
        }
    }

    int colOfField (int f) { return f == 0 ? 0 : f <= 2 ? 1 : f <= 4 ? 2 : f <= 7 ? 3 : 4; }

    const juce::String allowedFx = "ACDGPRV";
}

PatternGrid::PatternGrid (Context& c) : Panel (c)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    startTimerHz (30);
}

void PatternGrid::resized() { ensureCursorVisible(); }

// ---- geometry ------------------------------------------------------------------------------------

int PatternGrid::visibleTracks() const { return juce::jlimit (1, kNumTracks, (getWidth() - kNumW) / kTrackW); }
int PatternGrid::visibleRows() const   { return juce::jmax (1, (getHeight() - kHeaderH) / kRowH); }
float PatternGrid::charW() const       { return textWidth (mono (12.0f), "0"); }

float PatternGrid::fieldX (int f) const
{
    const float cw = charW();
    const float c1 = 3 * cw + kGap, c2 = c1 + 2 * cw + kGap, c3 = c2 + 2 * cw + kGap, c4 = c3 + 3 * cw + kGap;
    const float xs[kFields] = { 0, c1, c1 + cw, c2, c2 + cw, c3, c3 + cw, c3 + 2 * cw, c4, c4 + cw, c4 + 2 * cw };
    return kPad + xs[juce::jlimit (0, kFields - 1, f)];
}

float PatternGrid::fieldW (int f) const { return f == 0 ? 3 * charW() : charW(); }

int PatternGrid::fieldAt (float x) const
{
    int best = 0;
    for (int f = 0; f < kFields; ++f)
        if (x >= fieldX (f) - kGap * 0.5f)
            best = f;
    return best;
}

juce::Rectangle<float> PatternGrid::cellRect (int vt, int vr) const
{
    return { (float) (kNumW + vt * kTrackW), (float) (kHeaderH + vr * kRowH), (float) kTrackW, (float) kRowH };
}

bool PatternGrid::hitCell (juce::Point<float> p, int& track, int& row, int& field) const
{
    if (p.y < kHeaderH || p.x < kNumW) return false;
    const int vt = (int) ((p.x - kNumW) / kTrackW);
    const int vr = (int) ((p.y - kHeaderH) / kRowH);
    if (vt >= visibleTracks()) return false;
    track = juce::jlimit (0, kNumTracks - 1, trackScroll + vt);
    row = juce::jlimit (0, ctx.pattern().numRows - 1, rowScroll + vr);
    field = fieldAt (p.x - (float) (kNumW + vt * kTrackW));
    return true;
}

int PatternGrid::playRowFor (int t) const
{
    auto& ph = ctx.proc.seq.playhead;
    if (! ph.active.load() || ph.order.load() != ctx.song()->selectedOrder)
        return -1;
    return t < 0 ? ph.row.load() : ph.rows[t].load();
}

void PatternGrid::ensureCursorVisible()
{
    auto& u = ctx.ui();
    const int nVis = visibleTracks(), nRows = visibleRows(), numRows = ctx.pattern().numRows;
    u.row = juce::jlimit (0, numRows - 1, u.row);
    if (u.track < trackScroll) trackScroll = u.track;
    if (u.track >= trackScroll + nVis) trackScroll = u.track - nVis + 1;
    trackScroll = juce::jlimit (0, juce::jmax (0, kNumTracks - nVis), trackScroll);

    const int margin = juce::jmin (4, nRows / 4);
    if (u.row < rowScroll + margin) rowScroll = u.row - margin;
    if (u.row > rowScroll + nRows - 1 - margin) rowScroll = u.row - nRows + 1 + margin;
    rowScroll = juce::jlimit (0, juce::jmax (0, numRows - nRows), rowScroll);
}

void PatternGrid::timerCallback()
{
    const int pr = playRowFor (-1);
    if (pr != lastPlayRow)
    {
        lastPlayRow = pr;
        if (pr >= 0 && ctx.ui().follow)
        {
            const int nRows = visibleRows();
            rowScroll = juce::jlimit (0, juce::jmax (0, ctx.pattern().numRows - nRows), pr - nRows / 3);
        }
        repaint();
    }
}

// ---- painting ------------------------------------------------------------------------------------

void PatternGrid::paint (juce::Graphics& g)
{
    auto song = ctx.song();
    const auto& pat = ctx.pattern();
    const auto& u = ctx.ui();
    const int nVis = visibleTracks(), nRows = visibleRows();
    const int lpb = juce::jmax (1, song->lpb);
    const bool focused = hasKeyboardFocus (true);
    const int mainPlay = playRowFor (-1);

    int s0t = 0, s0r = 0, s1t = -1, s1r = -1;
    if (u.hasSelection) normaliseSelection (s0t, s0r, s1t, s1r);

    // ---- track headers ------------------------------------------------------------------------
    g.setColour (line);
    g.fillRect (0, kHeaderH - 1, getWidth(), 1);

    for (int vt = 0; vt < nVis; ++vt)
    {
        const int t = trackScroll + vt;
        const auto& tr = song->tracks[(size_t) t];
        const float x = (float) (kNumW + vt * kTrackW);
        const auto col = trackColour (t);
        const bool audible = song->trackAudible (t);

        g.setColour (col.withAlpha (audible ? 1.0f : 0.35f));
        g.fillRoundedRectangle (x + kPad, -3.0f, kTrackW - 2 * kPad, 6.0f, 3.0f);

        // output pill (right), M / S, name
        const juce::String out = tr.output == OutputKind::Internal ? "Int" : "MIDI " + juce::String (tr.midiChannel);
        const float pw = textWidth (mono (10.0f), out) + 14.0f;
        auto pill = juce::Rectangle<float> (x + kTrackW - kPad - pw, 14.0f, pw, 18.0f);
        drawPill (g, pill, tr.output == OutputKind::Internal ? raised : info);
        g.setColour (tr.output == OutputKind::Internal ? volCol : bg);
        g.setFont (mono (10.0f, tr.output == OutputKind::Midi));
        g.drawText (out, pill, juce::Justification::centred);

        auto sR = juce::Rectangle<float> (pill.getX() - 20.0f, 15.0f, 16.0f, 16.0f);
        auto mR = sR.translated (-19.0f, 0.0f);
        g.setFont (mono (9.5f, true));
        g.setColour (tr.mute ? danger : raised);
        g.fillRoundedRectangle (mR, 3.0f);
        g.setColour (tr.mute ? bg : faint);
        g.drawText ("M", mR, juce::Justification::centred);
        g.setColour (tr.solo ? prob : raised);
        g.fillRoundedRectangle (sR, 3.0f);
        g.setColour (tr.solo ? bg : faint);
        g.drawText ("S", sR, juce::Justification::centred);

        g.setColour (audible ? text : dim);
        g.setFont (sans (13.0f, 600));
        g.drawText (tr.name, juce::Rectangle<float> (x + kPad, 12.0f, mR.getX() - x - kPad - 4.0f, 22.0f),
                    juce::Justification::centredLeft, true);

        // column labels + length
        g.setFont (mono (10.0f));
        g.setColour (faint);
        const char* labels[] = { "Not", "In", "Vo", "Prb" };
        const int at[] = { 0, 1, 3, 5 };
        for (int i = 0; i < 4; ++i)
            g.drawText (labels[i], juce::Rectangle<float> (x + fieldX (at[i]), 44.0f, 30.0f, 14.0f), juce::Justification::centredLeft);

        const int len = song->trackLength (t, pat);
        const bool custom = len != pat.numRows;
        const juce::String lenText = "L" + juce::String (hex2 (len)) + (custom ? juce::String (juce::CharPointer_UTF8 (" \xe2\x86\xbb")) : juce::String());
        g.setColour (custom ? accent : faint);
        g.drawText (lenText, juce::Rectangle<float> (x + kTrackW - kPad - 50.0f, 44.0f, 50.0f, 14.0f), juce::Justification::centredRight);
    }

    // ---- rows ---------------------------------------------------------------------------------------
    const float cw = charW();
    const float colX[5] = { fieldX (0), fieldX (1), fieldX (3), fieldX (5), fieldX (8) };
    const int colChars[5] = { 3, 2, 2, 3, 3 };
    const juce::Colour colColours[5] = { {}, insCol, volCol, prob, info };
    const auto font = mono (12.0f);
    const auto fontBold = mono (12.0f, true);

    for (int vr = 0; vr < nRows; ++vr)
    {
        const int row = rowScroll + vr;
        if (row >= pat.numRows) break;
        const float y = (float) (kHeaderH + vr * kRowH);
        const bool beat = row % lpb == 0;

        if (beat)
        {
            g.setColour (beatRow);
            g.fillRect (0.0f, y, (float) (kNumW + nVis * kTrackW), (float) kRowH);
        }
        if (row == mainPlay)
        {
            g.setColour (playRow);
            g.fillRect (0.0f, y, (float) kNumW, (float) kRowH);
        }

        g.setFont (row == u.row ? fontBold : font);
        g.setColour (row == mainPlay ? juce::Colours::white : row == u.row && focused ? accent : beat ? volCol : faint);
        g.drawText (hex2 (row), juce::Rectangle<float> (10.0f, y, (float) kNumW - 10.0f, (float) kRowH), juce::Justification::centredLeft);

        for (int vt = 0; vt < nVis; ++vt)
        {
            const int t = trackScroll + vt;
            const float x = (float) (kNumW + vt * kTrackW);
            const auto& c = pat.at (t, row);
            const int len = song->trackLength (t, pat);
            const bool ghostRow = row >= len;
            const bool playing = playRowFor (t) == row;
            const bool selected = t >= s0t && t <= s1t && row >= s0r && row <= s1r;

            if (selected)
            {
                g.setColour (selection);
                g.fillRect (x, y, (float) kTrackW, (float) kRowH);
            }
            if (playing)
            {
                g.setColour (playRow);
                g.fillRect (x, y, (float) kTrackW, (float) kRowH);
            }

            for (int col = 0; col < 5; ++col)
            {
                juce::Colour colour;
                if (playing && ! colEmpty (c, col))      colour = juce::Colours::white;
                else if (colEmpty (c, col))              colour = playing ? ghost : empty;
                else if (ghostRow)                       colour = ghost;
                else                                     colour = col == 0 ? trackColour (t) : colColours[col];
                if (col == 0 && c.note == kNoteOff && ! playing && ! ghostRow) colour = dim;

                g.setColour (colour);
                g.setFont (col == 0 && c.hasNote() ? fontBold : font);
                g.drawText (cellText (c, col), juce::Rectangle<float> (x + colX[col], y, cw * (float) colChars[col] + 2.0f, (float) kRowH),
                            juce::Justification::centredLeft, false);
            }

            // cursor
            if (t == u.track && row == u.row)
            {
                const auto r = juce::Rectangle<float> (x + fieldX (u.field) - 1.0f, y + 2.0f, fieldW (u.field) + 2.0f, (float) kRowH - 4.0f);
                g.setColour (focused ? trackColour (t) : trackColour (t).withAlpha (0.35f));
                g.fillRoundedRectangle (r, 2.0f);
                const int col = colOfField (u.field);
                auto txt = cellText (c, col);
                const int ci = u.field == 0 ? 0 : (int) (u.field - (col == 1 ? 1 : col == 2 ? 3 : col == 3 ? 5 : 8));
                const auto shown = u.field == 0 ? txt : txt.substring (ci, ci + 1);
                g.setColour (focused ? bg : text);
                g.setFont (fontBold);
                g.drawText (shown, r.translated (1.0f, 0.0f), juce::Justification::centredLeft, false);
            }
        }
    }

    // separators between tracks
    g.setColour (line.withAlpha (0.6f));
    for (int vt = 1; vt < nVis; ++vt)
        g.fillRect ((float) (kNumW + vt * kTrackW), (float) kHeaderH, 1.0f, (float) (getHeight() - kHeaderH));

    // horizontal scroll hint: which tracks are visible
    if (kNumTracks > nVis)
    {
        g.setColour (faint);
        g.setFont (mono (9.5f));
        g.drawText (juce::String (trackScroll + 1) + "-" + juce::String (trackScroll + nVis) + "/" + juce::String (kNumTracks),
                    juce::Rectangle<float> (4.0f, 44.0f, (float) kNumW - 4.0f, 14.0f), juce::Justification::centredLeft);
    }
}

// ---- cursor & selection ------------------------------------------------------------------------

void PatternGrid::normaliseSelection (int& t0, int& r0, int& t1, int& r1) const
{
    const auto& u = ctx.ui();
    t0 = juce::jmin (u.selTrack0, u.selTrack1); t1 = juce::jmax (u.selTrack0, u.selTrack1);
    r0 = juce::jmin (u.selRow0, u.selRow1);     r1 = juce::jmax (u.selRow0, u.selRow1);
}

void PatternGrid::setCursor (int track, int row, int field, bool extend)
{
    auto& u = ctx.ui();
    if (extend)
    {
        if (! u.hasSelection)
        {
            u.hasSelection = true;
            u.selTrack0 = u.track;
            u.selRow0 = u.row;
        }
    }
    else
        u.hasSelection = false;

    u.track = juce::jlimit (0, kNumTracks - 1, track);
    u.row = juce::jlimit (0, ctx.pattern().numRows - 1, row);
    u.field = juce::jlimit (0, kFields - 1, field);
    if (extend)
    {
        u.selTrack1 = u.track;
        u.selRow1 = u.row;
    }
    ctx.proc.seq.setLiveTrack (u.track);
    ensureCursorVisible();
    ctx.uiChanged();
}

void PatternGrid::moveCursor (int dRows, int dFields, bool extend)
{
    auto& u = ctx.ui();
    int t = u.track, f = u.field + dFields;
    while (f < 0)        { if (t == 0) { f = 0; break; } --t; f += kFields; }
    while (f >= kFields) { if (t == kNumTracks - 1) { f = kFields - 1; break; } ++t; f -= kFields; }
    const int n = ctx.pattern().numRows;
    int r = u.row + dRows;
    if (! extend && std::abs (dRows) == 1) r = (r + n) % n; // single steps wrap like a tracker
    setCursor (t, r, f, extend);
}

void PatternGrid::advance()
{
    auto& u = ctx.ui();
    const int n = ctx.pattern().numRows;
    u.row = (u.row + u.editStep) % n;
    ensureCursorVisible();
}

// ---- editing ------------------------------------------------------------------------------------

bool PatternGrid::typeIntoField (juce::juce_wchar ch)
{
    auto& u = ctx.ui();
    const int t = u.track, row = u.row, f = u.field;
    const auto c = ctx.pattern().at (t, row);

    if (f == 0)
    {
        if (isNoteOffKey ((char32_t) ch, u.layout))
        {
            ctx.editPattern ([t, row] (Pattern& p) { auto& x = p.at (t, row); x.note = kNoteOff; x.instrument = kNone; });
            advance();
            return true;
        }
        const int off = pianoKeyOffset ((char32_t) ch, u.layout);
        if (off < 0) return false;
        const int note = (u.octave + 1) * 12 + off;
        if (note > kNoteMax) return true;
        const int ins = c.instrument != kNone ? c.instrument : ctx.song()->tracks[(size_t) t].instrument;
        ctx.editPattern ([t, row, note, ins] (Pattern& p)
        {
            auto& x = p.at (t, row);
            x.note = (uint8_t) note;
            x.instrument = (uint8_t) ins;
        });
        ctx.proc.seq.audition (t, note, ins);
        advance();
        return true;
    }

    if (f == 8)
    {
        const auto up = juce::CharacterFunctions::toUpperCase (ch);
        if (! allowedFx.containsChar (up)) return false;
        ctx.editPattern ([t, row, up] (Pattern& p) { p.at (t, row).fxCmd = (uint8_t) up; });
        advance();
        return true;
    }

    const int d = hexDigitForKey ((char32_t) ch, u.layout);
    if (d < 0) return false;

    if (f >= 5 && f <= 7)
    {
        if (d > 9) return false;
        int v = c.prob == kNone ? 0 : c.prob;
        int digits[3] = { v / 100, (v / 10) % 10, v % 10 };
        digits[f - 5] = d;
        v = juce::jmin (100, digits[0] * 100 + digits[1] * 10 + digits[2]);
        ctx.editPattern ([t, row, v] (Pattern& p) { p.at (t, row).prob = (uint8_t) v; });
        advance();
        return true;
    }

    auto setNibble = [d] (int value, bool hi) { return hi ? (d << 4) | (value & 0x0F) : (value & 0xF0) | d; };
    if (f == 1 || f == 2)
    {
        const int v = juce::jmin (kNumInstruments - 1, setNibble (c.instrument == kNone ? 0 : c.instrument, f == 1));
        ctx.editPattern ([t, row, v] (Pattern& p) { p.at (t, row).instrument = (uint8_t) v; });
    }
    else if (f == 3 || f == 4)
    {
        const int v = juce::jmin (0x7F, setNibble (c.volume == kNone ? 0 : c.volume, f == 3));
        ctx.editPattern ([t, row, v] (Pattern& p) { p.at (t, row).volume = (uint8_t) v; });
    }
    else
    {
        const int v = setNibble (c.fxVal, f == 9);
        ctx.editPattern ([t, row, v] (Pattern& p)
        {
            auto& x = p.at (t, row);
            x.fxVal = (uint8_t) v;
            if (x.fxCmd == 0) x.fxCmd = 'A';
        });
    }
    advance();
    return true;
}

void PatternGrid::clearField (bool whole)
{
    auto& u = ctx.ui();
    if (u.hasSelection)
    {
        int t0, r0, t1, r1;
        normaliseSelection (t0, r0, t1, r1);
        ctx.editPattern ([=] (Pattern& p)
        {
            for (int t = t0; t <= t1; ++t)
                for (int r = r0; r <= juce::jmin (r1, p.numRows - 1); ++r)
                    p.at (t, r) = Cell {};
        });
        return;
    }
    const int t = u.track, row = u.row, col = whole ? -1 : colOfField (u.field);
    ctx.editPattern ([=] (Pattern& p)
    {
        auto& x = p.at (t, row);
        switch (col)
        {
            case -1: x = Cell {}; break;
            case 0:  x.note = kNoteEmpty; x.instrument = kNone; break;
            case 1:  x.instrument = kNone; break;
            case 2:  x.volume = kNone; break;
            case 3:  x.prob = kNone; break;
            default: x.fxCmd = 0; x.fxVal = 0; break;
        }
    });
    advance();
}

void PatternGrid::insertRow (bool remove)
{
    auto& u = ctx.ui();
    int t0 = u.track, t1 = u.track;
    if (u.hasSelection) { int r0, r1; normaliseSelection (t0, r0, t1, r1); }
    const int row = u.row;
    if (remove && row == 0) return;
    ctx.editPattern ([=] (Pattern& p)
    {
        const int n = p.numRows;
        for (int t = t0; t <= t1; ++t)
        {
            if (remove)
            {
                for (int r = row - 1; r < n - 1; ++r) p.at (t, r) = p.at (t, r + 1);
                p.at (t, n - 1) = Cell {};
            }
            else
            {
                for (int r = n - 1; r > row; --r) p.at (t, r) = p.at (t, r - 1);
                p.at (t, row) = Cell {};
            }
        }
    });
    if (remove) u.row = row - 1;
    ensureCursorVisible();
}

void PatternGrid::transpose (int semis)
{
    auto& u = ctx.ui();
    int t0 = u.track, r0 = u.row, t1 = u.track, r1 = u.row;
    if (u.hasSelection) normaliseSelection (t0, r0, t1, r1);
    ctx.editPattern ([=] (Pattern& p)
    {
        for (int t = t0; t <= t1; ++t)
            for (int r = r0; r <= juce::jmin (r1, p.numRows - 1); ++r)
            {
                auto& c = p.at (t, r);
                if (c.hasNote())
                    c.note = (uint8_t) juce::jlimit (12, (int) kNoteMax, c.note + semis);
            }
    });
    const auto& c = ctx.pattern().at (u.track, u.row);
    if (! u.hasSelection && c.hasNote())
        ctx.proc.seq.audition (u.track, c.note, c.instrument != kNone ? c.instrument : ctx.instrumentIndex());
}

void PatternGrid::interpolate()
{
    auto& u = ctx.ui();
    if (! u.hasSelection) return;
    int t0, r0, t1, r1;
    normaliseSelection (t0, r0, t1, r1);
    if (r1 <= r0) return;
    const int col = colOfField (u.field);
    if (col < 2) return;
    ctx.editPattern ([=] (Pattern& p)
    {
        for (int t = t0; t <= t1; ++t)
        {
            auto& a = p.at (t, r0);
            auto& b = p.at (t, r1);
            auto get = [col] (const Cell& c) { return col == 2 ? (c.volume == kNone ? 0x64 : c.volume) : col == 3 ? (c.prob == kNone ? 100 : c.prob) : c.fxVal; };
            const int va = get (a), vb = get (b);
            const uint8_t cmd = a.fxCmd ? a.fxCmd : b.fxCmd;
            for (int r = r0; r <= r1; ++r)
            {
                const int v = juce::roundToInt (va + (vb - va) * (float) (r - r0) / (float) (r1 - r0));
                auto& c = p.at (t, r);
                if (col == 2)      c.volume = (uint8_t) juce::jlimit (0, 0x7F, v);
                else if (col == 3) c.prob = (uint8_t) juce::jlimit (0, 100, v);
                else if (cmd)      { c.fxCmd = cmd; c.fxVal = (uint8_t) juce::jlimit (0, 255, v); }
            }
        }
    });
}

void PatternGrid::copySelection (bool cut)
{
    auto& u = ctx.ui();
    int t0 = u.track, r0 = u.row, t1 = u.track, r1 = u.row;
    if (u.hasSelection) normaliseSelection (t0, r0, t1, r1);
    const auto& p = ctx.pattern();
    r1 = juce::jmin (r1, p.numRows - 1);
    auto& clip = ctx.proc.clipboard;
    clip.tracks = t1 - t0 + 1;
    clip.rows = r1 - r0 + 1;
    clip.cells.clear();
    for (int t = t0; t <= t1; ++t)
        for (int r = r0; r <= r1; ++r)
            clip.cells.push_back (p.at (t, r));
    if (cut) clearField (true);
}

void PatternGrid::paste()
{
    auto& u = ctx.ui();
    const auto clip = ctx.proc.clipboard;
    if (clip.cells.empty()) return;
    const int t0 = u.track, r0 = u.row;
    ctx.editPattern ([=] (Pattern& p)
    {
        for (int dt = 0; dt < clip.tracks; ++dt)
            for (int dr = 0; dr < clip.rows; ++dr)
            {
                const int t = t0 + dt, r = r0 + dr;
                if (t < kNumTracks && r < p.numRows)
                    p.at (t, r) = clip.cells[(size_t) (dt * clip.rows + dr)];
            }
    });
}

void PatternGrid::selectAllTrack()
{
    auto& u = ctx.ui();
    const int n = ctx.pattern().numRows;
    const bool wholeTrack = u.hasSelection && u.selTrack0 == u.selTrack1 && juce::jmin (u.selRow0, u.selRow1) == 0
                            && juce::jmax (u.selRow0, u.selRow1) == n - 1;
    u.hasSelection = true;
    u.selRow0 = 0; u.selRow1 = n - 1;
    u.selTrack0 = wholeTrack ? 0 : u.track;
    u.selTrack1 = wholeTrack ? kNumTracks - 1 : u.track;
    ctx.uiChanged();
}

// ---- keyboard -------------------------------------------------------------------------------------

bool PatternGrid::keyPressed (const juce::KeyPress& k)
{
    auto& u = ctx.ui();
    const auto mods = k.getModifiers();
    const bool ctrl = mods.isCommandDown(), shift = mods.isShiftDown(), alt = mods.isAltDown();
    const int code = k.getKeyCode();
    const int page = 16;

    auto done = [this] { ensureCursorVisible(); ctx.uiChanged(); return true; };

    if (ctrl && (code == 'Z' || code == 'z')) { shift ? ctx.doc().redo() : ctx.doc().undo(); return done(); }
    if (ctrl && (code == 'Y' || code == 'y')) { ctx.doc().redo(); return done(); }
    if (ctrl && (code == 'C' || code == 'c')) { copySelection (false); return true; }
    if (ctrl && (code == 'X' || code == 'x')) { copySelection (true); return done(); }
    if (ctrl && (code == 'V' || code == 'v')) { paste(); return done(); }
    if (ctrl && (code == 'A' || code == 'a')) { selectAllTrack(); return true; }
    if (ctrl && (code == 'I' || code == 'i')) { interpolate(); return done(); }

    if (code == juce::KeyPress::upKey)
    {
        if (ctrl) { transpose (shift ? 12 : 1); return done(); }
        moveCursor (alt ? -page : -1, 0, shift); return true;
    }
    if (code == juce::KeyPress::downKey)
    {
        if (ctrl) { transpose (shift ? -12 : -1); return done(); }
        moveCursor (alt ? page : 1, 0, shift); return true;
    }
    if (code == juce::KeyPress::leftKey)   { moveCursor (0, ctrl ? -kFields : -1, shift); return true; }
    if (code == juce::KeyPress::rightKey)  { moveCursor (0, ctrl ? kFields : 1, shift); return true; }
    if (code == juce::KeyPress::pageUpKey)   { moveCursor (-page, 0, shift); return true; }
    if (code == juce::KeyPress::pageDownKey) { moveCursor (page, 0, shift); return true; }
    if (code == juce::KeyPress::homeKey)   { setCursor (u.track, 0, u.field, shift); return true; }
    if (code == juce::KeyPress::endKey)    { setCursor (u.track, ctx.pattern().numRows - 1, u.field, shift); return true; }
    if (code == juce::KeyPress::tabKey)
    {
        setCursor ((u.track + (shift ? kNumTracks - 1 : 1)) % kNumTracks, u.row, 0, false);
        return true;
    }
    if (code == juce::KeyPress::escapeKey) { u.hasSelection = false; return done(); }
    if (code == juce::KeyPress::deleteKey) { clearField (shift); return done(); }
    if (code == juce::KeyPress::insertKey) { insertRow (false); return done(); }
    if (code == juce::KeyPress::backspaceKey) { insertRow (true); return done(); }
    if (code == juce::KeyPress::spaceKey)
    {
        ctx.proc.seq.setPreviewPlaying (! ctx.proc.seq.isPreviewPlaying());
        return done();
    }
    for (int i = 0; i <= 8; ++i)
        if (code == juce::KeyPress::F1Key + i)
        {
            u.octave = juce::jlimit (0, 8, i + 1);
            return done();
        }
    if (code == juce::KeyPress::numberPadMultiply || code == juce::KeyPress::numberPadAdd) { u.octave = juce::jmin (8, u.octave + 1); return done(); }
    if (code == juce::KeyPress::numberPadDivide || code == juce::KeyPress::numberPadSubtract) { u.octave = juce::jmax (0, u.octave - 1); return done(); }

    if (ctrl || alt)
        return false;

    const auto ch = k.getTextCharacter();
    if (ch != 0 && typeIntoField (ch))
        return done();

    // swallow plain printable keys so the host does not react to typing in the grid
    return ch >= 32;
}

// ---- mouse ----------------------------------------------------------------------------------------

void PatternGrid::mouseMove (const juce::MouseEvent& e)
{
    juce::String tip;
    if (e.position.y < kHeaderH && e.position.x >= kNumW)
    {
        const float lx = std::fmod (e.position.x - kNumW, (float) kTrackW);
        if (e.position.y > 40) tip = "Longueur de la piste : plus courte que le pattern = polyrythmie";
        else if (lx > kTrackW - 60) tip = U ("Sortie : synthé interne ou canal MIDI");
        else if (lx > kTrackW - 100) tip = U ("M : muet · S : solo");
        else tip = "Double-clic pour renommer";
    }
    else if (e.position.x < kNumW && e.position.y >= kHeaderH)
        tip = "Clic droit : longueur du pattern";
    setTooltip (tip);
}

void PatternGrid::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    dragging = false;
    auto song = ctx.song();

    if (e.position.y < kHeaderH && e.position.x >= kNumW)
    {
        const int vt = (int) ((e.position.x - kNumW) / kTrackW);
        if (vt >= visibleTracks()) return;
        const int t = trackScroll + vt;
        const float x = (float) (kNumW + vt * kTrackW);
        const auto& tr = song->tracks[(size_t) t];
        const juce::String out = tr.output == OutputKind::Internal ? "Int" : "MIDI " + juce::String (tr.midiChannel);
        const float pw = textWidth (mono (10.0f), out) + 14.0f;
        auto pill = juce::Rectangle<float> (x + kTrackW - kPad - pw, 14.0f, pw, 18.0f);
        auto sR = juce::Rectangle<float> (pill.getX() - 20.0f, 15.0f, 16.0f, 16.0f);
        auto mR = sR.translated (-19.0f, 0.0f);
        const auto p = e.position;

        if (pill.expanded (2).contains (p))      headerMenu (t, pill, 0);
        else if (mR.expanded (2).contains (p))   ctx.doc().modify ([t] (Song& s) { s.tracks[(size_t) t].mute = ! s.tracks[(size_t) t].mute; });
        else if (sR.expanded (2).contains (p))   ctx.doc().modify ([t] (Song& s) { s.tracks[(size_t) t].solo = ! s.tracks[(size_t) t].solo; });
        else if (p.y > 40)                       headerMenu (t, { x, 40.0f, (float) kTrackW, 20.0f }, 1);
        else                                     setCursor (t, ctx.ui().row, 0, false);
        return;
    }

    if (e.position.x < kNumW && e.position.y >= kHeaderH && e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Longueur du pattern");
        for (int n : { 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256 })
            m.addItem (n, juce::String (n) + " lignes", true, ctx.pattern().numRows == n);
        m.showMenuAsync ({}, [this] (int r) { if (r > 0) ctx.editPattern ([r] (Pattern& p) { p.numRows = r; }); ensureCursorVisible(); });
        return;
    }

    int t, row, f;
    if (hitCell (e.position, t, row, f))
        setCursor (t, row, f, e.mods.isShiftDown());
}

void PatternGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mouseDownPosition.y < kHeaderH) return;
    int t, row, f;
    if (hitCell (e.position, t, row, f))
    {
        auto& u = ctx.ui();
        if (! dragging)
        {
            dragging = true;
            u.hasSelection = true;
            u.selTrack0 = u.track;
            u.selRow0 = u.row;
        }
        u.selTrack1 = t;
        u.selRow1 = row;
        u.track = t;
        u.row = row;
        ensureCursorVisible();
        ctx.uiChanged();
    }
}

void PatternGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.position.y < 40 && e.position.x >= kNumW)
    {
        const int vt = (int) ((e.position.x - kNumW) / kTrackW);
        if (vt < visibleTracks())
            renameTrack (trackScroll + vt, { (float) (kNumW + vt * kTrackW) + kPad - 4.0f, 11.0f, 90.0f, 24.0f });
    }
}

void PatternGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        trackScroll = juce::jlimit (0, juce::jmax (0, kNumTracks - visibleTracks()), trackScroll + (d < 0 ? 1 : -1));
    }
    else
    {
        rowScroll = juce::jlimit (0, juce::jmax (0, ctx.pattern().numRows - visibleRows()), rowScroll + (w.deltaY < 0 ? 3 : -3));
    }
    repaint();
}

void PatternGrid::headerMenu (int t, juce::Rectangle<float> area, int which)
{
    auto song = ctx.song();
    const auto& tr = song->tracks[(size_t) t];
    juce::PopupMenu m;
    if (which == 0)
    {
        m.addSectionHeader ("Sortie de " + juce::String (tr.name));
        m.addItem (1, U ("Synthé interne"), true, tr.output == OutputKind::Internal);
        juce::PopupMenu midi;
        for (int ch = 1; ch <= 16; ++ch)
            midi.addItem (100 + ch, "Canal " + juce::String (ch), true, tr.output == OutputKind::Midi && tr.midiChannel == ch);
        m.addSubMenu ("Sortie MIDI", midi);
        m.addSeparator();
        juce::PopupMenu ins;
        for (int i = 0; i < kNumInstruments; ++i)
            ins.addItem (200 + i, hex2 (i) + "  " + song->instruments[(size_t) i].name, true, tr.instrument == i);
        m.addSubMenu (U ("Instrument par défaut"), ins);
    }
    else
    {
        const int n = ctx.pattern().numRows;
        m.addSectionHeader ("Longueur de " + juce::String (tr.name));
        m.addItem (300, "Comme le pattern (" + juce::String (n) + ")", true, tr.length == 0 || tr.length >= n);
        for (int len : { 3, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 20, 24, 28, 48 })
            if (len < n)
                m.addItem (300 + len, juce::String (len) + " lignes", true, tr.length == len);
    }

    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (area.toNearestInt())),
                     [this, t] (int r)
                     {
                         if (r == 0) return;
                         ctx.doc().modify ([t, r] (Song& s)
                         {
                             auto& x = s.tracks[(size_t) t];
                             if (r == 1)            x.output = OutputKind::Internal;
                             else if (r > 100 && r <= 116) { x.output = OutputKind::Midi; x.midiChannel = r - 100; }
                             else if (r >= 200 && r < 300) x.instrument = r - 200;
                             else if (r >= 300) x.length = r - 300;
                         });
                     });
}

void PatternGrid::renameTrack (int t, juce::Rectangle<float> area)
{
    nameEditor = std::make_unique<juce::TextEditor>();
    nameEditor->setFont (sans (13.0f, 600));
    nameEditor->setText (ctx.song()->tracks[(size_t) t].name, false);
    nameEditor->setBounds (area.toNearestInt());
    nameEditor->setIndents (4, 3);
    addAndMakeVisible (*nameEditor);
    nameEditor->selectAll();
    nameEditor->grabKeyboardFocus();

    auto doneFlag = std::make_shared<bool> (false);
    auto finish = [this, t, doneFlag] (bool commit)
    {
        if (*doneFlag || nameEditor == nullptr) return;
        *doneFlag = true;
        const auto name = nameEditor->getText().trim().substring (0, 16).toStdString();
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PatternGrid> (this)]
        {
            if (safe == nullptr) return;
            safe->nameEditor.reset();
            safe->grabKeyboardFocus();
        });
        if (commit && ! name.empty())
            ctx.doc().modify ([t, name] (Song& s) { s.tracks[(size_t) t].name = name; });
    };
    nameEditor->onReturnKey = [finish] { finish (true); };
    nameEditor->onEscapeKey = [finish] { finish (false); };
    nameEditor->onFocusLost = [finish] { finish (true); };
}

} // namespace lattice
