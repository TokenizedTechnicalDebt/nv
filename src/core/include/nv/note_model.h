#pragma once

#include <string>
#include <chrono>
#include <memory>
#include <optional>

namespace nv {

using NoteUUID = std::string;
using NoteTimestamp = std::chrono::system_clock::time_point;

enum class NoteType {
    TEXT,
    CHECKLIST
};

class Note {
public:
    Note(NoteUUID uuid, std::string title, std::string body,
         NoteTimestamp created, NoteTimestamp modified);
    Note(NoteUUID uuid, std::string title, std::string body,
         NoteTimestamp created, NoteTimestamp modified, 
         NoteType noteType, std::string syncStatus,
         int64_t createdAtMillis, int64_t updatedAtMillis,
         std::string deviceId);
    
    [[nodiscard]] const NoteUUID& uuid() const;
    [[nodiscard]] const std::string& title() const;
    [[nodiscard]] std::string& title();
    [[nodiscard]] const std::string& body() const;
    [[nodiscard]] std::string& body();
    [[nodiscard]] NoteTimestamp created() const;
    [[nodiscard]] NoteTimestamp modified() const;
    void setModified(NoteTimestamp t);
    
    // WebDAV-specific fields
    [[nodiscard]] NoteType noteType() const;
    void setNoteType(NoteType type);
    [[nodiscard]] const std::string& syncStatus() const;
    void setSyncStatus(const std::string& status);
    [[nodiscard]] int64_t createdAtMillis() const;
    void setCreatedAtMillis(int64_t ms);
    [[nodiscard]] int64_t updatedAtMillis() const;
    void setUpdatedAtMillis(int64_t ms);
    [[nodiscard]] const std::string& deviceId() const;
    void setDeviceId(const std::string& id);
    
private:
    NoteUUID uuid_;
    std::string title_;
    std::string body_;
    NoteTimestamp created_;
    NoteTimestamp modified_;
    
    // WebDAV-specific fields
    NoteType noteType_;
    std::string syncStatus_;
    int64_t createdAtMillis_;
    int64_t updatedAtMillis_;
    std::string deviceId_;
};

} // namespace nv
