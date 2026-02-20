#pragma once

#include <functional>
#include <vector>
#include <memory>

#include "note_model.h"

namespace nv {

using SearchResultCallback = std::function<void(const std::vector<std::shared_ptr<Note>>&)>;
using NoteSelectionCallback = std::function<void(std::shared_ptr<Note>)>;

class ISearchController {
public:
    virtual ~ISearchController() = default;
    virtual void setSearchQuery(const std::string& query) = 0;
    virtual void selectNote(size_t index) = 0;
    virtual void createNewNote(const std::string& title, const std::string& body) = 0;
    virtual std::vector<std::shared_ptr<Note>> getFilteredNotes() = 0;
    virtual void addSearchObserver(SearchResultCallback cb) = 0;
    virtual void addSelectionObserver(NoteSelectionCallback cb) = 0;
};

} // namespace nv
