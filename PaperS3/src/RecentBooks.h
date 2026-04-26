#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#define MAX_RECENT_BOOKS 5
#define RECENT_FILE "/progress/recent.json"

struct RecentBook {
    char path[128];
    char name[64];
    int totalPages;
    int currentPage;
    unsigned long lastReadTime;
};

class RecentBooks {
public:
    RecentBooks();
    
    bool load();
    bool save();
    
    void addBook(const char* path, const char* name, int currentPage, int totalPages);
    void updateProgress(const char* path, int currentPage, int totalPages);
    
    int getCount() const { return _count; }
    const RecentBook* getBook(int index) const;
    
    int getProgressPercent(int index) const;
    
private:
    RecentBook _books[MAX_RECENT_BOOKS];
    int _count;
};
