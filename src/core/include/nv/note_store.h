#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

#include "note_model.h"

namespace nv {

class NoteStoreObserver {
public:
    virtual void onNoteAdded(std::shared_ptr<Note> note) = 0;
    virtual void onNoteUpdated(std::shared_ptr<Note> note) = 0;
    virtual void onNoteDeleted(const NoteUUID& uuid) = 0;
};

class INoteStore {
public:
    virtual ~INoteStore() = default;
    virtual void addObserver(NoteStoreObserver* obs) = 0;
    virtual void removeObserver(NoteStoreObserver* obs) = 0;
    virtual void addNote(std::shared_ptr<Note> note) = 0;
    virtual void updateNote(std::shared_ptr<Note> note) = 0;
    virtual void deleteNote(const NoteUUID& uuid) = 0;
    virtual std::shared_ptr<Note> getNote(const NoteUUID& uuid) = 0;
    virtual std::vector<std::shared_ptr<Note>> getAllNotes() = 0;
};

class NoteStore : public INoteStore {
public:
    void addObserver(NoteStoreObserver* obs) override;
    void removeObserver(NoteStoreObserver* obs) override;
    void addNote(std::shared_ptr<Note> note) override;
    void updateNote(std::shared_ptr<Note> note) override;
    void deleteNote(const NoteUUID& uuid) override;
    std::shared_ptr<Note> getNote(const NoteUUID& uuid) override;
    std::vector<std::shared_ptr<Note>> getAllNotes() override;
    
private:
    std::unordered_map<NoteUUID, std::shared_ptr<Note>> notes_;
    std::vector<NoteStoreObserver*> observers_;
    mutable std::mutex mutex_;
};

} // namespace nv
