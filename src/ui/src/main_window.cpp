#include "nv/main_window.h"
#include <QVBoxLayout>
#include <QSplitter>
#include <QStackedWidget>
#include <QApplication>
#include <QGuiApplication>
#include <QSettings>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QMenuBar>
#include <QRadioButton>
#include "nv/app_state.h"
#include "nv/note_store.h"
#include "nv/storage.h"
#include "nv/note_editor.h"

namespace nv {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , note_editor_(nullptr)
    , layout_action_group_(nullptr)
    , theme_action_group_(nullptr)
    , layout_vertical_action_(nullptr)
    , layout_horizontal_action_(nullptr)
    , theme_white_action_(nullptr)
    , theme_black_action_(nullptr) {
    // Load window geometry and settings
    loadWindowGeometry();
    
    // Create widgets
    search_field_ = new SearchField();
    note_list_ = new NoteList();
    
    // Create splitter for list and editor
    // Start with vertical layout (default)
    splitter_ = new QSplitter(Qt::Vertical);
    splitter_->addWidget(note_list_);
    
    // Apply theme
    setTheme(ApplicationState::instance().theme());
    
    // Create main layout
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(search_field_);
    layout->addWidget(splitter_);
    
    setCentralWidget(centralWidget);
    
    // Set minimum sizes for widgets
    search_field_->setMinimumHeight(28);
    note_list_->setMinimumHeight(100);
    
    // Set window title
    setWindowTitle("Notation V");
    
    // Set focus policy
    search_field_->setFocusPolicy(Qt::StrongFocus);
    note_list_->setFocusPolicy(Qt::StrongFocus);
}

MainWindow::~MainWindow() {
    // Save splitter state
    ApplicationState::instance().setSplitterState(splitter_->saveState());
    saveWindowGeometry();
}

void MainWindow::loadWindowGeometry() {
    QSettings settings("nv", "NotationalVelocity");
    QByteArray geometry = settings.value("windowGeometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
}

void MainWindow::saveWindowGeometry() {
    QSettings settings("nv", "NotationalVelocity");
    settings.setValue("windowGeometry", saveGeometry());
}

void MainWindow::setupLayoutMenu(QMenuBar* menuBar) {
    if (!menuBar) return;
    
    // Find or create Settings menu
    QMenu* settingsMenu = nullptr;
    for (QMenu* menu : menuBar->findChildren<QMenu*>()) {
        if (menu->title() == "&Settings") {
            settingsMenu = menu;
            break;
        }
    }
    
    if (!settingsMenu) {
        settingsMenu = menuBar->addMenu("&Settings");
    }
    
    // Add layout menu if not already added
    if (!layout_action_group_) {
        layout_action_group_ = new QActionGroup(this);
        
        layout_vertical_action_ = settingsMenu->addAction("Layout: Vertical");
        layout_vertical_action_->setCheckable(true);
        layout_vertical_action_->setChecked(false);  // Don't pre-check - will be set by setLayoutMode()
        layout_action_group_->addAction(layout_vertical_action_);
        
        layout_horizontal_action_ = settingsMenu->addAction("Layout: Horizontal");
        layout_horizontal_action_->setCheckable(true);
        layout_horizontal_action_->setChecked(false);  // Don't pre-check - will be set by setLayoutMode()
        layout_action_group_->addAction(layout_horizontal_action_);
        
        // Store pointers to menu bar actions for later updates
        menu_layout_vertical_action_ = layout_vertical_action_;
        menu_layout_horizontal_action_ = layout_horizontal_action_;
        
        connect(layout_vertical_action_, &QAction::triggered, [this]() {
            setLayoutMode(0);
            emit layoutModeChanged(0);
        });
        
        connect(layout_horizontal_action_, &QAction::triggered, [this]() {
            setLayoutMode(1);
            emit layoutModeChanged(1);
        });
    }
}

void MainWindow::setupThemeMenu(QMenuBar* menuBar) {
    if (!menuBar) return;
    
    // Find or create Settings menu
    QMenu* settingsMenu = nullptr;
    for (QMenu* menu : menuBar->findChildren<QMenu*>()) {
        if (menu->title() == "&Settings") {
            settingsMenu = menu;
            break;
        }
    }
    
    if (!settingsMenu) {
        settingsMenu = menuBar->addMenu("&Settings");
    }
    
    // Add theme menu if not already added
    if (!theme_action_group_) {
        theme_action_group_ = new QActionGroup(this);
        
        theme_white_action_ = settingsMenu->addAction("Theme: White");
        theme_white_action_->setCheckable(true);
        theme_white_action_->setChecked(false);  // Don't pre-check - will be set by setTheme()
        theme_action_group_->addAction(theme_white_action_);
        
        theme_black_action_ = settingsMenu->addAction("Theme: Black");
        theme_black_action_->setCheckable(true);
        theme_black_action_->setChecked(false);  // Don't pre-check - will be set by setTheme()
        theme_action_group_->addAction(theme_black_action_);
        
        connect(theme_white_action_, &QAction::triggered, [this]() {
            setTheme(0);
            emit themeChanged(0);
        });
        
        connect(theme_black_action_, &QAction::triggered, [this]() {
            setTheme(1);
            emit themeChanged(1);
        });
    }
}

int MainWindow::layoutMode() const {
    return ApplicationState::instance().layoutMode();
}

void MainWindow::setLayoutMode(int mode) {
    ApplicationState::instance().setLayoutMode(mode);
    
    if (mode == 1) {
        // Horizontal/Landscape mode
        if (splitter_) {
            splitter_->setOrientation(Qt::Horizontal);
        }
        if (layout_horizontal_action_) {
            layout_horizontal_action_->setChecked(true);
        }
        if (menu_layout_vertical_action_) {
            menu_layout_vertical_action_->setChecked(false);
        }
        if (menu_layout_horizontal_action_) {
            menu_layout_horizontal_action_->setChecked(true);
        }
    } else {
        // Vertical mode (default)
        if (splitter_) {
            splitter_->setOrientation(Qt::Vertical);
        }
        if (layout_vertical_action_) {
            layout_vertical_action_->setChecked(true);
        }
        if (menu_layout_vertical_action_) {
            menu_layout_vertical_action_->setChecked(true);
        }
        if (menu_layout_horizontal_action_) {
            menu_layout_horizontal_action_->setChecked(false);
        }
    }
}

int MainWindow::theme() const {
    return ApplicationState::instance().theme();
}

void MainWindow::setMenuLayoutVerticalAction(QAction* action) {
    menu_layout_vertical_action_ = action;
}

void MainWindow::setMenuLayoutHorizontalAction(QAction* action) {
    menu_layout_horizontal_action_ = action;
}

void MainWindow::setMenuThemeWhiteAction(QAction* action) {
    menu_theme_white_action_ = action;
}

void MainWindow::setMenuThemeBlackAction(QAction* action) {
    menu_theme_black_action_ = action;
}

void MainWindow::setTheme(int theme) {
    ApplicationState::instance().setTheme(theme);
    
    // Update theme checkbox states (both internal and menu bar actions)
    if (theme_black_action_) {
        theme_black_action_->setChecked(theme == 1);
    }
    if (theme_white_action_) {
        theme_white_action_->setChecked(theme == 0);
    }
    if (menu_theme_black_action_) {
        menu_theme_black_action_->setChecked(theme == 1);
    }
    if (menu_theme_white_action_) {
        menu_theme_white_action_->setChecked(theme == 0);
    }
    
    QPalette palette;
    QString stylesheet;
    
    if (theme == 1) {
        // Black theme - invert colors
        palette.setColor(QPalette::Window, Qt::black);
        palette.setColor(QPalette::WindowText, Qt::white);
        palette.setColor(QPalette::Base, Qt::black);
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::Button, Qt::gray);
        palette.setColor(QPalette::ButtonText, Qt::white);
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Link, Qt::cyan);
        palette.setColor(QPalette::Highlight, Qt::gray);
        palette.setColor(QPalette::HighlightedText, Qt::black);
        
        // Use stylesheet to fix menu bar text color
        stylesheet = "QMenuBar { background-color: black; color: white; } "
                     "QMenuBar::item { background-color: transparent; color: white; } "
                     "QMenuBar::item:selected { background-color: gray; color: white; } "
                     "QMenuBar::item:pressed { background-color: gray; color: white; } "
                     "QMenu { background-color: black; color: white; } "
                     "QMenu::item { background-color: transparent; color: white; } "
                     "QMenu::item:selected { background-color: gray; color: white; } "
                     "QMenu::item:pressed { background-color: gray; color: white; }";
    } else {
        // White theme (default)
        palette.setColor(QPalette::Window, Qt::white);
        palette.setColor(QPalette::WindowText, Qt::black);
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::Button, QApplication::palette().button().color());
        palette.setColor(QPalette::ButtonText, QApplication::palette().buttonText().color());
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Link, Qt::blue);
        palette.setColor(QPalette::Highlight, QApplication::palette().highlight().color());
        palette.setColor(QPalette::HighlightedText, QApplication::palette().highlightedText().color());
        
        // Use stylesheet to fix menu bar text color
        stylesheet = "QMenuBar { background-color: white; color: black; } "
                     "QMenuBar::item { background-color: transparent; color: black; } "
                     "QMenuBar::item:selected { background-color: #e0e0e0; color: black; } "
                     "QMenuBar::item:pressed { background-color: #d0d0d0; color: black; } "
                     "QMenu { background-color: white; color: black; } "
                     "QMenu::item { background-color: transparent; color: black; } "
                     "QMenu::item:selected { background-color: #e0e0e0; color: black; } "
                     "QMenu::item:pressed { background-color: #d0d0d0; color: black; }";
    }
    
    setPalette(palette);
    search_field_->setPalette(palette);
    note_list_->setPalette(palette);
    if (note_editor_) {
        note_editor_->setPalette(palette);
    }
    
    // Also set palette for all children
    QApplication::setPalette(palette);
    
    // Apply stylesheet to menu bar to fix text color
    QMenuBar* menuBar = this->menuBar();
    if (menuBar) {
        menuBar->setStyleSheet(stylesheet);
        menuBar->setAutoFillBackground(true);
        
        // Force menu bar to repaint
        menuBar->update();
    }
}

