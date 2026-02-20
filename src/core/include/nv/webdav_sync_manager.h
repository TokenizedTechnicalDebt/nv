#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "nv/storage.h"
#include "nv/note_model.h"
#include "nv/note_store.h"

namespace nv {

class WebDAVSyncManager : public QObject {
    Q_OBJECT

public:
    explicit WebDAVSyncManager(INoteStore* noteStore, IStorage* storage, QObject* parent = nullptr);
    ~WebDAVSyncManager() override;
    
    // Configuration
    void setEnabled(bool enabled);
    void setServerAddress(const QString& address);
    void setUsername(const QString& username);
    void setPassword(const QString& password);
    void setSyncIntervalMinutes(int minutes);
    
    // Sync status
    bool isEnabled() const { return enabled_; }
    QString lastError() const { return last_error_; }
    QString lastSyncTime() const { return last_sync_time_; }
    int syncIntervalMinutes() const { return sync_interval_minutes_; }
    
    // Sync operations
    void syncStart();  // Start periodic sync
    void syncStop();   // Stop periodic sync
    void syncNow();    // Perform immediate sync
    void triggerSyncOnSearch();  // Sync when user stops typing (debounced)
    
    // Sync direction
    void uploadNote(const Note& note);  // Upload single note to WebDAV
    void downloadNotes();  // Download all notes from WebDAV
    
    // Conflict resolution
    static bool resolveConflict(const Note& localNote, const Note& remoteNote);
    
    // File listing
    std::vector<std::shared_ptr<Note>> listRemoteNotes();
    std::unordered_map<std::string, NoteTimestamp> getRemoteNoteTimestamps();
    
    // Check if a file is a valid note file
    static bool isNoteFile(const QString& fileName);
    
    // Check if a note has a newer version on WebDAV
    // Returns true if WebDAV has a newer version, false otherwise
    // Note: This is a blocking call that performs network I/O
    bool checkRemoteNoteForUpdate(const std::string& uuid);
    
    // Download and update a note from WebDAV if remote is newer
    void downloadNoteIfRemoteNewer(const std::string& uuid);

signals:
    void syncStarted();
    void syncFinished(bool success);
    void syncError(const QString& error);
    void noteUploaded(std::shared_ptr<Note> note);
    void noteDownloaded(std::shared_ptr<Note> note);
    
    // Emitted after checking a remote note
    void remoteNoteChecked(std::shared_ptr<Note> localNote, std::shared_ptr<Note> remoteNote, bool isRemoteNewer);

private slots:
    void onSyncTimerTimeout();
    void onSearchDebounceTimerTimeout();

private:
    // Refresh runtime configuration from persisted app settings.
    // Recreates WebDAV storage if credentials/server changed.
    void refreshConfigurationFromAppState();

    // Create WebDAVStorage with current configuration
    std::unique_ptr<WebDAVStorage> createWebDAVStorage() const;
    
    // Sync helpers
    void performSync();
    void downloadMissingOrUpdatedNotes(const std::unordered_map<std::string, NoteTimestamp>& remoteNotes);
    void uploadChangedNotes();
    
    // PROPFIND response parsing
    std::unordered_map<std::string, NoteTimestamp> parsePropfindResponse(const std::string& xmlResponse);
    
    // XML helpers
    std::string extractTagContent(const std::string& xml, const std::string& tag) const;
    std::vector<std::string> extractAllTagContents(const std::string& xml, const std::string& tag) const;
    
    // Configuration
    bool enabled_;
    QString server_address_;
    QString username_;
    QString password_;
    int sync_interval_minutes_;
    
    // State
    mutable std::mutex mutex_;
    std::unique_ptr<WebDAVStorage> webdav_storage_;
    INoteStore* note_store_;
    IStorage* storage_;
    
    // Timers
    QTimer sync_timer_;
    QTimer search_debounce_timer_;
    
    // Status
    QString last_error_;
    QString last_sync_time_;
    
    // Search debounce tracking
    bool pending_search_sync_;
};

} // namespace nv