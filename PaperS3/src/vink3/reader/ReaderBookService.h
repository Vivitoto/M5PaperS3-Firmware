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
    void renderCurrent();
    void renderTocPage(uint16_t page = 0);
    bool nextPage();
    bool prevPage();
    bool nextTocPage();
    bool prevTocPage();
    bool handleTap(int16_t x, int16_t y);
    bool openTocEntry(int index);

private:
    static constexpr int kMaxTocEntries = 1200;
    static constexpr int kTocEntriesPerPage = 13;
    static constexpr int16_t kTocFirstRowY = 128;
    static constexpr int16_t kTocRowH = 48;
    static constexpr int kMaxChapterPages = 512;

    bool ensureTocBuffer();
    bool ensureSdReady();
    bool isTxtPath(const char* name) const;
    void closeCurrent();
    void setTitleFromPath(const char* path);
    void getTocCachePath(char* out, size_t len) const;
    bool loadTocCache();
    void saveTocCache();
    bool buildChapterPages(int index);
    bool renderCurrentReadingPage();
    bool renderChapterPreview(int index);
    uint32_t chapterContentStart(int index);
    uint32_t chapterEndOffset(int index);
    size_t trimUtf8Tail(char* text, size_t len) const;

    bool sdReady_ = false;
    bool open_ = false;
    char bookPath_[160] = {0};
    char activeTextPath_[160] = {0};
    char title_[72] = {0};
    ChapterDetectResult* toc_ = nullptr;
    uint32_t* pageStarts_ = nullptr;
    int tocCount_ = 0;
    uint16_t tocPage_ = 0;
    int currentTocIndex_ = -1;
    int pageCount_ = 0;
    int currentPage_ = 0;
    bool showingToc_ = true;
};

extern ReaderBookService g_readerBook;

} // namespace vink3
