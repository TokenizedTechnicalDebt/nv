#pragma once

#include <QLineEdit>
#include <QTimer>

namespace nv {

class SearchField : public QLineEdit {
    Q_OBJECT

public:
    explicit SearchField(QWidget* parent = nullptr);

signals:
    void querySubmitted(const QString& text);
    void newNoteRequested(const QString& text);
    void searchStopped();  // Emitted when user stops typing

protected:
    void keyPressEvent(QKeyEvent* e) override;
};

} // namespace nv
