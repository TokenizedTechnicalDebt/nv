#include <QMainWindow>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QStandardPaths>
#include <QFileDialog>
#include <QMessageBox>
#include <QKeySequence>

#include "nv/main_window.h"
#include "nv/note_editor.h"
#include "nv/app_state.h"
#include "nv/webdav_config_dialog.h"
#include "../ShortcutsHelp.h"

namespace {

void setup_linux_menu_bar_impl(nv::MainWindow* win) {
    QMenuBar* menuBar = new QMenuBar();
    
    // File menu
    QMenu* fileMenu = menuBar->addMenu(QApplication::tr("&File"));
    QAction* quitAction = fileMenu->addAction(QApplication::tr("Quit"));
    #ifdef Q_OS_MACOS
    quitAction->setShortcut(QKeySequence::fromString("Ctrl+Q"));
    #else
    quitAction->setShortcuts(QKeySequence::Quit);
    #endif
    // Connect quit action to application quit
    QObject::connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    
    // Settings menu
    QMenu* settingsMenu = menuBar->addMenu(QApplication::tr("&Settings"));
    QAction* notesDirAction = settingsMenu->addAction(QApplication::tr("Notes Directory"));
    notesDirAction->setShortcut(QKeySequence::fromString("Ctrl+,"));
    QObject::connect(notesDirAction, &QAction::triggered, [win]() {
        QString dir = QFileDialog::getExistingDirectory(
            win,
            "Select Notes Directory",
            QApplication::applicationDirPath()
        );
        if (!dir.isEmpty()) {
            nv::ApplicationState::instance().setNotesDirectory(dir);
        }
    });
    
    // WebDAV Sync action
    QAction* webdavAction = settingsMenu->addAction("WebDAV Sync");
    QObject::connect(webdavAction, &QAction::triggered, [win]() {
        nv::WebDAVConfigDialog dialog(win);
        dialog.setWindowTitle("WebDAV Sync Configuration");
        
        // Initialize with current settings
        nv::ApplicationState& state = nv::ApplicationState::instance();
        dialog.setEnabled(state.webdavEnabled());
        dialog.setServerAddress(state.webdavServerAddress());
        dialog.setUsername(state.webdavUsername());
        dialog.setPassword(state.webdavPassword());
        dialog.setSyncIntervalMinutes(state.webdavSyncIntervalMinutes());
        
        if (dialog.exec() == QDialog::Accepted) {
            // Apply settings
            state.setWebdavEnabled(dialog.isEnabled());
            state.setWebdavServerAddress(dialog.serverAddress());
            state.setWebdavUsername(dialog.username());
            state.setWebdavPassword(dialog.password());
            state.setWebdavSyncIntervalMinutes(dialog.syncIntervalMinutes());
        }
    });
    
    // Layout menu group
    QActionGroup* layoutGroup = new QActionGroup(settingsMenu);
    QAction* layoutVerticalAction = settingsMenu->addAction("Layout: Vertical");
    layoutVerticalAction->setCheckable(true);
    layoutVerticalAction->setChecked(false);  // Don't pre-check - will be set by setLayoutMode()
    layoutGroup->addAction(layoutVerticalAction);
    
    QAction* layoutHorizontalAction = settingsMenu->addAction("Layout: Horizontal");
    layoutHorizontalAction->setCheckable(true);
    layoutHorizontalAction->setChecked(false);  // Don't pre-check - will be set by setLayoutMode()
    layoutGroup->addAction(layoutHorizontalAction);
    
    QObject::connect(layoutVerticalAction, &QAction::triggered, [win, layoutVerticalAction]() {
        win->setLayoutMode(0);
    });
    QObject::connect(layoutHorizontalAction, &QAction::triggered, [win, layoutHorizontalAction]() {
        win->setLayoutMode(1);
    });
    
    // Theme menu group
    QActionGroup* themeGroup = new QActionGroup(settingsMenu);
    QAction* themeWhiteAction = settingsMenu->addAction("Theme: White");
    themeWhiteAction->setCheckable(true);
    themeWhiteAction->setChecked(false);  // Don't pre-check - will be set by setTheme()
    themeGroup->addAction(themeWhiteAction);
    
    QAction* themeBlackAction = settingsMenu->addAction("Theme: Black");
    themeBlackAction->setCheckable(true);
    themeBlackAction->setChecked(false);  // Don't pre-check - will be set by setTheme()
    themeGroup->addAction(themeBlackAction);
    
    QObject::connect(themeWhiteAction, &QAction::triggered, [win, themeWhiteAction]() {
        win->setTheme(0);
    });
    QObject::connect(themeBlackAction, &QAction::triggered, [win, themeBlackAction]() {
        win->setTheme(1);
    });
    
    // Store menu bar action pointers for later updates (layout mode)
    win->setMenuLayoutVerticalAction(layoutVerticalAction);
    win->setMenuLayoutHorizontalAction(layoutHorizontalAction);
    
    // Store menu bar action pointers for later updates (theme)
    win->setMenuThemeWhiteAction(themeWhiteAction);
    win->setMenuThemeBlackAction(themeBlackAction);
    
    // Edit menu with functional items
    QMenu* editMenu = menuBar->addMenu(QApplication::tr("&Edit"));
    QAction* undoAction = editMenu->addAction(QApplication::tr("Undo"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->undo();
        }
    });
    undoAction->setShortcuts(QKeySequence::Undo);
    
