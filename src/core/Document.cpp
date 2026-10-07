#include "core/Document.h"

namespace lattice
{

namespace { constexpr size_t kMaxUndo = 256; }

Document::Document()
{
    std::shared_ptr<const Song> s = Song::createDefault();
    current = s;
    published.store (s);
}

void Document::publish (std::shared_ptr<const Song> s)
{
    current = s;
    published.store (std::move (s), std::memory_order_release);
    if (onChange)
        onChange();
}

void Document::modify (const std::function<void (Song&)>& edit, const std::string& key)
{
    auto copy = std::make_shared<Song> (*current);
    edit (*copy);

    const bool merge = ! key.empty() && key == lastKey && ! undoStack.empty();
    if (! merge)
    {
        undoStack.push_back (current);
        if (undoStack.size() > kMaxUndo)
            undoStack.erase (undoStack.begin());
    }
    else
    {
        graveyard.push_back (current);
        if (graveyard.size() > 16)
            graveyard.erase (graveyard.begin());
    }
    redoStack.clear();
    lastKey = key;
    publish (std::move (copy));
}

void Document::modifyQuiet (const std::function<void (Song&)>& edit)
{
    auto copy = std::make_shared<Song> (*current);
    edit (*copy);
    // keep the replaced snapshot alive a little longer so the audio thread never frees it
    graveyard.push_back (current);
    if (graveyard.size() > 16)
        graveyard.erase (graveyard.begin());
    publish (std::move (copy));
}

void Document::modifyPattern (int patternIndex, const std::function<void (Pattern&)>& edit, const std::string& key)
{
    if (patternIndex < 0 || patternIndex >= (int) current->patterns.size())
        return;
    modify ([&] (Song& s)
    {
        auto p = std::make_shared<Pattern> (*s.patterns[(size_t) patternIndex]);
        edit (*p);
        s.patterns[(size_t) patternIndex] = std::move (p);
    }, key);
}

void Document::replace (std::shared_ptr<Song> s)
{
    undoStack.clear();
    redoStack.clear();
    lastKey.clear();
    publish (std::move (s));
}

bool Document::undo()
{
    if (undoStack.empty())
        return false;
    redoStack.push_back (current);
    auto s = undoStack.back();
    undoStack.pop_back();
    lastKey.clear();
    publish (std::move (s));
    return true;
}

bool Document::redo()
{
    if (redoStack.empty())
        return false;
    undoStack.push_back (current);
    auto s = redoStack.back();
    redoStack.pop_back();
    lastKey.clear();
    publish (std::move (s));
    return true;
}

} // namespace lattice
