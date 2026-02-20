#include "nv/app_state.h"
#include <QStandardPaths>

namespace nv {

ApplicationState& ApplicationState::instance() {
    static ApplicationState instance;
    return instance;
}

ApplicationState::ApplicationState()
    : settings_("NotationalVelocity", "nv")
    , notes_directory_(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Notes")
    , auto_save_delay_(500)
    , font_size_(12)
    , show_previews_(false)
    , layout_mode_(0)
    , theme_(0)
    , splitter_state_(QByteArray())
    , webdav_enabled_(false)
    , webdav_sync_interval_minutes_(5) {
    
    // Load settings
    notes_directory_ = settings_.value("NV/notesDirectory", notes_directory_).toString();
    auto_save_delay_ = settings_.value("NV/autoSaveDelay", auto_save_delay_).toInt();
    font_size_ = settings_.value("NV/fontSize", font_size_).toInt();
    show_previews_ = settings_.value("NV/showPreviews", show_previews_).toBool();
    layout_mode_ = settings_.value("NV/layoutMode", 0).toInt();
    theme_ = settings_.value("NV/theme", 0).toInt();
    splitter_state_ = settings_.value("NV/splitterState").toByteArray();
    
    // Load WebDAV settings
    webdav_enabled_ = settings_.value("NV/webdavEnabled", false).toBool();
    webdav_server_address_ = settings_.value("NV/webdavServerAddress", "").toString();
    webdav_username_ = settings_.value("NV/webdavUsername", "").toString();
    webdav_password_ = settings_.value("NV/webdavPassword", "").toString();
    webdav_sync_interval_minutes_ = settings_.value("NV/webdavSyncIntervalMinutes", 5).toInt();
}

QString ApplicationState::notesDirectory() const {
    return notes_directory_;
}

void ApplicationState::setNotesDirectory(const QString& path) {
    notes_directory_ = path;
    settings_.setValue("NV/notesDirectory", path);
}

int ApplicationState::autoSaveDelay() const {
    return auto_save_delay_;
}

int ApplicationState::fontSize() const {
    return font_size_;
}

bool ApplicationState::showPreviews() const {
    return show_previews_;
}

int ApplicationState::layoutMode() const {
    return layout_mode_;
}

void ApplicationState::setLayoutMode(int mode) {
    layout_mode_ = mode;
    settings_.setValue("NV/layoutMode", mode);
}

int ApplicationState::theme() const {
    return theme_;
}

void ApplicationState::setTheme(int theme) {
    theme_ = theme;
    settings_.setValue("NV/theme", theme);
}

QByteArray ApplicationState::splitterState() const {
    return splitter_state_;
}

void ApplicationState::setSplitterState(const QByteArray& state) {
    splitter_state_ = state;
    settings_.setValue("NV/splitterState", state);
    settings_.sync();
}

// WebDAV settings getters and setters
bool ApplicationState::webdavEnabled() const {
    return webdav_enabled_;
}

void ApplicationState::setWebdavEnabled(bool enabled) {
    webdav_enabled_ = enabled;
    settings_.setValue("NV/webdavEnabled", enabled);
    settings_.sync();
}

QString ApplicationState::webdavServerAddress() const {
    return webdav_server_address_;
}

void ApplicationState::setWebdavServerAddress(const QString& address) {
    webdav_server_address_ = address;
    settings_.setValue("NV/webdavServerAddress", address);
    settings_.sync();
}

QString ApplicationState::webdavUsername() const {
    return webdav_username_;
}

void ApplicationState::setWebdavUsername(const QString& username) {
    webdav_username_ = username;
    settings_.setValue("NV/webdavUsername", username);
    settings_.sync();
}

QString ApplicationState::webdavPassword() const {
    return webdav_password_;
}

void ApplicationState::setWebdavPassword(const QString& password) {
    webdav_password_ = password;
    settings_.setValue("NV/webdavPassword", password);
    settings_.sync();
}

int ApplicationState::webdavSyncIntervalMinutes() const {
    return webdav_sync_interval_minutes_;
}

void ApplicationState::setWebdavSyncIntervalMinutes(int minutes) {
    webdav_sync_interval_minutes_ = minutes;
    settings_.setValue("NV/webdavSyncIntervalMinutes", minutes);
    settings_.sync();
}

} // namespace nv
