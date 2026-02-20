#include "nv/storage.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>

namespace nv {

LocalStorage::LocalStorage(const QString& directory)
    : directory_(directory) {
}

QString LocalStorage::notePath(const NoteUUID& uuid) const {
    return QDir(directory_).filePath(QString::fromStdString(uuid + ".txt"));
}

std::string LocalStorage::readFile(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("Failed to open file: " + path.toStdString());
    }
    
    QByteArray data = file.readAll();
    return std::string(data.constData(), data.size());
}

void LocalStorage::writeFile(const QString& path, const std::string& content) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        throw std::runtime_error("Failed to open file for writing: " + path.toStdString());
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QString::fromStdString(content);
}

Result<std::vector<std::shared_ptr<Note>>> LocalStorage::readAllNotes() {
    std::vector<std::shared_ptr<Note>> notes;
    
    QDir dir(directory_);
    QFileInfoList files = dir.entryInfoList({"*.txt"}, QDir::Files);
    
    for (const auto& fileInfo : files) {
        try {
            QString path = fileInfo.absoluteFilePath();
            std::string content = readFile(path);
            
            // Parse note: first line is title, rest is body
            size_t newlinePos = content.find('\n');
            std::string title = (newlinePos == std::string::npos) ? content : content.substr(0, newlinePos);
            std::string body = (newlinePos == std::string::npos) ? "" : content.substr(newlinePos + 1);
            
            // Extract UUID from filename
            QString fileName = fileInfo.fileName();
            QString uuidStr = fileName.left(fileName.length() - 4); // Remove .txt
            
            // Use actual file modification time for the note's timestamps
            // QFileInfo::lastModified() returns the modification time as a QDateTime
            auto fileMtime = fileInfo.lastModified();
            NoteTimestamp fileTime = std::chrono::system_clock::from_time_t(fileMtime.toSecsSinceEpoch());
            
            // Detect if this is a checkbox note by checking for checkbox patterns
            NoteType noteType = NoteType::TEXT;
            if (body.find("[x]") != std::string::npos || body.find("[ ]") != std::string::npos) {
                noteType = NoteType::CHECKLIST;
            }
            
            auto note = std::make_shared<Note>(
                uuidStr.toStdString(),
                title,
                body,
                fileTime,  // created = file creation/modification time
                fileTime,  // modified = file modification time
                noteType,  // noteType = detected type
                "PENDING", // syncStatus
                0,         // createdAtMillis
                0,         // updatedAtMillis
                ""         // deviceId
            );
            
            notes.push_back(note);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to load note from " << fileInfo.absoluteFilePath().toStdString() << ": " << e.what() << std::endl;
            continue;
        }
    }
    
    return Result<std::vector<std::shared_ptr<Note>>>{notes};
}

VoidResult LocalStorage::writeNote(const Note& note) {
    try {
        QString path = notePath(note.uuid());
        
        std::string content = note.title() + "\n" + note.body();
        writeFile(path, content);
        
        return VoidResult{SuccessType{}};
    } catch (const std::exception& e) {
        std::cerr << "Error writing note " << note.uuid() << ": " << e.what() << std::endl;
        return VoidResult{StorageError::WriteFailed};
    }
}

VoidResult LocalStorage::deleteNote(const NoteUUID& uuid) {
    try {
        QString path = notePath(uuid);
        QFile file(path);
        
        if (!file.exists()) {
            return VoidResult{SuccessType{}};
        }
        
        if (!file.remove()) {
            return VoidResult{StorageError::WriteFailed};
        }
        
        return VoidResult{SuccessType{}};
    } catch (const std::exception& e) {
        std::cerr << "Error deleting note " << uuid << ": " << e.what() << std::endl;
        return VoidResult{StorageError::WriteFailed};
    }
}

// WebDAVStorage implementation

WebDAVStorage::WebDAVStorage(const QString& serverAddress, const QString& username, const QString& password)
    : serverAddress_(serverAddress)
    , username_(username)
    , password_(password)
    , manager_(new QNetworkAccessManager()) {
}

