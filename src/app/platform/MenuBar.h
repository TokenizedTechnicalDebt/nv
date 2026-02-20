#ifndef NV_PLATFORM_MENUBAR_H
#define NV_PLATFORM_MENUBAR_H

#include <QMainWindow>
#include <QAction>

#include "nv/main_window.h"
#include "nv/webdav_config_dialog.h"

namespace nv {

#ifdef Q_OS_MACOS
void setup_macos_menu_bar(QMainWindow* win);
#endif

#ifdef Q_OS_LINUX
void setup_linux_menu_bar(nv::MainWindow* win);
#endif

#ifdef Q_OS_WINDOWS
void setup_windows_menu_bar(nv::MainWindow* win);
#endif

// Common function to add settings menu across platforms
void add_settings_menu(nv::MainWindow* win);

// WebDAV configuration action
void add_webdav_config_action(nv::MainWindow* win, QAction*& action);

} // namespace nv

#endif // NV_PLATFORM_MENUBAR_H
