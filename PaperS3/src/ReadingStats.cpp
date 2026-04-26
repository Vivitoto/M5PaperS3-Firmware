#include "ReadingStats.h"
#include "JsonHelper.h"
#include <ArduinoJson.h>

ReadingStats::ReadingStats() : _bookCount(0), _dailyCount(0), _sessionStartMs(0), _isReading(false) {
    memset(_currentBookName, 0, sizeof(_currentBookName));
    _currentBook = BookStats::Default();
}

void ReadingStats::load() {
    File f = SD.open(STATS_PATH, FILE_READ);
    if (!f) return;
    
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;
    
    // 加载书籍统计
    JsonArray books = doc["books"];
    _bookCount = min((int)books.size(), MAX_BOOKS);
    for (int i = 0; i < _bookCount; i++) {
        strlcpy(_books[i].bookName, books[i]["name"] | "", sizeof(_books[i].bookName));
        _books[i].totalPagesRead = books[i]["pages"] | 0;
        _books[i].totalSeconds = books[i]["seconds"] | 0;
        _books[i].lastReadTime = books[i]["last"] | 0;
        _books[i].readCount = books[i]["opens"] | 0;
    }
    
    // 加载每日统计
    JsonArray daily = doc["daily"];
    _dailyCount = min((int)daily.size(), MAX_DAILY);
    for (int i = 0; i < _dailyCount; i++) {
        strlcpy(_daily[i].date, daily[i]["date"] | "", sizeof(_daily[i].date));
        _daily[i].totalSeconds = daily[i]["seconds"] | 0;
        _daily[i].pagesRead = daily[i]["pages"] | 0;
        _daily[i].booksOpened = daily[i]["books"] | 0;
    }
    
    Serial.printf("[Stats] Loaded %d books, %d daily records\n", _bookCount, _dailyCount);
}

void ReadingStats::save() {
    if (_isReading) {
        stopReading();
    }
    
    File f = SD.open(STATS_PATH, FILE_WRITE);
    if (!f) return;
    
    DynamicJsonDocument doc(2048);
    JsonArray books = doc["books"].to<JsonArray>();
    for (int i = 0; i < _bookCount; i++) {
        JsonObject obj = addJsonObject(books);
        obj["name"] = _books[i].bookName;
        obj["pages"] = _books[i].totalPagesRead;
        obj["seconds"] = _books[i].totalSeconds;
        obj["last"] = _books[i].lastReadTime;
        obj["opens"] = _books[i].readCount;
    }
    
    JsonArray daily = doc["daily"].to<JsonArray>();
    for (int i = 0; i < _dailyCount; i++) {
        JsonObject obj = addJsonObject(daily);
        obj["date"] = _daily[i].date;
        obj["seconds"] = _daily[i].totalSeconds;
        obj["pages"] = _daily[i].pagesRead;
        obj["books"] = _daily[i].booksOpened;
    }
    
    serializeJson(doc, f);
    f.close();
    Serial.println("[Stats] Saved");
}

void ReadingStats::startReading(const char* bookName) {
    if (_isReading) {
        stopReading();
    }
    
    strlcpy(_currentBookName, bookName, sizeof(_currentBookName));
    _sessionStartMs = millis();
    _isReading = true;
    
    BookStats* book = findOrCreateBook(bookName);
    if (book) {
        book->readCount++;
        book->lastReadTime = millis();
        _currentBook = *book;
    }
    
    char today[12];
    getTodayStr(today, sizeof(today));
    DailyStats* day = findOrCreateDaily(today);
    if (day) {
        day->booksOpened = min((uint16_t)(day->booksOpened + 1), (uint16_t)999);
    }
    
    Serial.printf("[Stats] Start reading: %s\n", bookName);
}