WebDAVStorage::~WebDAVStorage() {
    if (manager_) {
        manager_->deleteLater();
    }
}

QString WebDAVStorage::buildUrl(const QString& fileName) const {
    QString url = serverAddress_;
    if (!url.endsWith("/")) {
        url += "/";
    }
    return url + fileName;
}

std::string WebDAVStorage::extractFileName(const QString& url) const {
    QUrl qurl(url);
    QString path = qurl.path();
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash >= 0) {
        return path.mid(lastSlash + 1).toStdString();
    }
    return path.toStdString();
}

std::string WebDAVStorage::sendRequest(const QString& url, const QString& method, const std::string& body) const {
    QNetworkRequest request{QUrl(url)};
    
    // Set Content-Type based on method
    if (method == "PROPFIND" || method == "REPORT") {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml");
        request.setRawHeader("Depth", "1");
    } else {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    }
    
    // Add basic auth header
    QString authString = username_ + ":" + password_;
    QByteArray authBytes = authString.toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + authBytes);
    
    QEventLoop loop;
    QNetworkReply* reply = nullptr;
    
    if (method == "GET") {
        reply = manager_->get(request);
    } else if (method == "PUT") {
        reply = manager_->put(request, QByteArray(body.c_str(), body.size()));
    } else if (method == "PROPFIND") {
        reply = manager_->sendCustomRequest(request, "PROPFIND", QByteArray(body.c_str(), body.size()));
    } else if (method == "DELETE") {
        reply = manager_->deleteResource(request);
    }
    
    if (!reply) {
        return "";
    }
    
    // Create timer for timeout
    QTimer* timer = new QTimer();
    timer->setSingleShot(true);
    timer->start(10000);  // 10 second timeout
    
    bool requestFinished = false;
    QString errorMessage;
    int statusCode = -1;
    
    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        timer->stop();
        requestFinished = true;
        loop.quit();
    });
    
    QObject::connect(timer, &QTimer::timeout, [&]() {
        reply->abort();
        errorMessage = "Request timeout";
        requestFinished = true;
        loop.quit();
    });
    
    loop.exec();
    
    if (reply->error() != QNetworkReply::NoError) {
        delete reply;
        timer->deleteLater();
        return "";
    }
    
    statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();
    
    delete reply;
    timer->deleteLater();
    
    // 200-299 are success codes
    // Note: 204 No Content is a success but has no response body, we still return empty string
    // The caller needs to check the status code to determine success, not just if response is empty
    if (statusCode >= 200 && statusCode < 300) {
        return responseData.toStdString();
    }
    
    return "";
}

bool WebDAVStorage::testConnection() const {
    QString url = serverAddress_;
    if (!url.endsWith("/")) {
        url += "/";
    }
    url += "test.json";  // Test with a dummy file
    
    std::string response = sendRequest(url, "GET");
    return !response.empty();
}

QString WebDAVStorage::lastError() const {
    // Store error state for last operation
    return QString();
}

