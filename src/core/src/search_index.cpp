#include "nv/search_index.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <array>
#include <random>
#include <sstream>
#include <iomanip>

namespace nv {

void SearchIndex::indexNoteInternal(std::shared_ptr<Note> note) {
    // This version does NOT lock the mutex - caller must lock if needed
    
    // Remove existing note from index
    auto it = std::find_if(all_notes_.begin(), all_notes_.end(),
        [&note](const std::shared_ptr<Note>& n) { return n->uuid() == note->uuid(); });
    
    if (it != all_notes_.end()) {
        Note* notePtr = it->get();
        
        // Remove from all_notes_
        all_notes_.erase(it);
        
        // Remove from note_to_terms_
        auto noteTermsIt = note_to_terms_.find(notePtr);
        if (noteTermsIt != note_to_terms_.end()) {
            for (const auto& term : noteTermsIt->second) {
                auto& notes = terms_to_notes_[term];
                notes.erase(std::remove(notes.begin(), notes.end(), notePtr), notes.end());
                if (notes.empty()) {
                    terms_to_notes_.erase(term);
                }
            }
            note_to_terms_.erase(noteTermsIt);
        }
    }
    
    all_notes_.push_back(note);
    
    // Tokenize title and body
    std::string text = note->title() + " " + note->body();
    auto tokens = tokenize(text);
    
    note_to_terms_[note.get()] = tokens;
    
    for (const auto& token : tokens) {
        terms_to_notes_[token].push_back(note.get());
    }
}

void SearchIndex::indexNote(std::shared_ptr<Note> note) {
    std::lock_guard<std::mutex> lock(mutex_);
    indexNoteInternal(note);
}

void SearchIndex::removeNoteInternal(const NoteUUID& uuid) {
    // This version does NOT lock the mutex - caller must lock if needed
    
    auto it = std::find_if(all_notes_.begin(), all_notes_.end(),
        [&uuid](const std::shared_ptr<Note>& n) { return n->uuid() == uuid; });
    
    if (it == all_notes_.end()) return;
    
    Note* notePtr = it->get();
    
    // Remove from all_notes_
    all_notes_.erase(it);
    
    // Remove from note_to_terms_
    auto noteTermsIt = note_to_terms_.find(notePtr);
    if (noteTermsIt != note_to_terms_.end()) {
        for (const auto& term : noteTermsIt->second) {
            auto& notes = terms_to_notes_[term];
            notes.erase(std::remove(notes.begin(), notes.end(), notePtr), notes.end());
            if (notes.empty()) {
                terms_to_notes_.erase(term);
            }
        }
        note_to_terms_.erase(noteTermsIt);
    }
}

void SearchIndex::removeNote(const NoteUUID& uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    removeNoteInternal(uuid);
}

void SearchIndex::updateNote(std::shared_ptr<Note> note) {
    // Lock once and do both operations without releasing the lock
    std::lock_guard<std::mutex> lock(mutex_);
    removeNoteInternal(note->uuid());
    indexNoteInternal(note);
}

std::vector<std::shared_ptr<Note>> SearchIndex::filter(const std::string& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (query.empty()) {
        return all_notes_;
    }
    
    auto tokens = tokenize(query);
    if (tokens.empty()) {
        return all_notes_;
    }
    
    // Find notes matching all tokens (AND semantics), allowing prefix partial matches.
    std::set<Note*> matchingNotes;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        std::set<Note*> tokenMatches;

        // Prefix partial matching: query token "proj" matches indexed term "project".
        for (const auto& [indexedTerm, notes] : terms_to_notes_) {
            if (indexedTerm.rfind(token, 0) == 0) {
                tokenMatches.insert(notes.begin(), notes.end());
            }
        }

        if (tokenMatches.empty()) {
            return {};
        }

        if (i == 0) {
            matchingNotes = std::move(tokenMatches);
        } else {
            std::set<Note*> temp;
            std::set_intersection(matchingNotes.begin(), matchingNotes.end(),
                                  tokenMatches.begin(), tokenMatches.end(),
                                  std::inserter(temp, temp.begin()));
            matchingNotes = std::move(temp);
        }
    }
    
    // Build result by finding matching notes in all_notes_
    std::vector<std::shared_ptr<Note>> result;
    for (const auto& note : all_notes_) {
        if (matchingNotes.count(note.get())) {
            result.push_back(note);
        }
    }
    
    return result;
}

void SearchIndex::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    terms_to_notes_.clear();
    note_to_terms_.clear();
    all_notes_.clear();
}

std::vector<std::string> SearchIndex::tokenize(const std::string& s) const {
    return QueryParser::tokenize(s);
}

std::vector<std::string> QueryParser::tokenize(const std::string& s) {
    std::vector<std::string> tokens;
    std::string token;
    
    for (char c : s) {
        if (std::isalnum(c) || std::ispunct(c)) {
            token += std::tolower(c);
        } else {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        }
    }
    
    if (!token.empty()) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string SearchIndex::generateUUID() {
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

} // namespace nv
