#include "nv/note_editor.h"
#include "nv/checkbox_widget.h"
#include <QTimer>
#include <QShortcut>
#include <QFocusEvent>
#include <QApplication>
#include <QTextCursor>
#include <QKeyEvent>
#include "nv/result.h"
#include "nv/search_index.h"
#include "nv/webdav_sync_manager.h"
#include "nv/main_window.h"

namespace nv {

NoteEditor::NoteEditor(INoteStore* store, IStorage* storage, QWidget* parent)
    : QPlainTextEdit(parent)
    , store_(store)
    , storage_(storage)
    , webdav_manager_(nullptr)
    , current_body_("") {
    auto_save_timer_.setInterval(500);
    auto_save_timer_.setSingleShot(true);
    
    // Create checkbox widget as child (no layout needed for parent-child relationship)
    checkbox_widget_ = new CheckboxWidget(this);
    checkbox_widget_->hide();
    
    connect(this, &QPlainTextEdit::textChanged, this, &NoteEditor::onTextChanged);
    connect(&auto_save_timer_, &QTimer::timeout, this, &NoteEditor::saveNote);

    auto* save_shortcut = new QShortcut(QKeySequence::Save, this);
    save_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(save_shortcut, &QShortcut::activated, this, &NoteEditor::saveNote);

    // Also bind Save on the checklist widget subtree explicitly. This ensures
    // Ctrl/Cmd+S works consistently when focus is inside checklist item line edits.
    auto* checklist_save_shortcut = new QShortcut(QKeySequence::Save, checkbox_widget_);
    checklist_save_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(checklist_save_shortcut, &QShortcut::activated, this, &NoteEditor::saveNote);
    
    // Connect checkbox widget signals
    connect(checkbox_widget_, &CheckboxWidget::checkboxToggled, [this](int index, bool checked) {
        Q_UNUSED(index);
        Q_UNUSED(checked);
        startAutoSaveTimer();
    });
    connect(checkbox_widget_, &CheckboxWidget::contentEdited, this, &NoteEditor::startAutoSaveTimer);
    connect(checkbox_widget_, &CheckboxWidget::newItemRequested, [this]() {
        // Add new empty checkbox item directly without re-parsing
        if (checkbox_widget_) {
            checkbox_widget_->appendNewItem();
        }
    });
    connect(checkbox_widget_, &CheckboxWidget::deleteRequested, [this](int index) {
        Q_UNUSED(index);
        startAutoSaveTimer();
    });
}

NoteEditor::~NoteEditor() {
}

void NoteEditor::setWebDAVSyncManager(WebDAVSyncManager* manager) {
    webdav_manager_ = manager;
}

bool NoteEditor::isCheckboxNote(const Note& note) const {
    // A note is a checkbox note if it is explicitly marked as checklist,
    // or if it has checkbox patterns in the body.
    if (note.noteType() == NoteType::CHECKLIST) {
        return true;
    }

    QString body = QString::fromStdString(note.body());
    return body.contains("[x]") || body.contains("[ ]");
}

void NoteEditor::setupCheckboxMode() {
    if (checkbox_widget_) {
        checkbox_widget_->setCheckboxMode(true);
        checkbox_widget_->show();
        checkbox_widget_->setGeometry(rect());
        checkbox_widget_->setFocus();
        is_checkbox_mode_ = true;
    }
}

void NoteEditor::setupRegularMode() {
    checkbox_widget_->hide();
    setFocus();
    is_checkbox_mode_ = false;
}

void NoteEditor::setNote(std::shared_ptr<Note> note) {
    current_note_ = note;
    
    if (note) {
        // Check if this is a checkbox note
        bool is_checkbox = isCheckboxNote(*note);
        
        if (is_checkbox) {
            setupCheckboxMode();
            if (checkbox_widget_) {
                checkbox_widget_->setContent(QString::fromStdString(current_note_->body()));
            }
        } else {
            setupRegularMode();
            if (current_note_) {
                current_body_ = QString::fromStdString(current_note_->body());
                setPlainText(current_body_);
                // Move cursor to end of text for new note selection via Enter key
                QTextCursor cursor = textCursor();
                cursor.movePosition(QTextCursor::End);
                setTextCursor(cursor);
            }
        }
        
        if (current_note_) {
            current_body_ = QString::fromStdString(current_note_->body());
        }
    } else {
        current_body_.clear();
        setupRegularMode();
        clear();
    }
}

void NoteEditor::clearNote() {
    current_note_.reset();
    current_body_.clear();
    setupRegularMode();
    clear();
}

void NoteEditor::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Tab && !(e->modifiers() & Qt::ShiftModifier)) {
        if (auto* main_window = qobject_cast<MainWindow*>(window())) {
            if (main_window->searchField()) {
                main_window->searchField()->setFocus(Qt::TabFocusReason);
                e->accept();
                return;
            }
        }
    }

    QPlainTextEdit::keyPressEvent(e);
}