Result<std::vector<std::shared_ptr<Note>>> WebDAVStorage::readAllNotes() {
    std::vector<std::shared_ptr<Note>> notes;
    
    QString url = serverAddress_;
    if (!url.endsWith("/")) {
        url += "/";
    }
    
    // Send PROPFIND request to list files
    std::string response = sendRequest(url, "PROPFIND", "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<propfind xmlns=\"DAV:\"><prop><getlastmodified/><getcontentlength/></prop></propfind>");
    
    if (response.empty()) {
        return Result<std::vector<std::shared_ptr<Note>>>{StorageError::ReadFailed};
    }
    
    // Parse XML response to extract file URLs
    // The response format is:
    // <D:response>
    //   <D:href>/notes/file.json</D:href>
    //   <D:propstat>
    //     <D:prop>
    //       <D:getlastmodified>...</D:getlastmodified>
    //     </D:prop>
    //     <D:status>HTTP/1.1 200 OK</D:status>
    //   </D:propstat>
    // </D:response>
    
    std::string openingResponse = "<D:response>";
    std::string closingResponse = "</D:response>";
    
    size_t pos = 0;
    while (true) {
        size_t start = response.find(openingResponse, pos);
        if (start == std::string::npos) {
            break;
        }
        start += openingResponse.length();
        size_t end = response.find(closingResponse, start);
        if (end == std::string::npos) {
            break;
        }
        
        std::string responseBlock = response.substr(start, end - start);
        
        // Extract href
        std::string openingHref = "<D:href>";
        std::string closingHref = "</D:href>";
        size_t hrefStart = responseBlock.find(openingHref);
        if (hrefStart == std::string::npos) {
            pos = end + closingResponse.length();
            continue;
        }
        hrefStart += openingHref.length();
        size_t hrefEnd = responseBlock.find(closingHref, hrefStart);
        if (hrefEnd == std::string::npos) {
            pos = end + closingResponse.length();
            continue;
        }
        std::string href = responseBlock.substr(hrefStart, hrefEnd - hrefStart);
        
        // Extract status
        std::string openingStatus = "<D:status>";
        std::string closingStatus = "</D:status>";
        size_t statusStart = responseBlock.find(openingStatus);
        if (statusStart == std::string::npos) {
            pos = end + closingResponse.length();
            continue;
        }
        statusStart += openingStatus.length();
        size_t statusEnd = responseBlock.find(closingStatus, statusStart);
        if (statusEnd == std::string::npos) {
            pos = end + closingResponse.length();
            continue;
        }
        std::string status = responseBlock.substr(statusStart, statusEnd - statusStart);
        
        // Check if status indicates success
        if (status.find("200 OK") == std::string::npos) {
            pos = end + closingResponse.length();
            continue;
        }
        
        // Extract filename from href
        QUrl qurl(QString::fromStdString(href));
        QString path = qurl.path();
        int lastSlash = path.lastIndexOf('/');
        QString fileName = (lastSlash >= 0) ? path.mid(lastSlash + 1) : path;
        
        // Only process .json files (notes)
        if (!fileName.endsWith(".json")) {
            pos = end + closingResponse.length();
            continue;
        }
        
        // Extract UUID from filename (remove .json extension)
        QString uuid = fileName.left(fileName.length() - 5);
        
        // Download the note file from WebDAV
        QString fileUrl = url + uuid + ".json";
        std::string fileResponse = sendRequest(fileUrl, "GET");
        
        if (!fileResponse.empty()) {
            try {
                auto note = parseJsonNote(fileResponse, fileName);
                notes.push_back(std::make_shared<Note>(note));
            } catch (const std::exception& e) {
                std::cerr << "Warning: Failed to parse note from " << fileName.toStdString() << ": " << e.what() << std::endl;
            }
        }
        
        pos = end + closingResponse.length();
    }
    
    return Result<std::vector<std::shared_ptr<Note>>>{notes};
}

VoidResult WebDAVStorage::writeNote(const Note& note) {
    QString fileName = QString::fromStdString(note.uuid() + ".json");
    QString url = buildUrl(fileName);
    
    std::string json = noteToJson(note);
    
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QString authString = username_ + ":" + password_;
    QByteArray authBytes = authString.toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + authBytes);
    
    QEventLoop loop;
    QNetworkReply* reply = manager_->put(request, QByteArray(json.c_str(), json.size()));
    
    if (!reply) {
        return VoidResult{StorageError::WriteFailed};
    }
    
    QTimer* timer = new QTimer();
    timer->setSingleShot(true);
    timer->start(10000);
    
    bool requestFinished = false;
    
    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        timer->stop();
        requestFinished = true;
        loop.quit();
    });
    
    QObject::connect(timer, &QTimer::timeout, [&]() {
        reply->abort();
        requestFinished = true;
        loop.quit();
    });
    
    loop.exec();
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    delete reply;
    timer->deleteLater();
    
    // PUT returns 204 No Content on success (no body)
    if (statusCode == 201 || statusCode == 204) {
        return VoidResult{SuccessType{}};
    }
    
    return VoidResult{StorageError::WriteFailed};
}

