#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QSettings>
#include <QRadioButton>
#include <QAction>

#include "nv/search_field.h"
#include "nv/note_list.h"
#include "nv/note_editor.h"

namespace nv {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    SearchField* searchField() { return search_field_; }
    NoteList* noteList() { return note_list_; }
    NoteEditor* noteEditor() { return note_editor_; }
    void setNoteEditor(NoteEditor* editor);
    
    // Settings methods
    void setupLayoutMenu(QMenuBar* menuBar);
    void setupThemeMenu(QMenuBar* menuBar);
    int layoutMode() const;
    void setLayoutMode(int mode);
    int theme() const;
    void setTheme(int theme);
    
    // Menu bar action setters (for platform-specific code)
    void setMenuLayoutVerticalAction(QAction* action);
    void setMenuLayoutHorizontalAction(QAction* action);
    void setMenuThemeWhiteAction(QAction* action);
    void setMenuThemeBlackAction(QAction* action);
    
    // Store and storage methods for note list model
    void setNoteStore(INoteStore* store);
    void setStorage(IStorage* storage);
    
signals:
    void layoutModeChanged(int mode);
    void themeChanged(int theme);

private:
    void loadWindowGeometry();
    void saveWindowGeometry();
    
    SearchField* search_field_;
    NoteList* note_list_;
    NoteEditor* note_editor_;
    QSplitter* splitter_;
    QActionGroup* layout_action_group_;
    QActionGroup* theme_action_group_;
    QAction* layout_vertical_action_;
    QAction* layout_horizontal_action_;
    QAction* theme_white_action_;
    QAction* theme_black_action_;
    
    // Menu bar actions (owned by menu bar, not by MainWindow)
    QAction* menu_layout_vertical_action_ = nullptr;
    QAction* menu_layout_horizontal_action_ = nullptr;
    QAction* menu_theme_white_action_ = nullptr;
    QAction* menu_theme_black_action_ = nullptr;
};

} // namespace nv