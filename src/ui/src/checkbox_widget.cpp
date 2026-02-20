#include "nv/checkbox_widget.h"
#include "nv/main_window.h"
#include <QApplication>
#include <QShortcut>
#include <QTextCursor>
#include <QScrollBar>
#include <QLayout>
#include <QList>
#include <QEvent>
#include <algorithm>

namespace {

void clearLayout(QLayout* layout) {
    if (!layout) {
        return;
    }

    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        if (!item) {
            continue;
        }

        if (QWidget* widget = item->widget()) {
            delete widget;
        }

        if (QLayout* child_layout = item->layout()) {
            clearLayout(child_layout);
        }

        delete item;
    }
}

} // namespace

namespace nv {

CheckboxWidget::CheckboxWidget(QWidget* parent)
    : QWidget(parent)
    , main_layout_(new QVBoxLayout(this))
    , scroll_area_(new QScrollArea(this))
    , scroll_content_(new QWidget(this))
    , items_layout_(new QVBoxLayout()) {
    
    scroll_area_->setWidget(scroll_content_);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    main_layout_->addWidget(scroll_area_);
    main_layout_->setContentsMargins(0, 0, 0, 0);
    
    // Configure scroll area layout
    scroll_content_->setLayout(items_layout_);
    items_layout_->setContentsMargins(0, 0, 0, 0);
    items_layout_->setSpacing(4);
    
    setLayout(main_layout_);

    // Ensure Ctrl+D works even when a child QLineEdit currently has focus.
    auto* delete_shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this);
    delete_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(delete_shortcut, &QShortcut::activated, this, [this]() {
        onItemDeleted(getCurrentIndex());
    });
}

void CheckboxWidget::setCheckboxMode(bool mode) {
    checkbox_mode_ = mode;
}

void CheckboxWidget::updateCheckboxItems() {
    const QString content = getContent();

    clearLayout(items_layout_);
    items_.clear();

    if (!checkbox_mode_) {
        return;
    }

    setContent(content);
}

void CheckboxWidget::setContent(const QString& content) {
    clearLayout(items_layout_);
    items_.clear();
    
    if (!checkbox_mode_) {
        return;
    }
    
    // Parse content line by line
    QStringList lines = content.split('\n', Qt::SkipEmptyParts);
    
    for (const QString& line : lines) {
        CheckboxItem item;
        
        // Create layout for this item
        QHBoxLayout* item_layout = new QHBoxLayout();
        item_layout->setContentsMargins(10, 0, 10, 0);
        item_layout->setSpacing(8);
        
        // Create checkbox
        item.checkbox = new QCheckBox(scroll_content_);
        item_layout->addWidget(item.checkbox);
        
        // Parse checkbox state
        bool checked = false;
        QString text;
        
        if (line.startsWith("[x]")) {
            checked = true;
            text = line.mid(4).trimmed();
        } else if (line.startsWith("[ ]")) {
            checked = false;
            text = line.mid(4).trimmed();
        } else {
            // Regular line without checkbox marker
            text = line;
        }
        
        item.checkbox->setChecked(checked);
        
        // Create line edit for text
        item.input = new QLineEdit(text, scroll_content_);
        item.input->setStyleSheet("border: none; background: transparent;");
        item.input->setFocusPolicy(Qt::StrongFocus);
        item.input->installEventFilter(this);
        item_layout->addWidget(item.input);
        connect(item.input, &QLineEdit::textEdited, this, [this](const QString&) {
            emit contentEdited();
        });
        
        // Add delete button
        QPushButton* delete_btn = new QPushButton("×", scroll_content_);
        delete_btn->setStyleSheet("QPushButton { border: none; color: #666; } "
                                  "QPushButton:hover { color: #c00; }");
        delete_btn->setCursor(Qt::PointingHandCursor);
        delete_btn->setFixedWidth(20);
        connect(delete_btn, &QPushButton::clicked, [this, index = items_.size()]() {
            onItemDeleted(index);
        });
        item_layout->addWidget(delete_btn);
        
        items_layout_->addLayout(item_layout);
        
        // Connect checkbox signals
        connect(item.checkbox, &QCheckBox::toggled, [this, index = items_.size()](bool checked) {
            emit checkboxToggled(index, checked);
        });
        
        items_.push_back(item);
    }
}

QString CheckboxWidget::getContent() const {
    if (!checkbox_mode_) {
        return "";
    }
    
    QStringList lines;
    
    for (const auto& item : items_) {
        if (item.checkbox && item.input) {
            QString text = item.input->text();
            if (item.checkbox->isChecked()) {
                lines.append(QString("[x] %1").arg(text));
            } else {
                lines.append(QString("[ ] %1").arg(text));
            }
        }
    }
    
    return lines.join('\n');
}

