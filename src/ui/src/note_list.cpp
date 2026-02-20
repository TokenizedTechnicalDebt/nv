#include "nv/note_list.h"
#include <QMouseEvent>
#include <QApplication>
#include <QStyleOptionViewItem>
#include <QHeaderView>
#include <QDateTime>
#include <QLineEdit>
#include <QFocusEvent>
#include <QTimer>

#include "nv/storage.h"
#include "nv/note_store.h"

namespace nv {

NoteListModel::NoteListModel(QObject* parent)
    : QAbstractTableModel(parent)
    , sort_column_(0)
    , sort_order_(Qt::AscendingOrder)
    , store_(nullptr)
    , storage_(nullptr) {
}

void NoteListModel::setStore(INoteStore* store) {
    store_ = store;
}

void NoteListModel::setStorage(IStorage* storage) {
    storage_ = storage;
}

void NoteListModel::updateSortOrder() {
    // Create a copy of notes for sorting
    sorted_notes_ = notes_;
    
    std::sort(sorted_notes_.begin(), sorted_notes_.end(), 
        [this](const std::shared_ptr<Note>& a, const std::shared_ptr<Note>& b) {
            if (sort_column_ == 0) {
                // Sort by title
                int result = a->title().compare(b->title());
                if (result == 0) {
                    // If titles are equal, sort by date modified
                    return a->modified() < b->modified();
                }
                return sort_order_ == Qt::AscendingOrder ? result < 0 : result > 0;
            } else {
                // Sort by date modified
                auto result = a->modified() < b->modified();
                return sort_order_ == Qt::AscendingOrder ? result : !result;
            }
        });
}

void NoteListModel::setNotes(const std::vector<std::shared_ptr<Note>>& notes) {
    beginResetModel();
    notes_ = notes;
    updateSortOrder();
    endResetModel();
}

std::shared_ptr<Note> NoteListModel::noteAt(int row) const {
    if (row < 0 || sorted_notes_.empty()) {
        return nullptr;
    }
    if (row >= static_cast<int>(sorted_notes_.size())) {
        return nullptr;
    }
    return sorted_notes_[row];
}

int NoteListModel::rowForNote(const std::shared_ptr<Note>& note) const {
    if (!note) return -1;
    
    for (int row = 0; row < static_cast<int>(sorted_notes_.size()); ++row) {
        if (sorted_notes_[row] == note) {
            return row;
        }
    }
    return -1;
}

int NoteListModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    // If no notes, return 1 for the empty state message
    return sorted_notes_.empty() ? 1 : static_cast<int>(sorted_notes_.size());
}

int NoteListModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return 2;  // Title and Date Modified columns
}

QVariant NoteListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0) {
        return QVariant();
    }
    
    // Handle empty state message
    if (sorted_notes_.empty()) {
        if (index.row() == 0 && role == Qt::DisplayRole) {
            return QString("You have no notes yet");
        }
        return QVariant();
    }
    
    if (index.row() >= static_cast<int>(sorted_notes_.size())) {
        return QVariant();
    }
    
    const auto& note = sorted_notes_[index.row()];
    
    if (role == TitleRole) {
        // Return just the title for editing
        if (index.column() == 0) {
            return QString::fromStdString(note->title());
        }
    } else if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            // Title column: show title followed by long hyphen and preview
            std::string title = note->title();
            std::string body = note->body();
            
            // Get preview - first line or first X characters
            size_t newlinePos = body.find('\n');
            std::string preview = (newlinePos == std::string::npos) 
                ? body.substr(0, 50) 
                : body.substr(0, newlinePos > 50 ? 50 : newlinePos);
            
            // Trim trailing whitespace from preview
            while (!preview.empty() && std::isspace(preview.back())) {
                preview.pop_back();
            }
            
            // Create display text: "Title — preview"
            QString display = QString::fromStdString(title + " — " + preview);
            return display;
        } else if (index.column() == 1) {
            // Date Modified column
            auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
                note->modified().time_since_epoch()).count();
            QDateTime dateTime = QDateTime::fromSecsSinceEpoch(epoch / 1000);
            return dateTime.toString("yyyy-MM-dd HH:mm");
        }
    }
    
    return QVariant();
}

QVariant NoteListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }
    
    if (section == 0) {
        return QString("Title");
    } else if (section == 1) {
        return QString("Date Modified");
    }
    
    return QVariant();
}

void NoteListModel::sort(int column, Qt::SortOrder order) {
    sort_column_ = column;
    sort_order_ = order;
    updateSortOrder();
    
    // Notify views that data has changed
    emit layoutChanged();
}

Qt::ItemFlags NoteListModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    
    // Only allow editing the title column (column 0)
    if (index.column() == 0) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool NoteListModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole) {
        return false;
    }
    
    if (index.column() != 0) {
        return false;
    }
    
    if (index.row() < 0 || index.row() >= static_cast<int>(sorted_notes_.size())) {
        return false;
    }
    
    auto note = sorted_notes_[index.row()];
    QString newText = value.toString();
    note->title() = newText.toStdString();
    
    // Update modified timestamp
    note->setModified(std::chrono::system_clock::now());
    
    // Notify store of update (this will trigger observer notifications)
    if (store_) {
        store_->updateNote(note);
    }
    
    // Persist to disk
    if (storage_) {
        auto result = storage_->writeNote(*note);
        if (!nv::isSuccess(result)) {
            qWarning() << "Failed to save note to disk";
        }
    }
    
    emit dataChanged(index, index, {Qt::EditRole});
    return true;
}

