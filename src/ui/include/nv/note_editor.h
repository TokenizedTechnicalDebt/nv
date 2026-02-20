#pragma once

#include <QPlainTextEdit>
#include <QTimer>
#include <memory>

#include "nv/note_model.h"
#include "nv/note_store.h"
#include "nv/storage.h"

// Forward declaration to avoid circular dependency
namespace nv { class WebDAVSyncManager; }
namespace nv { class CheckboxWidget; }

namespace nv {

class NoteEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit NoteEditor(INoteStore* store, IStorage* storage, QWidget* parent = nullptr);
    ~NoteEditor() override;
    void setNote(std::shared_ptr<Note> note);
    void clearNote();
    std::shared_ptr<Note> getNote() const { return current_note_; }
    
    // Set the WebDAV sync manager for bi-directional sync
    void setWebDAVSyncManager(WebDAVSyncManager* manager);
    
    // Get current content (handles both regular and checkbox modes)
    QString getCurrentContent() const;

signals:
    void textChangedForSave();

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private slots:
    void startAutoSaveTimer();
    void saveNote();
    void onTextChanged();

private:
    std::shared_ptr<Note> current_note_;
    INoteStore* store_;
    IStorage* storage_;
    WebDAVSyncManager* webdav_manager_;
    QTimer auto_save_timer_;
    QString current_body_;
    bool is_checkbox_mode_ = false;
    CheckboxWidget* checkbox_widget_ = nullptr;
    
    void setupCheckboxMode();
    void setupRegularMode();
    bool isCheckboxNote(const Note& note) const;
};

} // namespace nv
