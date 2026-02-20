#include "nv/note_model.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <array>
#include <QHostInfo>

namespace nv {

Note::Note(NoteUUID uuid, std::string title, std::string body,
           NoteTimestamp created, NoteTimestamp modified)
    : uuid_(std::move(uuid))
    , title_(std::move(title))
    , body_(std::move(body))
    , created_(created)
    , modified_(modified)
    , noteType_(NoteType::TEXT)
    , syncStatus_("PENDING")
    , createdAtMillis_(0)
    , updatedAtMillis_(0)
    , deviceId_(QHostInfo::localHostName().toStdString()) {
}

Note::Note(NoteUUID uuid, std::string title, std::string body,
           NoteTimestamp created, NoteTimestamp modified,
           NoteType noteType, std::string syncStatus,
           int64_t createdAtMillis, int64_t updatedAtMillis,
           std::string deviceId)
    : uuid_(std::move(uuid))
    , title_(std::move(title))
    , body_(std::move(body))
    , created_(created)
    , modified_(modified)
    , noteType_(noteType)
    , syncStatus_(std::move(syncStatus))
    , createdAtMillis_(createdAtMillis)
    , updatedAtMillis_(updatedAtMillis)
    , deviceId_(std::move(deviceId)) {
}

const NoteUUID& Note::uuid() const {
    return uuid_;
}

const std::string& Note::title() const {
    return title_;
}

std::string& Note::title() {
    return title_;
}

const std::string& Note::body() const {
    return body_;
}

std::string& Note::body() {
    return body_;
}

NoteTimestamp Note::created() const {
    return created_;
}

NoteTimestamp Note::modified() const {
    return modified_;
}

void Note::setModified(NoteTimestamp t) {
    modified_ = t;
}

// WebDAV-specific getters and setters
NoteType Note::noteType() const {
    return noteType_;
}

void Note::setNoteType(NoteType type) {
    noteType_ = type;
}

const std::string& Note::syncStatus() const {
    return syncStatus_;
}

void Note::setSyncStatus(const std::string& status) {
    syncStatus_ = status;
}

int64_t Note::createdAtMillis() const {
    return createdAtMillis_;
}

void Note::setCreatedAtMillis(int64_t ms) {
    createdAtMillis_ = ms;
}

int64_t Note::updatedAtMillis() const {
    return updatedAtMillis_;
}

void Note::setUpdatedAtMillis(int64_t ms) {
    updatedAtMillis_ = ms;
}

const std::string& Note::deviceId() const {
    return deviceId_;
}

void Note::setDeviceId(const std::string& id) {
    deviceId_ = id;
}

static std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    std::array<uint8_t, 16> bytes;
    for (int i = 0; i < 16; i++) {
        bytes[i] = dis(gen);
    }
    
    // Set version to 4 (bits 12-15 of time_hi_and_version)
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    
    // Set variant to RFC 4122 (bits 6-7 of clock_seq_hi_and_reserved)
    bytes[8] = (bytes[8] & 0x3F) | 0x80;
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    // Format: 8-4-4-4-12
    for (int i = 0; i < 4; i++) ss << std::setw(2) << static_cast<int>(bytes[i]);
    ss << "-";
    for (int i = 4; i < 6; i++) ss << std::setw(2) << static_cast<int>(bytes[i]);
    ss << "-";
    for (int i = 6; i < 8; i++) ss << std::setw(2) << static_cast<int>(bytes[i]);
    ss << "-";
    for (int i = 8; i < 10; i++) ss << std::setw(2) << static_cast<int>(bytes[i]);
    ss << "-";
    for (int i = 10; i < 16; i++) ss << std::setw(2) << static_cast<int>(bytes[i]);
    
    return ss.str();
}

std::shared_ptr<Note> createNote(const std::string& title, const std::string& body) {
    auto now = std::chrono::system_clock::now();
    return std::make_shared<Note>(
        generateUUID(),
        title,
        body,
        now,
        now
    );
}

} // namespace nv
