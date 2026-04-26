#pragma once
#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>

// 单本书的阅读统计
struct BookStats {
    char bookName[64];
    uint32_t totalPagesRead;     // 累计翻过的页数
    uint32_t totalSeconds;     // 累计阅读秒数
    uint32_t lastReadTime;     // 上次阅读时间戳（millis）
    uint16_t readCount;        // 打开次数
    
    static BookStats Default() {
        return {
            .bookName = {0},
            .totalPagesRead = 0,
            .totalSeconds = 0,
            .lastReadTime = 0,
            .readCount = 0,
        };
    }
};

// 每日全局统计
struct DailyStats {
    char date[12];             // YYYY-MM-DD
    uint32_t totalSeconds;
    uint32_t pagesRead;
    uint16_t booksOpened;
};

class ReadingStats {
public:
    ReadingStats();
    
    // 加载/保存
    void load();
    void save();
    
    // 当前书统计
    void startReading(const char* bookName);
    void stopReading();
    void recordPageTurn();
    
    // 获取当前书统计
    BookStats getCurrentBookStats() const { return _currentBook; }
    
    // 所有书统计
    int getBookCount() const { return _bookCount; }
    const BookStats* getBookStats(int index) const;
    
    // 总统计
    uint32_t getTotalReadingSeconds() const;
    uint32_t getTotalPagesRead() const;
    uint32_t getTodaySeconds() const;
    uint32_t getTodayPages() const;
    
    // 格式化时间
    static String formatTime(uint32_t seconds);
    
private:
    static const int MAX_BOOKS = 50;
    static const int MAX_DAILY = 30;
    
    BookStats _books[MAX_BOOKS];
    int _bookCount;
    
    DailyStats _daily[MAX_DAILY];
    int _dailyCount;
    
    BookStats _currentBook;
    char _currentBookName[64];
    unsigned long _sessionStartMs;
    bool _isReading;
    
    // 查找/创建书籍记录
    BookStats* findOrCreateBook(const char* bookName);
    DailyStats* findOrCreateDaily(const char* date);
    
    // 文件路径
    static constexpr const char* STATS_PATH = "/progress/reading_stats.json";
    
    // 获取今日日期字符串
    static void getTodayStr(char* out, size_t len);
};
