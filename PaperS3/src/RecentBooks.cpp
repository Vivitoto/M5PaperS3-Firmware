#include "RecentBooks.h"
#include "JsonHelper.h"
#include <SD.h>

RecentBooks::RecentBooks() : _count(0) {
}

bool RecentBooks::load() {
    _count = 0;
    
    if (!SD.exists(RECENT_FILE)) {
        return false;
    }
    
    File file = SD.open(RECENT_FILE, FILE_READ);
    if (!file) return false;
    
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    
    if (err) return false;
    
    JsonArray arr = doc["books"].as<JsonArray>();
    if (arr.isNull()) return false;
    
    _count = 0;
    for (JsonObject book : arr) {
        if (_count >= MAX_RECENT_BOOKS) break;
        
        strlcpy(_books[_count].path, book["path"] | "", sizeof(_books[_count].path));
        strlcpy(_books[_count].name, book["name"] | "", sizeof(_books[_count].name));
        _books[_count].currentPage = book["currentPage"] | 0;
        _books[_count].totalPages = book["totalPages"] | 1;
        _books[_count].lastReadTime = book["lastReadTime"] | 0;
        _count++;
    }
    
    return true;
}

bool RecentBooks::save() {
    DynamicJsonDocument doc(2048);
    JsonArray arr = doc["books"].to<JsonArray>();
    
    for (int i = 0; i < _count; i++) {
        JsonObject book = addJsonObject(arr);
        book["path"] = _books[i].path;
        book["name"] = _books[i].name;
        book["currentPage"] = _books[i].currentPage;
        book["totalPages"] = _books[i].totalPages;
        book["lastReadTime"] = _books[i].lastReadTime;
    }
    
    File file = SD.open(RECENT_FILE, FILE_WRITE);
    if (!file) return false;
    
    serializeJson(doc, file);
    file.close();
    return true;
}

void RecentBooks::addBook(const char* path, const char* name, int currentPage, int totalPages) {
    // 查找是否已存在
    int existing = -1;
    for (int i = 0; i < _count; i++) {
        if (strcmp(_books[i].path, path) == 0) {
            existing = i;
            break;
        }
    }
    
    if (existing >= 0) {
        // 移到最前面
        RecentBook temp = _books[existing];
        for (int i = existing; i > 0; i--) {
            _books[i] = _books[i - 1];
        }
        _books[0] = temp;
        _books[0].currentPage = currentPage;
        _books[0].totalPages = totalPages;
        _books[0].lastReadTime = millis();
    } else {
        // 腾出位置
        if (_count < MAX_RECENT_BOOKS) {
            _count++;
        }
        for (int i = _count - 1; i > 0; i--) {
            _books[i] = _books[i - 1];
        }
        strlcpy(_books[0].path, path, sizeof(_books[0].path));
        strlcpy(_books[0].name, name, sizeof(_books[0].name));
        _books[0].currentPage = currentPage;
        _books[0].totalPages = totalPages;
        _books[0].lastReadTime = millis();
    }
    
    save();
}

void RecentBooks::updateProgress(const char* path, int currentPage, int totalPages) {
    for (int i = 0; i < _count; i++) {
        if (strcmp(_books[i].path, path) == 0) {
            _books[i].currentPage = currentPage;
            _books[i].totalPages = totalPages;
            _books[i].lastReadTime = millis();
            save();
            return;
        }
    }
}

const RecentBook* RecentBooks::getBook(int index) const {
    if (index < 0 || index >= _count) return nullptr;
    return &_books[index];
}

int RecentBooks::getProgressPercent(int index) const {
    const RecentBook* book = getBook(index);
    if (!book || book->totalPages <= 0) return 0;
    return (book->currentPage * 100) / book->totalPages;
}
