#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

#include "note_model.h"

namespace nv {

class SearchIndex {
public:
    void indexNote(std::shared_ptr<Note> note);
    void removeNote(const NoteUUID& uuid);
    void updateNote(std::shared_ptr<Note> note);
    std::vector<std::shared_ptr<Note>> filter(const std::string& query) const;
    void clear();
    static std::string generateUUID();
    
private:
    void indexNoteInternal(std::shared_ptr<Note> note);
    void removeNoteInternal(const NoteUUID& uuid);
    std::vector<std::string> tokenize(const std::string& s) const;
    std::unordered_map<std::string, std::vector<Note*>> terms_to_notes_;
    std::unordered_map<Note*, std::vector<std::string>> note_to_terms_;
    std::vector<std::shared_ptr<Note>> all_notes_;
    mutable std::mutex mutex_;
};

class QueryParser {
public:
    static std::vector<std::string> tokenize(const std::string& s);
};

} // namespace nv
