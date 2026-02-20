#include "nv/note_store.h"

#include <algorithm>

namespace nv {

void NoteStore::addObserver(NoteStoreObserver* obs) {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.push_back(obs);
}

void NoteStore::removeObserver(NoteStoreObserver* obs) {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.erase(std::remove(observers_.begin(), observers_.end(), obs), observers_.end());
}

void NoteStore::addNote(std::shared_ptr<Note> note) {
    std::lock_guard<std::mutex> lock(mutex_);
    notes_[note->uuid()] = note;
    
    for (auto* obs : observers_) {
        obs->onNoteAdded(note);
    }
}

void NoteStore::updateNote(std::shared_ptr<Note> note) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = notes_.find(note->uuid());
    if (it == notes_.end()) {
        return;
    }
    
    it->second = note;
    
    for (auto* obs : observers_) {
        obs->onNoteUpdated(note);
    }
}

void NoteStore::deleteNote(const NoteUUID& uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = notes_.find(uuid);
    if (it == notes_.end()) {
        return;
    }
    
    notes_.erase(it);
    
    for (auto* obs : observers_) {
        obs->onNoteDeleted(uuid);
    }
}

std::shared_ptr<Note> NoteStore::getNote(const NoteUUID& uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = notes_.find(uuid);
    if (it == notes_.end()) {
        return nullptr;
    }
    
    return it->second;
}

std::vector<std::shared_ptr<Note>> NoteStore::getAllNotes() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<Note>> result;
    for (const auto& pair : notes_) {
        result.push_back(pair.second);
    }
    
    return result;
}

} // namespace nv