void CheckboxWidget::keyPressEvent(QKeyEvent* e) {
    // Handle special keys only when checkbox mode is enabled
    if (!checkbox_mode_) {
        QWidget::keyPressEvent(e);
        return;
    }
    
    int current_index = getCurrentIndex();
    
    switch (e->key()) {
        case Qt::Key_T:
            // Ctrl+T toggles current checkbox
            if (e->modifiers() & Qt::ControlModifier) {
                if (current_index >= 0 && current_index < static_cast<int>(items_.size())) {
                    QCheckBox* cb = items_[current_index].checkbox;
                    if (cb) {
                        cb->setChecked(!cb->isChecked());
                        emit checkboxToggled(current_index, cb->isChecked());
                    }
                }
                return;  // Don't pass to parent
            }
            break;
            
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // Add new item after current
            emit newItemRequested();
            break;
            
        case Qt::Key_D:
            // Ctrl+D / Cmd+D deletes the current row (works on both macOS and other platforms)
            if (e->modifiers() & Qt::ControlModifier) {
                onItemDeleted(current_index);
                return;  // Don't pass to parent
            }
            break;
            
        case Qt::Key_Up:
            // Move to previous item
            if (current_index > 0) {
                if (items_[current_index - 1].input) {
                    items_[current_index - 1].input->setFocus();
                }
            } else if (!items_.empty()) {
                items_.back().input->setFocus();
            }
            return;  // Don't pass to parent
            
        case Qt::Key_Down:
            // Move to next item
            if (current_index >= 0 && current_index < static_cast<int>(items_.size() - 1)) {
                if (items_[current_index + 1].input) {
                    items_[current_index + 1].input->setFocus();
                }
            } else if (!items_.empty()) {
                items_.front().input->setFocus();
            }
            return;  // Don't pass to parent
            
        default:
            QWidget::keyPressEvent(e);
    }
}

bool CheckboxWidget::eventFilter(QObject* watched, QEvent* event) {
    if (!checkbox_mode_ || event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }

    auto* key_event = static_cast<QKeyEvent*>(event);
    const int current_index = getCurrentIndex();

    switch (key_event->key()) {
        case Qt::Key_Tab:
            if (!(key_event->modifiers() & Qt::ShiftModifier)) {
                if (auto* main_window = qobject_cast<MainWindow*>(window())) {
                    if (main_window->searchField()) {
                        main_window->searchField()->setFocus(Qt::TabFocusReason);
                        key_event->accept();
                        return true;
                    }
                }
            }
            break;

        case Qt::Key_Up:
            if (current_index > 0 && items_[current_index - 1].input) {
                items_[current_index - 1].input->setFocus();
                return true;
            }
            if (!items_.empty() && items_.back().input) {
                items_.back().input->setFocus();
                return true;
            }
            break;

        case Qt::Key_Down:
            if (current_index >= 0 && current_index < static_cast<int>(items_.size() - 1) && items_[current_index + 1].input) {
                items_[current_index + 1].input->setFocus();
                return true;
            }
            if (!items_.empty() && items_.front().input) {
                items_.front().input->setFocus();
                return true;
            }
            break;

        default:
            break;
    }

    return QWidget::eventFilter(watched, event);
}

int CheckboxWidget::getCurrentIndex() const {
    // Find which input currently has focus
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].input && items_[i].input->hasFocus()) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CheckboxWidget::onCheckboxToggled(bool checked) {
    Q_UNUSED(checked);
}

void CheckboxWidget::onItemDeleted(int index) {
    // Delete the item at the given index
    deleteItem(index);
    emit deleteRequested(index);
}

void CheckboxWidget::appendNewItem() {
    if (!checkbox_mode_) {
        return;
    }
    
    CheckboxItem item;
    
    // Create layout for this item
    QHBoxLayout* item_layout = new QHBoxLayout();
    item_layout->setContentsMargins(10, 0, 10, 0);
    item_layout->setSpacing(8);
    
    // Create checkbox (unchecked by default)
    item.checkbox = new QCheckBox(scroll_content_);
    item_layout->addWidget(item.checkbox);
    item.checkbox->setChecked(false);
    
    // Create line edit for text (empty)
    item.input = new QLineEdit("", scroll_content_);
    item.input->setStyleSheet("border: none; background: transparent;");
    item.input->setFocusPolicy(Qt::StrongFocus);
    item.input->installEventFilter(this);
    item_layout->addWidget(item.input);
    connect(item.input, &QLineEdit::textEdited, this, [this](const QString&) {
        emit contentEdited();
    });
    
    // Add delete button
    QPushButton* delete_btn = new QPushButton("×", scroll_content_);
    delete_btn->setStyleSheet("QPushButton { border: none; color: #666; } "
                              "QPushButton:hover { color: #c00; }");
    delete_btn->setCursor(Qt::PointingHandCursor);
    delete_btn->setFixedWidth(20);
    connect(delete_btn, &QPushButton::clicked, [this, index = items_.size()]() {
        onItemDeleted(index);
    });
    item_layout->addWidget(delete_btn);
    
    items_layout_->addLayout(item_layout);
    
    // Connect checkbox signals
    connect(item.checkbox, &QCheckBox::toggled, [this, index = items_.size()](bool checked) {
        emit checkboxToggled(index, checked);
    });
    
    items_.push_back(item);
    emit contentEdited();
    
    // Focus on the new item
    item.input->setFocus();
}

void CheckboxWidget::deleteItem(int index) {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return;
    }

    struct SavedItemData {
        bool checked = false;
        QString text;
    };

    std::vector<SavedItemData> saved_data;
    saved_data.reserve(items_.size() - 1);

    for (size_t i = 0; i < items_.size(); ++i) {
        if (static_cast<int>(i) != index && items_[i].checkbox && items_[i].input) {
            SavedItemData data;
            data.checked = items_[i].checkbox->isChecked();
            data.text = items_[i].input->text();
            saved_data.push_back(data);
        }
    }

    QStringList lines;
    lines.reserve(static_cast<int>(saved_data.size()));
    for (const auto& data : saved_data) {
        lines.append(QString("[%1] %2").arg(data.checked ? "x" : " ", data.text));
    }

    setContent(lines.join('\n'));

    const int focus_index = std::min(index, static_cast<int>(items_.size()) - 1);
    if (focus_index >= 0 && focus_index < static_cast<int>(items_.size()) && items_[focus_index].input) {
        items_[focus_index].input->setFocus();
    }
}

} // namespace nv