#pragma once

#include <QString>
#include <QSettings>

namespace nv {

class ApplicationState {
public:
    static ApplicationState& instance();
    
    [[nodiscard]] QString notesDirectory() const;
    void setNotesDirectory(const QString& path);
    [[nodiscard]] int autoSaveDelay() const;
    [[nodiscard]] int fontSize() const;
    [[nodiscard]] bool showPreviews() const;
    
    // Layout mode: 0 = vertical (default), 1 = horizontal (landscape)
    [[nodiscard]] int layoutMode() const;
    void setLayoutMode(int mode);
    
    // Theme: 0 = white (default), 1 = black
    [[nodiscard]] int theme() const;
    void setTheme(int theme);
    
    // Splitter proportions (horizontal and vertical)
    [[nodiscard]] QByteArray splitterState() const;
    void setSplitterState(const QByteArray& state);
    
    // WebDAV sync settings
    [[nodiscard]] bool webdavEnabled() const;
    void setWebdavEnabled(bool enabled);
    [[nodiscard]] QString webdavServerAddress() const;
    void setWebdavServerAddress(const QString& address);
    [[nodiscard]] QString webdavUsername() const;
    void setWebdavUsername(const QString& username);
    [[nodiscard]] QString webdavPassword() const;
    void setWebdavPassword(const QString& password);
    [[nodiscard]] int webdavSyncIntervalMinutes() const;
    void setWebdavSyncIntervalMinutes(int minutes);
    
private:
    ApplicationState();
    QSettings settings_;
    QString notes_directory_;
    int auto_save_delay_;
    int font_size_;
    bool show_previews_;
    int layout_mode_;
    int theme_;
    QByteArray splitter_state_;
    
    // WebDAV settings
    bool webdav_enabled_;
    QString webdav_server_address_;
    QString webdav_username_;
    QString webdav_password_;
    int webdav_sync_interval_minutes_;
};

} // namespace nv