VoidResult WebDAVStorage::deleteNote(const NoteUUID& uuid) {
    QString fileName = QString::fromStdString(uuid + ".json");
    QString url = buildUrl(fileName);
    
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QString authString = username_ + ":" + password_;
    QByteArray authBytes = authString.toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + authBytes);
    
    QEventLoop loop;
    QNetworkReply* reply = manager_->deleteResource(request);
    
    if (!reply) {
        return VoidResult{StorageError::WriteFailed};
    }
    
    QTimer* timer = new QTimer();
    timer->setSingleShot(true);
    timer->start(10000);
    
    bool requestFinished = false;
    
    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        timer->stop();
        requestFinished = true;
        loop.quit();
    });
    
    QObject::connect(timer, &QTimer::timeout, [&]() {
        reply->abort();
        requestFinished = true;
        loop.quit();
    });
    
    loop.exec();
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    delete reply;
    timer->deleteLater();
    
    // DELETE returns 204 No Content on success
    if (statusCode == 204) {
        return VoidResult{SuccessType{}};
    }
    
    return VoidResult{StorageError::WriteFailed};
}

std::string WebDAVStorage::noteToJson(const Note& note) const {
    QJsonObject jsonObj;
    jsonObj["content"] = QString::fromStdString(note.body());
    
    // Convert time_point to milliseconds since epoch
    auto createdAtEpoch = std::chrono::time_point_cast<std::chrono::milliseconds>(note.created());
    auto updatedAtEpoch = std::chrono::time_point_cast<std::chrono::milliseconds>(note.modified());
    
    jsonObj["createdAt"] = static_cast<qint64>(createdAtEpoch.time_since_epoch().count());
    jsonObj["deviceId"] = QString::fromStdString(note.deviceId());
    jsonObj["id"] = QString::fromStdString(note.uuid());
    
    // Note type: TEXT or CHECKLIST
    jsonObj["noteType"] = note.noteType() == NoteType::TEXT ? "TEXT" : "CHECKLIST";
    
    jsonObj["syncStatus"] = note.syncStatus().empty() ? "PENDING" : QString::fromStdString(note.syncStatus());
    jsonObj["title"] = QString::fromStdString(note.title());
    jsonObj["updatedAt"] = static_cast<qint64>(updatedAtEpoch.time_since_epoch().count());
    
    QJsonDocument doc(jsonObj);
    return doc.toJson(QJsonDocument::Compact).toStdString();
}

Note WebDAVStorage::parseJsonNote(const std::string& jsonStr, const QString& fileName) const {
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonStr.c_str(), jsonStr.size()));
    QJsonObject jsonObj = doc.object();
    
    std::string uuid = fileName.left(fileName.length() - 5).toStdString();  // Remove .json extension
    
    std::string title = jsonObj["title"].toString().toStdString();
    std::string content = jsonObj["content"].toString().toStdString();
    
    // Convert timestamps from milliseconds to system_clock::time_point
    // Note: Qt JSON parser treats large integers as doubles, so we use toDouble() first
    double createdAtMillisDouble = jsonObj["createdAt"].toDouble(0);
    double updatedAtMillisDouble = jsonObj["updatedAt"].toDouble(0);
    int64_t createdAtMillis = static_cast<int64_t>(createdAtMillisDouble);
    int64_t updatedAtMillis = static_cast<int64_t>(updatedAtMillisDouble);
    
    auto createdAt = std::chrono::system_clock::from_time_t(createdAtMillis / 1000);
    auto modifiedAt = std::chrono::system_clock::from_time_t(updatedAtMillis / 1000);
    
    std::string deviceId = jsonObj["deviceId"].toString().toStdString();
    std::string syncStatus = jsonObj["syncStatus"].toString().toStdString();
    
    // Parse note type
    QString noteTypeStr = jsonObj["noteType"].toString();
    NoteType noteType = (noteTypeStr == "CHECKLIST") ? NoteType::CHECKLIST : NoteType::TEXT;
    
    return Note(uuid, title, content, createdAt, modifiedAt, 
                noteType, syncStatus, createdAtMillis, updatedAtMillis, deviceId);
}

} // namespace nv
