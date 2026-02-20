#include "nv/search_field.h"
#include <QKeyEvent>
#include <QTimer>

namespace nv {

SearchField::SearchField(QWidget* parent)
    : QLineEdit(parent) {
    setPlaceholderText("Search notes...");
    
    // Use a timer to emit searchStopped after user stops typing
    auto* debounceTimer = new QTimer(this);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(500);  // 500ms after user stops typing
    
    // Reset timer and emit querySubmitted on text change
    connect(this, &QLineEdit::textEdited, [this, debounceTimer]() {
        debounceTimer->stop();
        emit querySubmitted(text());
        debounceTimer->start();
    });
    
    // Emit searchStopped when timer times out (user stopped typing)
    connect(debounceTimer, &QTimer::timeout, [this]() {
        emit searchStopped();
    });
}

void SearchField::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        // Emit both signals - querySubmitted for filtering, newNoteRequested for creating note
        emit querySubmitted(text());
        emit newNoteRequested(text());
    } else if (e->key() == Qt::Key_Escape) {
        clear();
        emit querySubmitted("");
    } else {
        QLineEdit::keyPressEvent(e);
    }
}

} // namespace nv
