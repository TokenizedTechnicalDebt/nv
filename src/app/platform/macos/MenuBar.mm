#include <AppKit/AppKit.h>
#include <QMainWindow>
#include <QAction>
#include <QMenuBar>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QKeySequence>
#include <QMessageBox>

#include "nv/main_window.h"
#include "nv/note_editor.h"
#include "nv/app_state.h"
#include "nv/webdav_config_dialog.h"
#include "../ShortcutsHelp.h"

namespace {

void setup_macos_menu_bar_impl(QMainWindow* win) {
    // Create system menu bar using AppKit
    NSMenu* mainMenu = [[NSMenu alloc] init];
    
    // About menu
    NSMenuItem* aboutItem = [[NSMenuItem alloc] initWithTitle:@"About Notation V"
                                                       action:@selector(about:)
                                                keyEquivalent:@""];
    [aboutItem setTarget:[NSApp delegate]];
    
    // Preferences menu
    NSMenuItem* prefsItem = [[NSMenuItem alloc] initWithTitle:@"Preferences..."
                                                       action:@selector(preferences:)
                                                keyEquivalent:@","];
    [prefsItem setTarget:[NSApp delegate]];
    
    // Help menu
    NSMenuItem* helpItem = [[NSMenuItem alloc] initWithTitle:@"Help"
                                                      action:@selector(help:)
                                               keyEquivalent:@""];
    [helpItem setTarget:[NSApp delegate]];
    
    // Quit menu - Cmd+Q on macOS
    // Use QApp's quit method via QAction since we can't use [NSApp delegate]
    NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit Notation V"
                                                      action:nil
                                               keyEquivalent:@"q"];
    [quitItem setKeyEquivalentModifierMask:NSCommandKeyMask];
    [quitItem setTarget:nil];
    
    // Create menus
    NSMenu* appleMenu = [[NSMenu alloc] initWithTitle:@""];
    [appleMenu addItem:aboutItem];
    [appleMenu addItem:prefsItem];
    
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@""];
    [appMenu addItem:appleMenu];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItem:quitItem];
    
    NSMenu* helpMenu = [[NSMenu alloc] initWithTitle:@""];
    [helpMenu addItem:helpItem];
    
    NSMenuItem* mainMenuItem = [[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""] autorelease];
    [mainMenuItem setSubmenu:appMenu];
    [mainMenu addItem:mainMenuItem];
    
    NSMenuItem* helpMenuItem = [[[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""] autorelease];
    [helpMenuItem setSubmenu:helpMenu];
    [mainMenu addItem:helpMenuItem];
    
    [NSApp setMainMenu:mainMenu];
    
    // Setup dock icon
    [[NSApplication sharedApplication] setApplicationIconImage:
        [[NSImage alloc] initByReferencingFile:@"nv.icns"]];
    
    // Setup macOS menu bar actions for Edit menu (Undo, Redo, Cut, Copy, Paste, Select All)
    // These need to be connected to the editor's methods
    QMenuBar* menuBar = new QMenuBar();
    menuBar->setNativeMenuBar(true);
    
    // Add quit action to menu bar (Cmd+Q)
    QAction* quitAction = new QAction(menuBar);
    quitAction->setShortcuts(QKeySequence::Quit);  // Cmd+Q on macOS
    QObject::connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
    menuBar->addAction(quitAction);
    
    // Edit menu with functional items (using Cmd key on macOS)
    QMenu* editMenu = menuBar->addMenu(QApplication::tr("&Edit"));
    
    QAction* undoAction = editMenu->addAction(QApplication::tr("Undo"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->undo();
        }
    });
    // Cmd+Z on macOS, Ctrl+Z on other platforms
    undoAction->setShortcut(QKeySequence::fromString("Meta+Z"));
    
    QAction* redoAction = editMenu->addAction(QApplication::tr("Redo"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->redo();
        }
    });
    redoAction->setShortcut(QKeySequence::fromString("Meta+Y"));
    
    editMenu->addSeparator();
    
    QAction* cutAction = editMenu->addAction(QApplication::tr("Cut"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->cut();
        }
    });
    cutAction->setShortcut(QKeySequence::fromString("Meta+X"));
    
    QAction* copyAction = editMenu->addAction(QApplication::tr("Copy"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->copy();
        }
    });
    copyAction->setShortcut(QKeySequence::fromString("Meta+C"));
    
    QAction* pasteAction = editMenu->addAction(QApplication::tr("Paste"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->paste();
        }
    });
    pasteAction->setShortcut(QKeySequence::fromString("Meta+V"));
    
    editMenu->addSeparator();
    
    QAction* selectAllAction = editMenu->addAction(QApplication::tr("Select All"), [win]() {
        if (win->noteEditor()) {
            win->noteEditor()->selectAll();
        }
    });
    selectAllAction->setShortcut(QKeySequence::fromString("Meta+A"));

    // Help menu
    QMenu* helpMenuQt = menuBar->addMenu(QApplication::tr("&Help"));
    QAction* shortcutsAction = helpMenuQt->addAction(QApplication::tr("Shortcuts"));
    QObject::connect(shortcutsAction, &QAction::triggered, [win]() {
        nv::show_shortcuts_popup(win);
    });

    QAction* aboutAction = helpMenuQt->addAction(QApplication::tr("About"));
    QObject::connect(aboutAction, &QAction::triggered, [win]() {
        QMessageBox::about(win, "Notation V",
            "A lightweight, keyboard-centric note-taking application.\n\n"
            "Version 1.0.0\n"
            "Built with Qt 6 and C++17");
    });
    
    win->setMenuBar(menuBar);
}

} // anonymous namespace

void nv::setup_macos_menu_bar(QMainWindow* win) {
    setup_macos_menu_bar_impl(win);
}

void nv::add_settings_menu(QMainWindow* win) {
    // Settings menu is already part of macOS menu bar
    // This is a no-op for macOS since we handle it in setup_macos_menu_bar_impl
}