#pragma once

#include "ui/Context.h"

namespace lattice
{

/** The tracker grid: track headers, rows, cursor, keyboard entry, selection and clipboard. */
class PatternGrid : public Panel, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PatternGrid (Context&);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void focusGained (FocusChangeType) override { ctx.uiChanged(); }
    void focusLost (FocusChangeType) override { ctx.uiChanged(); }

    static constexpr int kHeaderH = 66, kRowH = 19, kNumW = 38, kTrackW = 148, kFields = 11;

private:
    void timerCallback() override;

    // geometry
    int visibleTracks() const;
    int visibleRows() const;
    float charW() const;
    float fieldX (int field) const;   // x offset of a cursor field inside a cell
    float fieldW (int field) const;
    int fieldAt (float xInCell) const;
    juce::Rectangle<float> cellRect (int visTrack, int visRow) const;
    bool hitCell (juce::Point<float>, int& track, int& row, int& field) const;
    void ensureCursorVisible();
    int playRowFor (int track) const;  // -1 when this pattern is not playing

    // editing
    void moveCursor (int dRows, int dFields, bool extendSelection);
    void setCursor (int track, int row, int field, bool extendSelection);
    void advance();
    bool typeIntoField (juce::juce_wchar c);
    void clearField (bool wholeCell);
    void insertRow (bool remove);
    void transpose (int semis);
    void interpolate();
    void copySelection (bool cut);
    void paste();
    void selectAllTrack();
    void normaliseSelection (int& t0, int& r0, int& t1, int& r1) const;
    void headerMenu (int track, juce::Rectangle<float> area, int which);
    void renameTrack (int track, juce::Rectangle<float> area);

    int trackScroll = 0, rowScroll = 0;
    int lastPlayRow = -1;
    bool dragging = false;
    std::unique_ptr<juce::TextEditor> nameEditor;
};

} // namespace lattice