    QAction* redoAction = editMenu->addAction(QApplication::tr("Redo"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->redo();
        }
    });
    redoAction->setShortcuts(QKeySequence::Redo);
    
    editMenu->addSeparator();
    
    QAction* cutAction = editMenu->addAction(QApplication::tr("Cut"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->cut();
        }
    });
    cutAction->setShortcuts(QKeySequence::Cut);
    
    QAction* copyAction = editMenu->addAction(QApplication::tr("Copy"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->copy();
        }
    });
    copyAction->setShortcuts(QKeySequence::Copy);
    
    QAction* pasteAction = editMenu->addAction(QApplication::tr("Paste"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->paste();
        }
    });
    pasteAction->setShortcuts(QKeySequence::Paste);
    
    editMenu->addSeparator();
    
    QAction* selectAllAction = editMenu->addAction(QApplication::tr("Select All"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->selectAll();
        }
    });
    selectAllAction->setShortcuts(QKeySequence::SelectAll);
    
    // Help menu
    QMenu* helpMenu = menuBar->addMenu(QApplication::tr("&Help"));
    QAction* shortcutsAction = helpMenu->addAction(QApplication::tr("Shortcuts"));
    QObject::connect(shortcutsAction, &QAction::triggered, [win]() {
        nv::show_shortcuts_popup(win);
    });

    QAction* aboutAction = helpMenu->addAction(QApplication::tr("About"));
    QObject::connect(aboutAction, &QAction::triggered, [win]() {
        QMessageBox::about(win, "Notation V", 
            "A lightweight, keyboard-centric note-taking application.\n\n"
            "Version 1.0.0\n"
            "Built with Qt 6 and C++17");
    });
    
    win->setMenuBar(menuBar);
}

void add_settings_menu_impl(nv::MainWindow* win) {
    QMenuBar* menuBar = win->menuBar();
    if (!menuBar) {
        setup_linux_menu_bar_impl(win);
        return;
    }
    
    // Find or create Settings menu
    QMenu* settingsMenu = nullptr;
    for (QMenu* menu : menuBar->findChildren<QMenu*>()) {
        if (menu->title() == "&Settings") {
            settingsMenu = menu;
            break;
        }
    }
    
    if (!settingsMenu) {
        settingsMenu = menuBar->addMenu(QApplication::tr("&Settings"));
    }
    
    // WebDAV Sync action is already added in setup_linux_menu_bar_impl
    Q_UNUSED(settingsMenu);
    
    // Add settings menu items if not already added
    bool hasLayoutAction = false;
    for (QAction* action : settingsMenu->actions()) {
        if (action->text().contains("Layout:")) {
            hasLayoutAction = true;
            break;
        }
    }
    
    if (!hasLayoutAction) {
        // Layout menu group
        QActionGroup* layoutGroup = new QActionGroup(settingsMenu);
        QAction* layoutVerticalAction = settingsMenu->addAction("Layout: Vertical");
        layoutVerticalAction->setCheckable(true);
        layoutVerticalAction->setChecked(false);  // Don't pre-check - will be set by setLayoutMode()
        layoutGroup->addAction(layoutVerticalAction);
        
        QAction* layoutHorizontalAction = settingsMenu->addAction("Layout: Horizontal");
        layoutHorizontalAction->setCheckable(true);
        layoutHorizontalAction->setChecked(false);  // Don't pre-check - will be set by setLayoutMode()
        layoutGroup->addAction(layoutHorizontalAction);
        
        QObject::connect(layoutVerticalAction, &QAction::triggered, [win, layoutVerticalAction]() {
            win->setLayoutMode(0);
        });
        QObject::connect(layoutHorizontalAction, &QAction::triggered, [win, layoutHorizontalAction]() {
            win->setLayoutMode(1);
        });
        
        // Theme menu group
        QActionGroup* themeGroup = new QActionGroup(settingsMenu);
        QAction* themeWhiteAction = settingsMenu->addAction("Theme: White");
        themeWhiteAction->setCheckable(true);
        themeWhiteAction->setChecked(false);  // Don't pre-check - will be set by setTheme()
        themeGroup->addAction(themeWhiteAction);
        
        QAction* themeBlackAction = settingsMenu->addAction("Theme: Black");
        themeBlackAction->setCheckable(true);
        themeBlackAction->setChecked(false);  // Don't pre-check - will be set by setTheme()
        themeGroup->addAction(themeBlackAction);
        
        QObject::connect(themeWhiteAction, &QAction::triggered, [win, themeWhiteAction]() {
            win->setTheme(0);
        });
        QObject::connect(themeBlackAction, &QAction::triggered, [win, themeBlackAction]() {
            win->setTheme(1);
        });
        
        // Store menu bar action pointers for later updates
        win->setMenuLayoutVerticalAction(layoutVerticalAction);
        win->setMenuLayoutHorizontalAction(layoutHorizontalAction);
        win->setMenuThemeWhiteAction(themeWhiteAction);
        win->setMenuThemeBlackAction(themeBlackAction);
    }
}

} // anonymous namespace

namespace nv {

void setup_linux_menu_bar(nv::MainWindow* win) {
    setup_linux_menu_bar_impl(win);
}

void add_settings_menu(nv::MainWindow* win) {
    add_settings_menu_impl(win);
}

} // namespace nv