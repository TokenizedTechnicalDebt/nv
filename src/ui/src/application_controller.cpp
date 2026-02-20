#include "nv/application_controller.h"
#include <QTimer>
#include <QThreadPool>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QTextCursor>
#include <QAction>
#include <QMenuBar>
#include <QSettings>
#include "nv/main_window.h"
#include "nv/app_state.h"

namespace nv {

ApplicationController::ApplicationController(MainWindow* win, INoteStore* store, IStorage* storage, QObject* parent)
    : QObject(parent)
    , win_(win)
    , store_(store)
    , storage_(storage)
    , search_index_(std::make_unique<SearchIndex>())
    , webdav_manager_(nullptr) {
    
    // Register as observer for note store changes
    store_->addObserver(this);
    
    // Create editor with store and storage
    NoteEditor* editor = new NoteEditor(store_, storage_);
    win_->setNoteEditor(editor);
    
    // Menu bars are set up by platform-specific code in main.cpp
    // No need to setup menus here again to avoid duplicates
    
    // Connect UI signals
    connect(win_->searchField(), &SearchField::querySubmitted, this, &ApplicationController::onSearchFieldSubmitted);
    connect(win_->searchField(), &SearchField::searchStopped, [this]() {
        // Trigger WebDAV sync when user stops typing
        if (webdav_manager_) {
            webdav_manager_->triggerSyncOnSearch();
        }
    });
    connect(win_->noteList(), &NoteList::noteSelected, this, &ApplicationController::onNoteListSelected);
    connect(win_->noteList(), &NoteList::enterPressed, this, &ApplicationController::focusEditorAtEnd);
    connect(win_->noteList(), &NoteList::noteDoubleClicked, this, &ApplicationController::onNoteDoubleClicked);
    
    // Connect layout and theme change signals
    connect(win_, &MainWindow::layoutModeChanged, this, &ApplicationController::onLayoutModeChanged);
    connect(win_, &MainWindow::themeChanged, this, &ApplicationController::onThemeChanged);
    
    // Setup keyboard shortcuts
    setupShortcuts();
    
    // Apply saved theme
    win_->setTheme(ApplicationState::instance().theme());
    
    // Apply saved layout mode
    win_->setLayoutMode(ApplicationState::instance().layoutMode());
    
    connect(win_->noteEditor(), &NoteEditor::textChangedForSave, [this]() {
        // If editor has text but no current note, create a new note
        if (!win_->noteEditor()->toPlainText().isEmpty() && !win_->noteEditor()->getNote()) {
            std::string text = win_->noteEditor()->toPlainText().toStdString();
            size_t newlinePos = text.find('\n');
            std::string title = (newlinePos == std::string::npos) ? text : text.substr(0, newlinePos);
            std::string body = (newlinePos == std::string::npos) ? "" : text.substr(newlinePos + 1);
            createNewNote(title, body);
        }
    });
    
    // Connect new note requested signal from search field
    connect(win_->searchField(), &SearchField::newNoteRequested, this, &ApplicationController::onNewNoteRequested);
    
    // Load all notes from storage and add to store
    auto notesResult = storage_->readAllNotes();
    if (nv::isSuccess(notesResult)) {
        auto allNotes = nv::getSuccess(notesResult);
        for (const auto& note : allNotes) {
            store_->addNote(note);
        }
    } else {
        qWarning() << "Failed to load notes from storage";
    }
    
    // Update UI with all notes (empty query shows all)
    updateSearchResults("");
}

ApplicationController::~ApplicationController() {
    if (store_) {
        store_->removeObserver(this);
    }
}

void ApplicationController::setupShortcuts() {
    // Ctrl/Cmd + L - focus search
    QAction* focusSearchAction = new QAction(this);
    focusSearchAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(focusSearchAction, &QAction::triggered, this, &ApplicationController::focusSearch);
    win_->addAction(focusSearchAction);
    
    // Ctrl/Cmd + R - rename note
    QAction* renameNoteAction = new QAction(this);
    renameNoteAction->setShortcut(QKeySequence("Ctrl+R"));
    connect(renameNoteAction, &QAction::triggered, this, [this]() {
        if (selected_index_ && *selected_index_ < filtered_notes_.size()) {
            renameNote(filtered_notes_[*selected_index_]);
        }
    });
    win_->addAction(renameNoteAction);
}

void ApplicationController::focusSearch() {
    if (win_->searchField()) {
        win_->searchField()->setFocus(Qt::OtherFocusReason);
    }
}

void ApplicationController::renameNote(std::shared_ptr<Note> note) {
    if (!note) return;
    
    NoteListModel* model = qobject_cast<NoteListModel*>(win_->noteList()->model());
    if (!model) return;
    
    // Find the note in the model's rows (respects sorting)
    int row = model->rowForNote(note);
    if (row >= 0) {
        selected_index_ = row;
        win_->noteList()->setCurrentIndex(model->index(row, 0));
        // Start editing the title
        win_->noteList()->edit(model->index(row, 0));
    }
}

void ApplicationController::onNoteDoubleClicked(std::shared_ptr<Note> note) {
    if (!note) {
        return;
    }

    // Defer entering edit mode until the double-click event sequence finishes.
    // If we call edit() synchronously during mouse double-click handling,
    // the subsequent mouse release can immediately close the editor.
    QTimer::singleShot(0, this, [this, note]() {
        renameNote(note);
    });
}

void ApplicationController::onLayoutModeChanged(int mode) {
    // Layout mode is already handled in MainWindow
}

void ApplicationController::onThemeChanged(int theme) {
    // Theme is already handled in MainWindow
}

void ApplicationController::setSearchQuery(const std::string& query) {
    active_query_ = query;
    
    // Run search in background
    auto* watcher = new QFutureWatcher<std::vector<std::shared_ptr<Note>>>(this);
    connect(watcher, &QFutureWatcher<std::vector<std::shared_ptr<Note>>>::finished, this, [this, watcher]() {
        filtered_notes_ = watcher->result();
        
        // Update UI on main thread
        updateUIFromFilteredNotes();
        
        // Clear selection if no results
        if (filtered_notes_.empty()) {
            selected_index_.reset();
        } else if (!selected_index_ || *selected_index_ >= filtered_notes_.size()) {
            selected_index_ = 0;
            NoteListModel* model = qobject_cast<NoteListModel*>(win_->noteList()->model());
            if (model) {
                win_->noteList()->setCurrentIndex(model->index(0, 0));
            }
        }
        
        // Notify observers
        for (auto& cb : search_observers_) {
            cb(filtered_notes_);
        }
        
        // Clean up watcher
        watcher->deleteLater();
    });
    
    // Run search in background
    QFuture<std::vector<std::shared_ptr<Note>>> future = QtConcurrent::run([this, query]() {
        return search_index_->filter(query);
    });
    watcher->setFuture(future);
}

void ApplicationController::selectNote(size_t index) {
    if (index >= filtered_notes_.size()) return;
    
    selected_index_ = index;
    auto note = filtered_notes_[index];
    
    // Set editor content
    win_->noteEditor()->setNote(note);
    
    // Clear pending note if exists
    pending_note_.reset();
    
    // Notify observers
    for (auto& cb : selection_observers_) {
        cb(note);
    }
}

void ApplicationController::createNewNote(const std::string& title, const std::string& body) {
    auto note = std::make_shared<Note>(
        SearchIndex::generateUUID(),
        title,
        body,
        std::chrono::system_clock::now(),
        std::chrono::system_clock::now()
    );
    
    // Add to store (this will trigger onNoteAdded which updates filtered_notes_ and UI)
    store_->addNote(note);
    // Also add to search index directly (onNoteAdded will re-index it, but this is faster)
    search_index_->indexNote(note);
    
    // Re-filter based on current query to ensure we have the latest filtered list
    filtered_notes_ = search_index_->filter(active_query_);
    
    // Set editor content
    win_->noteEditor()->setNote(note);
    
    // Write new note to disk immediately
    auto saveResult = storage_->writeNote(*note);
    if (!nv::isSuccess(saveResult)) {
        qWarning() << "Failed to save new note to disk";
    }
    
    // Clear pending note
    pending_note_.reset();
    
    // Notify observers
    for (auto& cb : search_observers_) {
        cb(filtered_notes_);
    }
    
    // Update UI to show the new note
    updateUIFromFilteredNotes();
}

std::vector<std::shared_ptr<Note>> ApplicationController::getFilteredNotes() {
    return filtered_notes_;
}

void ApplicationController::addSearchObserver(SearchResultCallback cb) {
    search_observers_.push_back(cb);
}

void ApplicationController::addSelectionObserver(NoteSelectionCallback cb) {
    selection_observers_.push_back(cb);
}

void ApplicationController::onSearchFieldSubmitted(const QString& text) {
    std::string query = text.toStdString();
    setSearchQuery(query);
}

void ApplicationController::onNoteListSelected(std::shared_ptr<Note> note) {
    // Find the note in filtered_notes_ to get its index
    for (size_t i = 0; i < filtered_notes_.size(); ++i) {
        if (filtered_notes_[i] == note || filtered_notes_[i]->uuid() == note->uuid()) {
            selectNote(i);
            return;
        }
    }
}

void ApplicationController::focusEditorAtEnd() {
    if (win_->noteEditor()) {
        win_->noteEditor()->setFocus(Qt::OtherFocusReason);
        // Move cursor to end of text
        QTextCursor cursor = win_->noteEditor()->textCursor();
        cursor.movePosition(QTextCursor::End);
        win_->noteEditor()->setTextCursor(cursor);
    }
}

void ApplicationController::onEditorTextChanged() {
    // This slot is connected in constructor
}

void ApplicationController::onNewNoteRequested(const QString& text) {
    // If there are no matching notes, create a new note
    if (filtered_notes_.empty()) {
        std::string title = text.toStdString();
        createNewNote(title, "");
    } else {
        // Select the first matching note and focus the editor
        selectNote(0);
    }
    
    // Focus on the editor in both cases
    win_->noteEditor()->setFocus();
}

void ApplicationController::updateSearchResults(const std::string& query) {
    setSearchQuery(query);
}

void ApplicationController::updateUIFromState() {
    NoteListModel* model = qobject_cast<NoteListModel*>(win_->noteList()->model());
    if (!model) {
        model = new NoteListModel(win_->noteList());
        win_->noteList()->setModel(model);
    }
    model->setNotes(filtered_notes_);
}

void ApplicationController::updateUIFromFilteredNotes() {
    NoteListModel* model = qobject_cast<NoteListModel*>(win_->noteList()->model());
    if (!model) {
        model = new NoteListModel(win_->noteList());
        win_->noteList()->setModel(model);
    }
    model->setNotes(filtered_notes_);
    
    // Update selection if valid
    if (selected_index_ && *selected_index_ < filtered_notes_.size()) {
        auto selected_note = filtered_notes_[*selected_index_];
        int row = model->rowForNote(selected_note);
        if (row >= 0) {
            win_->noteList()->setCurrentIndex(model->index(row, 0));
        }
    }
}

void ApplicationController::updateUIFromFilteredNotesWithTheme() {
    updateUIFromFilteredNotes();
    // Refresh the palette to apply theme changes
    QPalette palette = win_->palette();
    win_->setPalette(palette);
    win_->noteList()->setPalette(palette);
    if (win_->noteEditor()) {
        win_->noteEditor()->setPalette(palette);
    }
}

// NoteStoreObserver implementation
void ApplicationController::onNoteAdded(std::shared_ptr<Note> note) {
    // Add to search index
    search_index_->indexNote(note);
    
    // Update filtered notes before notifying observers
    filtered_notes_ = search_index_->filter(active_query_);

    // Refresh list UI so model pointers stay in sync with filtered_notes_
    updateUIFromFilteredNotes();
    
    // Notify observers
    for (auto& cb : search_observers_) {
        cb(filtered_notes_);
    }
}

void ApplicationController::onNoteUpdated(std::shared_ptr<Note> note) {
    std::optional<NoteUUID> selected_uuid;
    if (win_ && win_->noteEditor() && win_->noteEditor()->getNote()) {
        selected_uuid = win_->noteEditor()->getNote()->uuid();
    } else if (selected_index_ && *selected_index_ < filtered_notes_.size()) {
        selected_uuid = filtered_notes_[*selected_index_]->uuid();
    }

    // Re-index the note
    search_index_->updateNote(note);
    
    // Update filtered notes before notifying observers
    filtered_notes_ = search_index_->filter(active_query_);

    // Keep selection stable across model refreshes
    if (selected_uuid) {
        selected_index_.reset();
        for (size_t i = 0; i < filtered_notes_.size(); ++i) {
            if (filtered_notes_[i] && filtered_notes_[i]->uuid() == *selected_uuid) {
                selected_index_ = i;
                break;
            }
        }
    }

    // Refresh list UI so user sees sync updates immediately
    updateUIFromFilteredNotes();

    // If the currently opened note was updated by sync, reload editor content
    if (win_ && win_->noteEditor()) {
        auto current = win_->noteEditor()->getNote();
        if (current && current->uuid() == note->uuid()) {
            win_->noteEditor()->setNote(note);
        }
    }
    
    // Notify observers
    for (auto& cb : search_observers_) {
        cb(filtered_notes_);
    }
}

void ApplicationController::onNoteDeleted(const NoteUUID& uuid) {
    // Remove from search index
    search_index_->removeNote(uuid);

    // Recompute filtered notes after deletion
    filtered_notes_ = search_index_->filter(active_query_);
    
    // Update selection if needed
    if (selected_index_ && *selected_index_ >= filtered_notes_.size()) {
        selected_index_.reset();
    }

    // Refresh list UI so deleted/remote-pruned notes disappear immediately
    updateUIFromFilteredNotes();
    
    // Notify observers
    for (auto& cb : search_observers_) {
        cb(filtered_notes_);
    }
}

void ApplicationController::setWebDAVSyncManager(WebDAVSyncManager* manager) {
    webdav_manager_ = manager;
    
    // Also set it in the note editor
    if (win_ && win_->noteEditor()) {
        win_->noteEditor()->setWebDAVSyncManager(manager);
    }
}

} // namespace nv
