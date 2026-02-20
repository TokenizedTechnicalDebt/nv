#include <QApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QSettings>

#include "nv/main_window.h"
#include "nv/application_controller.h"
#include "nv/note_store.h"
#include "nv/storage.h"
#include "nv/app_state.h"
#include "nv/webdav_sync_manager.h"
#include "platform/MenuBar.h"

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);

    // Setup logging
#ifdef NV_DEBUG
    QLoggingCategory::setFilterRules("nv.core.warning=true");
#endif

    // Initialize application state
    auto& appState = nv::ApplicationState::instance();

    // Create storage
    const QString notesDir = appState.notesDirectory();
    QDir().mkpath(notesDir);
    auto storage = std::make_unique<nv::LocalStorage>(notesDir);

    // Create note store
    auto noteStore = std::make_unique<nv::NoteStore>();

    // Create main window
    nv::MainWindow window;
#ifdef Q_OS_MACOS
    nv::setup_macos_menu_bar(&window);
#elif defined(Q_OS_LINUX)
    nv::setup_linux_menu_bar(&window);
#else
    nv::setup_windows_menu_bar(&window);
#endif

    // Wire store and storage to the window's note list model
    window.setNoteStore(noteStore.get());
    window.setStorage(storage.get());

    // Create application controller
    nv::ApplicationController controller(&window, noteStore.get(), storage.get());

    // Create and configure WebDAV sync manager
    auto webdavManager = std::make_unique<nv::WebDAVSyncManager>(noteStore.get(), storage.get(), &controller);
    
    // Configure from application state
    if (appState.webdavEnabled()) {
        webdavManager->setServerAddress(appState.webdavServerAddress());
        webdavManager->setUsername(appState.webdavUsername());
        webdavManager->setPassword(appState.webdavPassword());
        webdavManager->setSyncIntervalMinutes(appState.webdavSyncIntervalMinutes());
        
        // Enable WebDAV sync (must be done before syncStart())
        webdavManager->setEnabled(true);
        
        // Start periodic sync first (this creates webdav_storage_)
        webdavManager->syncStart();
        
        // Perform initial sync on startup
        webdavManager->syncNow();
    }
    
    // Set WebDAV manager in controller (for search-triggered sync)
    controller.setWebDAVSyncManager(webdavManager.get());
    
    // Set WebDAV manager in note editor (for bi-directional sync on save)
    if (auto* editor = window.noteEditor()) {
        editor->setWebDAVSyncManager(webdavManager.get());
    }

    // Show window
    window.show();

    return app.exec();
}