void MainWindow::setNoteEditor(NoteEditor* editor) {
    if (note_editor_) {
        splitter_->replaceWidget(splitter_->indexOf(note_editor_), editor);
        note_editor_->deleteLater();
    }
    
    note_editor_ = editor;
    
    if (note_editor_ && splitter_->indexOf(note_editor_) == -1) {
        // Editor not yet in splitter (first time)
        splitter_->addWidget(note_editor_);
        note_editor_->setMinimumHeight(100);
        note_editor_->setFocusPolicy(Qt::StrongFocus);
        
        // Restore saved splitter state after adding editor
        // This ensures the saved proportions are applied even if editor was added after constructor
        QByteArray savedState = ApplicationState::instance().splitterState();
        if (!savedState.isEmpty()) {
            // Use saved state
            splitter_->restoreState(savedState);
        } else {
            // No saved state - set initial sizes (30% list, 70% editor)
            int win_h = QGuiApplication::primaryScreen()->geometry().height();
            splitter_->setSizes({int(win_h * 0.3), win_h - 28 - int(win_h * 0.3)});
        }
    }
}

void MainWindow::setNoteStore(INoteStore* store) {
    note_list_->setStore(store);
}

void MainWindow::setStorage(IStorage* storage) {
    note_list_->setStorage(storage);
}

} // namespace nv
