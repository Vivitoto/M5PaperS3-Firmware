#pragma once
#include <Arduino.h>
#include <SD.h>
#include "../../ChapterDetector.h"

namespace vink3 {

class ReaderBookService {
public:
    bool begin();
    bool openFirstBook();
    bool openBook(const char* path);
    bool isOpen() const { return open_; }
    int tocCount() const { return tocCount_; }
    const char* title() const { return title_; }
    const char* path() const { return bookPath_; }

    void renderOpenOrHelp();
    void renderTocPage(uint16_t page = 0);

private:
    static constexpr int kMaxTocEntries = 1200;
    static constexpr int kTocEntriesPerPage = 13;

    bool ensureTocBuffer();
    bool ensureSdReady();
    bool isTxtPath(const char* name) const;
    void closeCurrent();
    void setTitleFromPath(const char* path);
    void getTocCachePath(char* out, size_t len) const;
    bool loadTocCache();
    void saveTocCache();

    bool sdReady_ = false;
    bool open_ = false;
    char bookPath_[160] = {0};
    char activeTextPath_[160] = {0};
    char title_[72] = {0};
    ChapterDetectResult* toc_ = nullptr;
    int tocCount_ = 0;
};

extern ReaderBookService g_readerBook;

} // namespace vink3
