#include "nv/webdav_sync_manager.h"
#include "nv/app_state.h"
#include <QDateTime>
#include <QXmlStreamReader>
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace nv {

namespace {
constexpr bool kWebDAVSyncDebugLogging = false;
}

WebDAVSyncManager::WebDAVSyncManager(INoteStore* noteStore, IStorage* storage, QObject* parent)
    : QObject(parent)
    , enabled_(false)
    , sync_interval_minutes_(5)
    , note_store_(noteStore)
    , storage_(storage)
    , pending_search_sync_(false) {
    
    // Setup sync timer
    connect(&sync_timer_, &QTimer::timeout, this, &WebDAVSyncManager::onSyncTimerTimeout);
    
    // Setup search debounce timer (500ms after user stops typing)
    search_debounce_timer_.setSingleShot(true);
    connect(&search_debounce_timer_, &QTimer::timeout, this, &WebDAVSyncManager::onSearchDebounceTimerTimeout);
}

WebDAVSyncManager::~WebDAVSyncManager() {
    syncStop();
}

void WebDAVSyncManager::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (enabled_) {
        syncStart();
    } else {
        syncStop();
    }
}

void WebDAVSyncManager::setServerAddress(const QString& address) {
    server_address_ = address;
    if (!server_address_.endsWith("/")) {
        server_address_ += "/";
    }
}

void WebDAVSyncManager::setUsername(const QString& username) {
    username_ = username;
}

void WebDAVSyncManager::setPassword(const QString& password) {
    password_ = password;
}

void WebDAVSyncManager::setSyncIntervalMinutes(int minutes) {
    sync_interval_minutes_ = std::max(1, minutes);  // Minimum 1 minute
}

std::unique_ptr<WebDAVStorage> WebDAVSyncManager::createWebDAVStorage() const {
    return std::make_unique<WebDAVStorage>(server_address_, username_, password_);
}

void WebDAVSyncManager::refreshConfigurationFromAppState() {
    auto& state = ApplicationState::instance();

    bool shouldEnable = state.webdavEnabled();
    QString serverAddress = state.webdavServerAddress();
    if (!serverAddress.isEmpty() && !serverAddress.endsWith("/")) {
        serverAddress += "/";
    }

    QString username = state.webdavUsername();
    QString password = state.webdavPassword();
    int syncIntervalMinutes = std::max(1, state.webdavSyncIntervalMinutes());

    const bool connectionConfigChanged =
        (server_address_ != serverAddress) ||
        (username_ != username) ||
        (password_ != password);

    enabled_ = shouldEnable;
    server_address_ = serverAddress;
    username_ = username;
    password_ = password;
    sync_interval_minutes_ = syncIntervalMinutes;

    if (!enabled_) {
        if (sync_timer_.isActive() || webdav_storage_) {
            syncStop();
        }
        return;
    }

    if (connectionConfigChanged || !webdav_storage_) {
        webdav_storage_ = createWebDAVStorage();
    }

    const int intervalMs = sync_interval_minutes_ * 60000;
    if (!sync_timer_.isActive() || sync_timer_.interval() != intervalMs) {
        sync_timer_.start(intervalMs);
    }
}

void WebDAVSyncManager::syncStart() {
    refreshConfigurationFromAppState();

    if (!enabled_ || sync_timer_.isActive()) {
        return;
    }
    
    // Create WebDAV storage with current configuration
    webdav_storage_ = createWebDAVStorage();
    
    // Start periodic sync timer
    sync_timer_.start(sync_interval_minutes_ * 60000);  // Convert to milliseconds
}

void WebDAVSyncManager::syncStop() {
    sync_timer_.stop();
    webdav_storage_.reset();
}

void WebDAVSyncManager::syncNow() {
    refreshConfigurationFromAppState();

    if (!enabled_) {
        return;
    }
    
    performSync();
}

