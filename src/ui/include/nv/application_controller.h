#pragma once

#include <QObject>
#include <functional>
#include <vector>
#include <optional>

#include "nv/interfaces.h"
#include "nv/note_model.h"
#include "nv/main_window.h"
#include "nv/search_index.h"
#include "nv/note_store.h"
#include "nv/storage.h"
#include "nv/search_field.h"
#include "nv/note_list.h"
#include "nv/note_editor.h"
#include "nv/webdav_sync_manager.h"

namespace nv {

using NoteSelectionCallback = std::function<void(std::shared_ptr<Note>)>;

class ApplicationController : public QObject, public ISearchController, public NoteStoreObserver {
    Q_OBJECT

signals:
    void searchResultsUpdated(const std::vector<std::shared_ptr<Note>>& notes);
    void noteSelectedSignal(std::shared_ptr<Note> note);
    void renameNoteRequested(std::shared_ptr<Note> note);

public:
    explicit ApplicationController(MainWindow* win, INoteStore* store, IStorage* storage, QObject* parent = nullptr);
    ~ApplicationController();
    
    void setSearchQuery(const std::string& query) override;
    void selectNote(size_t index) override;
    void createNewNote(const std::string& title, const std::string& body) override;
    std::vector<std::shared_ptr<Note>> getFilteredNotes() override;
    void addSearchObserver(SearchResultCallback cb) override;
    void addSelectionObserver(NoteSelectionCallback cb) override;
    void setWebDAVSyncManager(WebDAVSyncManager* manager);

    // NoteStoreObserver implementation
    void onNoteAdded(std::shared_ptr<Note> note) override;
    void onNoteUpdated(std::shared_ptr<Note> note) override;
    void onNoteDeleted(const NoteUUID& uuid) override;

    // Keyboard shortcuts
    void focusSearch();
    void renameNote(std::shared_ptr<Note> note);
    void setupShortcuts();

private slots:
    void onSearchFieldSubmitted(const QString& text);
    void onNoteListSelected(std::shared_ptr<Note> note);
    void onEditorTextChanged();
    void onNewNoteRequested(const QString& text);
    void focusEditorAtEnd();
    void onNoteDoubleClicked(std::shared_ptr<Note> note);
    void onLayoutModeChanged(int mode);
    void onThemeChanged(int theme);
    void updateUIFromFilteredNotesWithTheme();

private:
    void updateSearchResults(const std::string& query);
    void updateUIFromState();
    void updateUIFromFilteredNotes();
    
    MainWindow* win_;
    INoteStore* store_;
    IStorage* storage_;
    std::unique_ptr<SearchIndex> search_index_;
    std::string active_query_;
    std::vector<std::shared_ptr<Note>> filtered_notes_;
    std::optional<size_t> selected_index_;
    std::optional<std::shared_ptr<Note>> pending_note_;
    std::vector<SearchResultCallback> search_observers_;
    std::vector<NoteSelectionCallback> selection_observers_;
    WebDAVSyncManager* webdav_manager_;  // Not owned by this class
};

} // namespace nv
