#pragma once

#include "core/Model.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace lattice
{

/**
    Owns the song on the message thread and hands immutable snapshots to the audio thread.

    Every edit copies the Song (cheap: patterns are shared) and only the patterns it touches,
    then publishes the new snapshot atomically. Old snapshots live on in the undo history, so
    the audio thread is never the one freeing a song.
*/
class Document
{
public:
    Document();

    std::shared_ptr<const Song> song() const noexcept { return current; }
    std::shared_ptr<const Song> audioSnapshot() const noexcept { return published.load (std::memory_order_acquire); }

    /** Applies an edit. Edits sharing a non-empty coalesce key merge into one undo step. */
    void modify (const std::function<void (Song&)>& edit, const std::string& coalesceKey = {});

    /** Edits one pattern (copy-on-write). */
    void modifyPattern (int patternIndex, const std::function<void (Pattern&)>& edit, const std::string& coalesceKey = {});

    /** An edit that is not worth an undo step (selection, view state). */
    void modifyQuiet (const std::function<void (Song&)>& edit);

    /** Replaces everything (state load). Clears history. */
    void replace (std::shared_ptr<Song> s);

    bool undo();
    bool redo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }

    std::function<void()> onChange;

private:
    void publish (std::shared_ptr<const Song> s);

    std::shared_ptr<const Song> current;
    std::atomic<std::shared_ptr<const Song>> published;
    std::vector<std::shared_ptr<const Song>> undoStack, redoStack, graveyard;
    std::string lastKey;
};

} // namespace lattice