void WebDAVSyncManager::triggerSyncOnSearch() {
    refreshConfigurationFromAppState();

    if (!enabled_) {
        return;
    }
    
    // If there's already a pending sync, don't reset the timer
    if (pending_search_sync_) {
        return;
    }
    
    pending_search_sync_ = true;
    
    // Wait 500ms after user stops typing before syncing
    search_debounce_timer_.start(500);
}

void WebDAVSyncManager::onSyncTimerTimeout() {
    performSync();
}

void WebDAVSyncManager::onSearchDebounceTimerTimeout() {
    if (pending_search_sync_) {
        performSync();
        pending_search_sync_ = false;
    }
}

void WebDAVSyncManager::performSync() {
    refreshConfigurationFromAppState();

    if (!enabled_) {
        return;
    }

    if (!webdav_storage_) {
        last_error_ = "WebDAV storage not initialized";
        qWarning() << "WebDAV sync error:" << last_error_;
        emit syncError(last_error_);
        emit syncFinished(false);
        return;
    }
    
    emit syncStarted();
    
    // Step 1: Get remote notes
    auto remoteNotes = getRemoteNoteTimestamps();
    if (kWebDAVSyncDebugLogging) {
        qInfo() << "WebDAV sync: retrieved" << remoteNotes.size() << "remote notes";
    }
    
    // Step 2: Download missing or updated notes from WebDAV
    // Note: downloadMissingOrUpdatedNotes handles empty remoteNotes gracefully
    downloadMissingOrUpdatedNotes(remoteNotes);
    
    // Step 3: Upload local notes that have changed
    uploadChangedNotes();
    
    // Update last sync time
    last_sync_time_ = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    
    emit syncFinished(true);
}

std::unordered_map<std::string, NoteTimestamp> WebDAVSyncManager::getRemoteNoteTimestamps() {
    std::unordered_map<std::string, NoteTimestamp> result;
    
    if (!webdav_storage_) {
        return result;
    }
    
    // Use the existing readAllNotes method which uses PROPFIND
    auto notesResult = webdav_storage_->readAllNotes();
    if (nv::isSuccess(notesResult)) {
        auto notes = nv::getSuccess(notesResult);
        if (kWebDAVSyncDebugLogging) {
            qInfo() << "WebDAV sync: read" << notes.size() << "notes from storage";
        }
        for (const auto& note : notes) {
            result[note->uuid()] = note->modified();
        }
    } else {
        qWarning() << "WebDAV sync: failed to read notes from storage";
    }
    
    return result;
}

