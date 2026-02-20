#pragma once

#include <string>
#include <variant>
#include <vector>
#include <memory>
#include <filesystem>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

#include "note_model.h"

namespace nv {

enum class StorageError {
    None,
    ReadFailed,
    WriteFailed,
    CorruptFile,
    DiskFull
};

enum class WebDAVError {
    None,
    NetworkError,
    AuthenticationFailed,
    NotFound,
    Timeout,
    CorruptJSON
};

struct SuccessType {};

template<typename T>
using Result = std::variant<T, StorageError>;

template<typename T>
inline bool isSuccess(const Result<T>& r) {
    return std::holds_alternative<T>(r);
}

template<typename T>
inline const T& getSuccess(const Result<T>& r) {
    return std::get<T>(r);
}

// Special case for Result<void> - use a tag type instead of void
using VoidResult = std::variant<SuccessType, StorageError>;

inline bool isSuccess(const VoidResult& r) {
    return std::holds_alternative<SuccessType>(r);
}

// WebDAV result type
template<typename T>
using WebDAVResult = std::variant<T, WebDAVError>;

template<typename T>
inline bool isSuccess(const WebDAVResult<T>& r) {
    return std::holds_alternative<T>(r);
}

template<typename T>
inline const T& getSuccess(const WebDAVResult<T>& r) {
    return std::get<T>(r);
}

class IStorage {
public:
    virtual ~IStorage() = default;
    virtual Result<std::vector<std::shared_ptr<Note>>> readAllNotes() = 0;
    virtual VoidResult writeNote(const Note& note) = 0;
    virtual VoidResult deleteNote(const NoteUUID& uuid) = 0;
};

class LocalStorage : public IStorage {
public:
    explicit LocalStorage(const QString& directory);
    Result<std::vector<std::shared_ptr<Note>>> readAllNotes() override;
    VoidResult writeNote(const Note& note) override;
    VoidResult deleteNote(const NoteUUID& uuid) override;
    
private:
    QString directory_;
    QString notePath(const NoteUUID& uuid) const;
    std::string readFile(const QString& path) const;
    void writeFile(const QString& path, const std::string& content) const;
};

class WebDAVStorage : public IStorage {
public:
    WebDAVStorage(const QString& serverAddress, const QString& username, const QString& password);
    ~WebDAVStorage() override;
    
    Result<std::vector<std::shared_ptr<Note>>> readAllNotes() override;
    VoidResult writeNote(const Note& note) override;
    VoidResult deleteNote(const NoteUUID& uuid) override;
    
    // Test connection
    bool testConnection() const;
    
    // Sync status
    QString lastError() const;
    
private:
    QString serverAddress_;
    QString username_;
    QString password_;
    mutable QNetworkAccessManager* manager_;
    
    QString buildUrl(const QString& fileName) const;
    std::string sendRequest(const QString& url, const QString& method, 
                           const std::string& body = "") const;
    std::string extractFileName(const QString& url) const;
    Note parseJsonNote(const std::string& jsonStr, const QString& fileName) const;
    std::string noteToJson(const Note& note) const;
};

} // namespace nv
