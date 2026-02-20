#pragma once

#include <QTableView>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QColor>
#include <memory>
#include <vector>

// Forward declarations to avoid circular dependency
namespace nv {
class INoteStore;
class IStorage;
}
#include "nv/note_model.h"

namespace nv {

// Custom roles for the model
enum {
    TitleRole = Qt::UserRole + 1,
};

class NoteListModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit NoteListModel(QObject* parent = nullptr);
    void setNotes(const std::vector<std::shared_ptr<Note>>& notes);
    std::shared_ptr<Note> noteAt(int row) const;
    
    void setStore(INoteStore* store);
    void setStorage(IStorage* storage);
    
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    
    // Enable editing for title column (column 0)
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    
    // Sorting support
    void setSortOrder(int column, Qt::SortOrder order);
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    
    // Editing support
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    
    // Get row for a note (returns -1 if not found)
    int rowForNote(const std::shared_ptr<Note>& note) const;

private:
    std::vector<std::shared_ptr<Note>> notes_;
    std::vector<std::shared_ptr<Note>> sorted_notes_;
    int sort_column_ = 0;
    Qt::SortOrder sort_order_ = Qt::AscendingOrder;
    INoteStore* store_ = nullptr;
    IStorage* storage_ = nullptr;
    void updateSortOrder();
};

class NoteListItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit NoteListItemDelegate(QObject* parent = nullptr) 
        : QStyledItemDelegate(parent) {}
    
    void paint(QPainter* painter, const QStyleOptionViewItem& option, 
               const QModelIndex& index) const override {
        if (!index.isValid()) {
            return;
        }
        
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        
        painter->save();
        
        // Handle the empty state message
        if (opt.text.isEmpty() && index.row() == 0) {
            QStyledItemDelegate::paint(painter, option, index);
            painter->restore();
            return;
        }
        
        // Get the display text
        QString displayText = opt.text;
        
        // Find the separator " — " and split into title and preview
        int separatorPos = displayText.indexOf(" — ");
        
        if (separatorPos != -1) {
            QString title = displayText.left(separatorPos + 1);  // Include the em dash
            QString preview = displayText.mid(separatorPos + 2); // After the em dash
            
            // Get the palette
            QPalette palette = opt.palette;
            
            // Set colors based on theme
            QColor titleColor = palette.color(QPalette::WindowText);
            QColor previewColor = palette.color(QPalette::WindowText);
            
            // Adjust preview color to be slightly different (gray)
            // In white theme, preview is gray; in black theme, preview is light gray
            int alpha = (palette.color(QPalette::Window).value() < 128) ? 180 : 100;
            previewColor.setAlpha(alpha);
            
            // Draw background if selected
            if (opt.state & QStyle::State_Selected) {
                painter->fillRect(opt.rect, palette.color(QPalette::Highlight));
                titleColor = palette.color(QPalette::HighlightedText);
                previewColor = palette.color(QPalette::HighlightedText);
                previewColor.setAlpha(alpha);
            } else if (opt.state & QStyle::State_MouseOver) {
                painter->fillRect(opt.rect, palette.color(QPalette::AlternateBase));
            } else {
                // Draw alternating row colors
                if (opt.features & QStyleOptionViewItem::Alternate) {
                    painter->fillRect(opt.rect, palette.color(QPalette::AlternateBase));
                } else {
                    painter->fillRect(opt.rect, palette.color(QPalette::Base));
                }
            }
            
            // Set font for title (normal) and preview (slightly smaller)
            QFont titleFont = opt.font;
            QFont previewFont = opt.font;
            previewFont.setPointSizeF(opt.font.pointSizeF() * 0.9);
            
            // Draw title
            painter->setFont(titleFont);
            painter->setPen(titleColor);
            QRect titleRect = opt.rect;
            titleRect.setRight(opt.rect.center().x());
            
            // Calculate text width to find where to split
            int titleWidth = painter->fontMetrics().horizontalAdvance(title);
            int previewX = opt.rect.left() + titleWidth + 10;  // 10px gap
            
            // Draw title
            painter->drawText(opt.rect.left(), opt.rect.top() + 2, titleWidth, 
                             opt.rect.height() - 4, Qt::AlignLeft | Qt::AlignVCenter, title);
            
            // Draw preview with different color
            painter->setFont(previewFont);
            painter->setPen(previewColor);
            QRect previewRect = opt.rect;
            previewRect.setLeft(previewX);
            
            // Calculate available width for preview
            int previewWidth = opt.rect.right() - previewX - 10;
            if (previewWidth > 0) {
                QString elidedPreview = painter->fontMetrics().elidedText(
                    preview, Qt::ElideRight, previewWidth);
                painter->drawText(previewX, opt.rect.top() + 2, previewWidth,
                                 opt.rect.height() - 4, Qt::AlignLeft | Qt::AlignVCenter, elidedPreview);
            }
        } else {
            // Fallback to default painting
            QStyledItemDelegate::paint(painter, option, index);
        }
        
        painter->restore();
    }
    
    QSize sizeHint(const QStyleOptionViewItem& option, 
                   const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(size.height() + 4);  // Add a bit of extra height for two-line display
        return size;
    }
    
    // Editing support
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, 
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, 
                      const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, 
                              const QModelIndex& index) const override;
};

class NoteList : public QTableView {
    Q_OBJECT

public:
    explicit NoteList(QWidget* parent = nullptr);

signals:
    void noteSelected(std::shared_ptr<Note> note);
    void noteDoubleClicked(std::shared_ptr<Note> note);
    void enterPressed();

public:
    // Store and storage methods for model
    void setStore(INoteStore* store) { note_model_.setStore(store); }
    void setStorage(IStorage* storage) { note_model_.setStorage(storage); }

protected:
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    
    // Handle column header clicks for sorting
    void mousePressEvent(QMouseEvent* e) override;

private:
    void handleHeaderClick(int logicalIndex);
    NoteListModel note_model_;
    std::unique_ptr<NoteListItemDelegate> delegate_;
    bool suppress_next_release_selection_ = false;
};

} // namespace nv