void WebDAVSyncManager::downloadMissingOrUpdatedNotes(const std::unordered_map<std::string, NoteTimestamp>& remoteNotes) {
    if (!note_store_ || !webdav_storage_) {
        return;
    }
    
    // Get all local notes
    auto localNotes = note_store_->getAllNotes();
    std::unordered_map<std::string, NoteTimestamp> localTimestamps;
    
    for (const auto& note : localNotes) {
        localTimestamps[note->uuid()] = note->modified();
    }
    
    // Fetch all remote notes once (instead of fetching for each comparison)
    auto allRemoteNotes = webdav_storage_->readAllNotes();
    std::vector<std::shared_ptr<Note>> remoteNoteList;
    if (nv::isSuccess(allRemoteNotes)) {
        remoteNoteList = nv::getSuccess(allRemoteNotes);
    }
    
    // Tolerance: 3 seconds
    const auto tolerance = std::chrono::seconds(3);
    
    // Check each remote note
    for (const auto& [uuid, remoteTime] : remoteNotes) {
        auto localIt = localTimestamps.find(uuid);
        
        if (localIt == localTimestamps.end()) {
            // Note doesn't exist locally - download it
            if (kWebDAVSyncDebugLogging) {
                qInfo() << "WebDAV sync: note" << QString::fromStdString(uuid) << "not found locally, downloading from remote";
            }
            for (const auto& note : remoteNoteList) {
                if (note->uuid() == uuid) {
                    note_store_->addNote(note);
                    // Save to local storage as well
                    auto saveResult = storage_->writeNote(*note);
                    if (nv::isSuccess(saveResult)) {
                        if (kWebDAVSyncDebugLogging) {
                            qInfo() << "WebDAV sync: saved downloaded note to local storage" << uuid.c_str();
                        }
                    } else {
                        qWarning() << "WebDAV sync: failed to save downloaded note to local storage" << uuid.c_str();
                    }
                    emit noteDownloaded(note);
                    break;
                }
            }
        } else {
            // Note exists locally - check if remote is newer
            auto localTime = localIt->second;
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(remoteTime - localTime);
            
            if (remoteTime > localTime + tolerance) {
                // Remote is newer - download it
                if (kWebDAVSyncDebugLogging) {
                    qInfo() << "WebDAV sync: remote note" << QString::fromStdString(uuid)
                            << "is" << (diff.count() / 1000.0) << "seconds newer, downloading";
                }
                for (const auto& note : remoteNoteList) {
                    if (note->uuid() == uuid) {
                        note_store_->updateNote(note);
                        // Save to local storage as well
                        auto saveResult = storage_->writeNote(*note);
                        if (nv::isSuccess(saveResult)) {
                            if (kWebDAVSyncDebugLogging) {
                                qInfo() << "WebDAV sync: saved updated note to local storage" << uuid.c_str();
                            }
                        } else {
                            qWarning() << "WebDAV sync: failed to save updated note to local storage" << uuid.c_str();
                        }
                        emit noteDownloaded(note);
                        break;
                    }
                }
            } else {
                if (kWebDAVSyncDebugLogging) {
                    qInfo() << "WebDAV sync: note" << QString::fromStdString(uuid)
                            << "- remote time within" << (diff.count() / 1000.0) << "seconds of local, skipping download";
                }
            }
        }
    }
}

void WebDAVSyncManager::uploadChangedNotes() {
    if (!note_store_ || !storage_) {
        return;
    }
    
    // Get remote notes timestamps to compare
    auto remoteNotes = getRemoteNoteTimestamps();
    
    // Get all local notes
    auto localNotes = note_store_->getAllNotes();
    
    // Tolerance: 3 seconds
    //const auto tolerance = std::chrono::seconds(3);
    const auto tolerance = std::chrono::seconds(0);

    
    for (const auto& localNote : localNotes) {
        auto remoteIt = remoteNotes.find(localNote->uuid());
        
        if (remoteIt == remoteNotes.end()) {
            // Note doesn't exist on WebDAV - upload it
            if (kWebDAVSyncDebugLogging) {
                qInfo() << "WebDAV sync: uploading note" << QString::fromStdString(localNote->uuid()) << "- note does not exist on remote";
            }
            uploadNote(*localNote);
        } else {
            // Note exists on WebDAV - only upload if local is newer (with tolerance)
            auto localTime = localNote->modified();
            auto remoteTime = remoteIt->second;
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(localTime - remoteTime);
            
            if (kWebDAVSyncDebugLogging) {
                qInfo() << "WebDAV sync: comparing note" << QString::fromStdString(localNote->uuid())
                        << "- local:" << localTime.time_since_epoch().count()
                        << "remote:" << remoteTime.time_since_epoch().count()
                        << "diff:" << diff.count() << "ms";
            }
            
            if (localTime > remoteTime + tolerance) {
                if (kWebDAVSyncDebugLogging) {
                    qInfo() << "WebDAV sync: uploading note" << QString::fromStdString(localNote->uuid())
                            << "- local time is" << (diff.count() / 1000.0) << "seconds newer than remote";
                }
                uploadNote(*localNote);
            } else if (localTime < remoteTime - tolerance) {
                if (kWebDAVSyncDebugLogging) {
                    qInfo() << "WebDAV sync: note" << QString::fromStdString(localNote->uuid())
                            << "- remote is newer, will download on next sync";
                }
            } else {
                if (kWebDAVSyncDebugLogging) {
                    qInfo() << "WebDAV sync: note" << QString::fromStdString(localNote->uuid())
                            << "- times are within tolerance (" << diff.count() << "ms), skipping upload";
                }
            }
        }
    }
}