void ReadingStats::stopReading() {
    if (!_isReading) return;
    
    unsigned long elapsed = (millis() - _sessionStartMs) / 1000;
    if (elapsed > 3600) elapsed = 3600;  // 单次上限1小时，防止异常
    
    BookStats* book = findOrCreateBook(_currentBookName);
    if (book) {
        book->totalSeconds += elapsed;
    }
    
    char today[12];
    getTodayStr(today, sizeof(today));
    DailyStats* day = findOrCreateDaily(today);
    if (day) {
        day->totalSeconds += elapsed;
    }
    
    _isReading = false;
    Serial.printf("[Stats] Stop reading: %s, elapsed=%ds\n", _currentBookName, elapsed);
}

void ReadingStats::recordPageTurn() {
    if (!_isReading) return;
    
    BookStats* book = findOrCreateBook(_currentBookName);
    if (book) {
        book->totalPagesRead++;
        _currentBook.totalPagesRead = book->totalPagesRead;
    }
    
    char today[12];
    getTodayStr(today, sizeof(today));
    DailyStats* day = findOrCreateDaily(today);
    if (day) {
        day->pagesRead++;
    }
}

const BookStats* ReadingStats::getBookStats(int index) const {
    if (index < 0 || index >= _bookCount) return nullptr;
    return &_books[index];
}

uint32_t ReadingStats::getTotalReadingSeconds() const {
    uint32_t total = 0;
    for (int i = 0; i < _bookCount; i++) {
        total += _books[i].totalSeconds;
    }
    return total;
}

uint32_t ReadingStats::getTotalPagesRead() const {
    uint32_t total = 0;
    for (int i = 0; i < _bookCount; i++) {
        total += _books[i].totalPagesRead;
    }
    return total;
}

uint32_t ReadingStats::getTodaySeconds() const {
    char today[12];
    getTodayStr(today, sizeof(today));
    for (int i = 0; i < _dailyCount; i++) {
        if (strcmp(_daily[i].date, today) == 0) {
            return _daily[i].totalSeconds;
        }
    }
    return 0;
}

uint32_t ReadingStats::getTodayPages() const {
    char today[12];
    getTodayStr(today, sizeof(today));
    for (int i = 0; i < _dailyCount; i++) {
        if (strcmp(_daily[i].date, today) == 0) {
            return _daily[i].pagesRead;
        }
    }
    return 0;
}

String ReadingStats::formatTime(uint32_t seconds) {
    if (seconds < 60) {
        return String(seconds) + "秒";
    } else if (seconds < 3600) {
        return String(seconds / 60) + "分" + String(seconds % 60) + "秒";
    } else {
        return String(seconds / 3600) + "时" + String((seconds % 3600) / 60) + "分";
    }
}

BookStats* ReadingStats::findOrCreateBook(const char* bookName) {
    // 查找
    for (int i = 0; i < _bookCount; i++) {
        if (strcmp(_books[i].bookName, bookName) == 0) {
            return &_books[i];
        }
    }
    // 创建
    if (_bookCount < MAX_BOOKS) {
        int idx = _bookCount++;
        strlcpy(_books[idx].bookName, bookName, sizeof(_books[idx].bookName));
        _books[idx].totalPagesRead = 0;
        _books[idx].totalSeconds = 0;
        _books[idx].lastReadTime = 0;
        _books[idx].readCount = 0;
        return &_books[idx];
    }
    return nullptr;
}

DailyStats* ReadingStats::findOrCreateDaily(const char* date) {
    for (int i = 0; i < _dailyCount; i++) {
        if (strcmp(_daily[i].date, date) == 0) {
            return &_daily[i];
        }
    }
    if (_dailyCount < MAX_DAILY) {
        int idx = _dailyCount++;
        strlcpy(_daily[idx].date, date, sizeof(_daily[idx].date));
        _daily[idx].totalSeconds = 0;
        _daily[idx].pagesRead = 0;
        _daily[idx].booksOpened = 0;
        return &_daily[idx];
    }
    return nullptr;
}

void ReadingStats::getTodayStr(char* out, size_t len) {
    // 使用 millis() 的粗略日期（从上电开始的天数）
    // 实际设备上应该使用 RTC，这里简化处理
    unsigned long days = millis() / 86400000UL;
    snprintf(out, len, "DAY_%lu", days);
}