void NoteEditor::focusInEvent(QFocusEvent* e) {
    if (is_checkbox_mode_ && checkbox_widget_) {
        checkbox_widget_->setFocus();
    } else {
        QPlainTextEdit::focusInEvent(e);
        if (current_note_) {
            current_body_ = QString::fromStdString(current_note_->body());
            setPlainText(current_body_);
        }
    }
}

void NoteEditor::focusOutEvent(QFocusEvent* e) {
    if (current_note_ && store_ && storage_) {
        QString newText = getCurrentContent();
        // Don't save if text hasn't changed
        if (newText != current_body_) {
            saveNote();
        }
    }
    
    QPlainTextEdit::focusOutEvent(e);
}

void NoteEditor::onTextChanged() {
    // Only save if there's actually text to save
    if (!getCurrentContent().isEmpty()) {
        startAutoSaveTimer();
    }
}

void NoteEditor::startAutoSaveTimer() {
    auto_save_timer_.start();
}

QString NoteEditor::getCurrentContent() const {
    if (is_checkbox_mode_ && checkbox_widget_) {
        return checkbox_widget_->getContent();
    }
    return toPlainText();
}

void NoteEditor::saveNote() {
    if (!store_ || !storage_) return;
    
    // Check if text has actually changed
    QString newText = getCurrentContent();
    if (newText == current_body_) {
        return;  // No changes, don't save
    }
    
    // Update current body to track the change
    current_body_ = newText;
    
    // If current_note_ is null, we need to create a new note
    if (!current_note_) {
        std::string text = newText.toStdString();
        size_t newlinePos = text.find('\n');
        std::string title = (newlinePos == std::string::npos) ? text : text.substr(0, newlinePos);
        std::string body = (newlinePos == std::string::npos) ? "" : text.substr(newlinePos + 1);
        
        // Determine if this is a checkbox note
        bool isCheckbox = is_checkbox_mode_ && checkbox_widget_ && 
                         (text.find("[x]") != std::string::npos || text.find("[ ]") != std::string::npos);
        
        // Generate UUID and create new note
        std::string uuid = SearchIndex::generateUUID();
        NoteTimestamp now = std::chrono::system_clock::now();
        current_note_ = std::make_shared<Note>(uuid, title, body, now, now);
        
        // Set note type
        if (isCheckbox) {
            current_note_->setNoteType(NoteType::CHECKLIST);
            current_note_->setSyncStatus("CHECKLIST");
        }
        
        // Add to store
        store_->addNote(current_note_);
        
        // Write to disk
        auto writeResult = storage_->writeNote(*current_note_);
        if (!nv::isSuccess(writeResult)) {
            qWarning() << "Failed to save new note to disk";
        }
    } else {
        // Update existing note content
        std::string newBody = newText.toStdString();
        current_note_->body() = newBody;
        
        // Determine if this is now a checkbox note
        bool isCheckbox = newText.contains("[x]") || newText.contains("[ ]");
        if (isCheckbox) {
            current_note_->setNoteType(NoteType::CHECKLIST);
        } else {
            current_note_->setNoteType(NoteType::TEXT);
        }
        
        // Update modified timestamp
        current_note_->setModified(std::chrono::system_clock::now());
        
        // Persist to store
        store_->updateNote(current_note_);
        
        // Persist to disk
        auto result = storage_->writeNote(*current_note_);
        if (!nv::isSuccess(result)) {
            qWarning() << "Failed to save note to disk";
        }
    }
    
    // Emit signal for UI update
    emit textChangedForSave();
    
    // Trigger WebDAV sync if enabled
    if (webdav_manager_) {
        webdav_manager_->triggerSyncOnSearch();
    }
}

} // namespace nv