void WebDAVSyncManager::uploadNote(const Note& note) {
    if (!webdav_storage_) {
        return;
    }
    
    // Upload the note
    auto result = webdav_storage_->writeNote(note);
    
    if (nv::isSuccess(result)) {
        if (kWebDAVSyncDebugLogging) {
            qInfo() << "WebDAV sync: uploaded note" << note.uuid().c_str();
        }
        emit noteUploaded(note_store_->getNote(note.uuid()));
    } else {
        qWarning() << "WebDAV sync: failed to upload note" << note.uuid().c_str();
    }
}

bool WebDAVSyncManager::resolveConflict(const Note& localNote, const Note& remoteNote) {
    // Conflict resolution: use the note with the later updatedAt timestamp
    return remoteNote.updatedAtMillis() > localNote.updatedAtMillis();
}

std::vector<std::shared_ptr<Note>> WebDAVSyncManager::listRemoteNotes() {
    if (!webdav_storage_) {
        return {};
    }
    
    auto result = webdav_storage_->readAllNotes();
    if (nv::isSuccess(result)) {
        return nv::getSuccess(result);
    }
    
    return {};
}

bool WebDAVSyncManager::isNoteFile(const QString& fileName) {
    // Check if file has .json extension (note files)
    return fileName.endsWith(".json", Qt::CaseInsensitive);
}

bool WebDAVSyncManager::checkRemoteNoteForUpdate(const std::string& uuid) {
    if (!webdav_storage_ || !note_store_) {
        return false;
    }
    
    // Get local note
    auto localNote = note_store_->getNote(uuid);
    if (!localNote) {
        return false;  // Note doesn't exist locally
    }
    
    // Get remote notes to check
    auto remoteNotesResult = webdav_storage_->readAllNotes();
    if (!nv::isSuccess(remoteNotesResult)) {
        return false;
    }
    
    auto remoteNotes = nv::getSuccess(remoteNotesResult);
    std::shared_ptr<Note> remoteNote;
    for (const auto& note : remoteNotes) {
        if (note->uuid() == uuid) {
            remoteNote = note;
            break;
        }
    }
    
    if (!remoteNote) {
        return false;  // Note doesn't exist on remote
    }
    
    // Compare timestamps with 3-second tolerance
    const auto tolerance = std::chrono::seconds(3);
    bool isRemoteNewer = remoteNote->modified() > localNote->modified() + tolerance;
    
    // Emit signal with the check result
    emit remoteNoteChecked(localNote, remoteNote, isRemoteNewer);
    
    return isRemoteNewer;
}

void WebDAVSyncManager::downloadNoteIfRemoteNewer(const std::string& uuid) {
    if (!webdav_storage_ || !note_store_) {
        return;
    }
    
    // Get local note
    auto localNote = note_store_->getNote(uuid);
    if (!localNote) {
        return;  // Note doesn't exist locally
    }
    
    // Get remote note
    auto remoteNotesResult = webdav_storage_->readAllNotes();
    if (!nv::isSuccess(remoteNotesResult)) {
        qWarning() << "WebDAV sync: failed to read remote notes";
        return;
    }
    
    auto remoteNotes = nv::getSuccess(remoteNotesResult);
    std::shared_ptr<Note> remoteNote;
    for (const auto& note : remoteNotes) {
        if (note->uuid() == uuid) {
            remoteNote = note;
            break;
        }
    }
    
    if (!remoteNote) {
        return;  // Note doesn't exist on remote
    }
    
    // Compare timestamps with 3-second tolerance
    const auto tolerance = std::chrono::seconds(3);
    if (remoteNote->modified() > localNote->modified() + tolerance) {
        // Remote is newer - download it
        if (kWebDAVSyncDebugLogging) {
            qInfo() << "WebDAV sync: remote note" << QString::fromStdString(uuid)
                    << "is newer, updating local copy";
        }
        note_store_->updateNote(remoteNote);
        auto saveResult = storage_->writeNote(*remoteNote);
        if (nv::isSuccess(saveResult)) {
            if (kWebDAVSyncDebugLogging) {
                qInfo() << "WebDAV sync: updated local note from WebDAV" << uuid.c_str();
            }
        } else {
            qWarning() << "WebDAV sync: failed to save updated note" << uuid.c_str();
        }
    } else {
        if (kWebDAVSyncDebugLogging) {
            qInfo() << "WebDAV sync: local note" << QString::fromStdString(uuid)
                    << "is up to date";
        }
    }
}

