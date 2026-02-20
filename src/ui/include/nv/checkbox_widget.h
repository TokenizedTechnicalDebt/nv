#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QKeyEvent>
#include <QPushButton>
#include <memory>
#include <vector>

namespace nv {

class CheckboxWidget : public QWidget {
    Q_OBJECT

public:
    explicit CheckboxWidget(QWidget* parent = nullptr);
    
    // Set content from markdown checkbox format
    void setContent(const QString& content);
    // Get content as markdown checkbox format
    QString getContent() const;
    
    // Get/set current note type
    bool isCheckboxMode() const { return checkbox_mode_; }
    void setCheckboxMode(bool mode);
    
signals:
    void checkboxToggled(int index, bool checked);
    void newItemRequested();
    void deleteRequested(int index);
    void focusRequested();
    void contentEdited();
    
public slots:
    void appendNewItem();
    void deleteItem(int index);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onCheckboxToggled(bool checked);
    void onItemDeleted(int index);

private:
    struct CheckboxItem {
        QCheckBox* checkbox;
        QLineEdit* input;
    };
    
    void updateCheckboxItems();
    int getCurrentIndex() const;
    
    QVBoxLayout* main_layout_;
    QScrollArea* scroll_area_;
    QWidget* scroll_content_;
    QVBoxLayout* items_layout_;
    std::vector<CheckboxItem> items_;
    bool checkbox_mode_ = true;
};

} // namespace nv