// Editor widget for inline title editing
class NoteTitleEditor : public QLineEdit {
    Q_OBJECT
public:
    explicit NoteTitleEditor(QWidget* parent = nullptr) : QLineEdit(parent) {
        setAttribute(Qt::WA_DeleteOnClose);
    }
    
protected:
    void focusOutEvent(QFocusEvent* e) override {
        // Emit signal that editing is done
        emit editingFinished();
        QLineEdit::focusOutEvent(e);
    }
    
signals:
    void editingFinished();
};

// Delegate editing methods
QWidget* NoteListItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, 
                                            const QModelIndex& index) const {
    QLineEdit* editor = new QLineEdit(parent);
    return editor;
}

void NoteListItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    QString text = index.model()->data(index, TitleRole).toString();
    QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
    if (lineEdit) {
        lineEdit->setText(text);
    }
}

void NoteListItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, 
                                        const QModelIndex& index) const {
    QLineEdit* lineEdit = qobject_cast<QLineEdit*>(editor);
    if (lineEdit) {
        QString text = lineEdit->text();
        model->setData(index, text, Qt::EditRole);
    }
}

void NoteListItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, 
                                                const QModelIndex& index) const {
    editor->setGeometry(option.rect);
}

NoteList::NoteList(QWidget* parent)
    : QTableView(parent)
    , delegate_(std::make_unique<NoteListItemDelegate>(this)) {
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSortingEnabled(true);
    
    // Set model (even if empty initially)
    setModel(&note_model_);
    
    // Use custom delegate for rendering
    setItemDelegate(delegate_.get());
    
    // Get the header and configure it
    QHeaderView* headerView = this->horizontalHeader();
    if (headerView) {
        headerView->setSectionResizeMode(0, QHeaderView::Stretch);
        headerView->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        headerView->setSectionsClickable(true);
        headerView->setSortIndicatorShown(true);
        
        connect(headerView, &QHeaderView::sectionClicked, this, &NoteList::handleHeaderClick);
    }
}

void NoteList::handleHeaderClick(int logicalIndex) {
    // Get the header
    QHeaderView* headerView = this->horizontalHeader();
    if (!headerView) return;
    
    // Toggle sort order if clicking same column
    if (logicalIndex == headerView->sortIndicatorSection()) {
        if (headerView->sortIndicatorOrder() == Qt::AscendingOrder) {
            headerView->setSortIndicator(logicalIndex, Qt::DescendingOrder);
        } else {
            headerView->setSortIndicator(logicalIndex, Qt::AscendingOrder);
        }
    } else {
        headerView->setSortIndicator(logicalIndex, Qt::AscendingOrder);
    }
    
    // Get current sort order
    Qt::SortOrder order = headerView->sortIndicatorOrder();
    
    // Tell model to sort
    QAbstractItemModel* baseModel = this->model();
    NoteListModel* model = qobject_cast<NoteListModel*>(baseModel);
    if (model) {
        model->sort(logicalIndex, order);
    }
}

void NoteList::mouseReleaseEvent(QMouseEvent* e) {
    QTableView::mouseReleaseEvent(e);

    // If this release follows a double-click, suppress normal noteSelected
    // emission to avoid focus switching away from inline title editor.
    if (suppress_next_release_selection_) {
        suppress_next_release_selection_ = false;
        return;
    }
    
    QModelIndex index = indexAt(e->pos());
    if (index.isValid()) {
        NoteListModel* model = qobject_cast<NoteListModel*>(this->model());
        if (model) {
            std::shared_ptr<Note> note = model->noteAt(index.row());
            if (note) {
                emit noteSelected(note);
            }
        }
    }
}

void NoteList::mouseDoubleClickEvent(QMouseEvent* e) {
    // Mark so the subsequent mouse-release does not emit noteSelected.
    suppress_next_release_selection_ = true;

    QTableView::mouseDoubleClickEvent(e);
    
    QModelIndex index = indexAt(e->pos());
    if (index.isValid()) {
        NoteListModel* model = qobject_cast<NoteListModel*>(this->model());
        if (model) {
            std::shared_ptr<Note> note = model->noteAt(index.row());
            if (note) {
                emit noteDoubleClicked(note);
            }
        }
    }
}

void NoteList::mousePressEvent(QMouseEvent* e) {
    QTableView::mousePressEvent(e);
}

void NoteList::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Tab) {
        // Enter/Tab move focus to editor/checklist via controller.
        // Consume the key so it does not get forwarded and create text/new checklist items.
        emit enterPressed();
        e->accept();
        return;
    } else if (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down) {
        QModelIndex index = currentIndex();
        int row = index.row();
        int rowCount = model()->rowCount();
        
        // Move selection
        if (e->key() == Qt::Key_Up) {
            if (row > 0) {
                setCurrentIndex(model()->index(row - 1, 0));
            } else {
                setCurrentIndex(model()->index(rowCount - 1, 0));
            }
        } else {
            if (row < rowCount - 1) {
                setCurrentIndex(model()->index(row + 1, 0));
            } else {
                setCurrentIndex(model()->index(0, 0));
            }
        }
        
        // Get the new note at the new position
        int newRow = currentIndex().row();
        NoteListModel* model = qobject_cast<NoteListModel*>(this->model());
        std::shared_ptr<Note> newNote = model ? model->noteAt(newRow) : nullptr;
        
        // Emit noteSelected to load the note, but keep focus on note list
        if (newNote) {
            emit noteSelected(newNote);
        }
        
        // Re-select the note list to keep focus
        setFocus(Qt::OtherFocusReason);
    } else {
        QTableView::keyPressEvent(e);
    }
}

} // namespace nv