std::unordered_map<std::string, NoteTimestamp> WebDAVSyncManager::parsePropfindResponse(const std::string& xmlResponse) {
    std::unordered_map<std::string, NoteTimestamp> result;
    
    QXmlStreamReader reader(QString::fromStdString(xmlResponse));
    
    std::string currentHref;
    std::string currentModified;
    bool inHref = false;
    bool inModified = false;
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString();
            
            if (name == "href") {
                inHref = true;
                currentHref.clear();
            } else if (name == "getlastmodified") {
                inModified = true;
                currentModified.clear();
            }
        } else if (reader.isEndElement()) {
            QString name = reader.name().toString();
            
            if (name == "href") {
                inHref = false;
            } else if (name == "getlastmodified") {
                inModified = false;
                
                // Parse the modification time
                // Format: "Sun, 06 Nov 1994 08:49:37 GMT"
                if (!currentModified.empty()) {
                    QDateTime modified = QDateTime::fromString(
                        QString::fromStdString(currentModified),
                        "ddd, dd MMM yyyy HH:mm:ss 'GMT'"
                    );
                    
                    if (modified.isValid()) {
                        // Extract UUID from href
                        // Format: http://server/path/uuid.json
                        if (!currentHref.empty()) {
                            QUrl url(QString::fromStdString(currentHref));
                            QString path = url.path();
                            int lastSlash = path.lastIndexOf('/');
                            if (lastSlash >= 0) {
                                QString fileName = path.mid(lastSlash + 1);
                                if (isNoteFile(fileName)) {
                                    // Remove .json extension to get UUID
                                    QString uuid = fileName.left(fileName.length() - 5);
                                    auto timePoint = std::chrono::system_clock::from_time_t(
                                        modified.toSecsSinceEpoch()
                                    );
                                    result[uuid.toStdString()] = timePoint;
                                }
                            }
                        }
                    }
                }
            }
        } else if (reader.isCharacters() && !reader.isWhitespace()) {
            if (inHref) {
                currentHref += reader.text().toString().toStdString();
            } else if (inModified) {
                currentModified += reader.text().toString().toStdString();
            }
        }
    }
    
    return result;
}

std::string WebDAVSyncManager::extractTagContent(const std::string& xml, const std::string& tag) const {
    std::string openingTag = "<" + tag + ">";
    std::string closingTag = "</" + tag + ">";
    
    size_t start = xml.find(openingTag);
    if (start == std::string::npos) {
        return "";
    }
    
    start += openingTag.length();
    size_t end = xml.find(closingTag, start);
    if (end == std::string::npos) {
        return "";
    }
    
    return xml.substr(start, end - start);
}

std::vector<std::string> WebDAVSyncManager::extractAllTagContents(const std::string& xml, const std::string& tag) const {
    std::vector<std::string> results;
    std::string openingTag = "<" + tag + ">";
    std::string closingTag = "</" + tag + ">";
    
    size_t pos = 0;
    while (true) {
        size_t start = xml.find(openingTag, pos);
        if (start == std::string::npos) {
            break;
        }
        
        start += openingTag.length();
        size_t end = xml.find(closingTag, start);
        if (end == std::string::npos) {
            break;
        }
        
        results.push_back(xml.substr(start, end - start));
        pos = end + closingTag.length();
    }
    
    return results;
}

} // namespace nv