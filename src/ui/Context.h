#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/Theme.h"

namespace lattice
{

/** What every panel of the editor shares. */
struct Context
{
    LatticeProcessor& proc;
    std::function<void()> uiChanged; // call after changing proc.ui so every panel repaints

    Document& doc() { return proc.doc; }
    UiState& ui() { return proc.ui; }
    std::shared_ptr<const Song> song() const { return proc.doc.song(); }

    int patternIndex() const
    {
        auto s = song();
        if (s->order.empty()) return 0;
        return s->order[(size_t) juce::jlimit (0, s->numOrders() - 1, s->selectedOrder)];
    }

    const Pattern& pattern() const
    {
        auto s = song();
        return *s->patterns[(size_t) juce::jlimit (0, (int) s->patterns.size() - 1, patternIndex())];
        // the document keeps the pattern alive until the next edit
    }

    void editPattern (const std::function<void (Pattern&)>& fn, const std::string& key = {})
    {
        doc().modifyPattern (patternIndex(), fn, key);
    }

    /** Current instrument of the cursor track. */
    int instrumentIndex() const { return song()->tracks[(size_t) proc.ui.track].instrument; }
};

/** Base for editor panels: knows the context and repaints on any change. */
class Panel : public juce::Component
{
public:
    explicit Panel (Context& c) : ctx (c) {}
    virtual void refresh() { repaint(); }

protected:
    Context& ctx;
};

} // namespace lattice
