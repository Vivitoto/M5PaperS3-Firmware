#pragma once
#include <Arduino.h>
#include <SD.h>
#include <M5Unified.h>
#include "FontManager.h"
#include "Config.h"
#include "EpubParser.h"

// 书签信息
struct Bookmark {
    int index;              // 书签序号
    uint32_t charOffset;    // 字符偏移位置
    uint16_t pageNum;       // 页码
    String name;            // 书签名称（可选）
    unsigned long timestamp; // 创建时间
};

// 章节信息
struct ChapterInfo {
    int index;              // 章节序号
    uint32_t charOffset;    // 字符偏移位置
    String title;           // 章节标题
    int confidence;         // 识别置信度 (0-100)
    int pageNum;            // 所在页码（分页后填充）
};

// 页面信息
struct PageInfo {
    uint32_t startOffset;  // 页起始在文件中的偏移
    uint32_t endOffset;    // 页结束偏移
    bool startsParagraph;  // 页面第一行是否为段首（避免跨页误缩进）
};

class EbookReader {
public:
    EbookReader(FontManager& font);
    ~EbookReader();

    // 打开/关闭书籍
    bool openBook(const char* path);
    void closeBook();
    bool isOpen() const;

    // 翻页
    bool nextPage();  // 返回 false 表示已在最后一页
    void prevPage();
    void gotoPage(uint16_t pageNum);
    uint16_t getCurrentPage() const { return _currentPage; }
    uint16_t getTotalPages() const { return _totalPages; }
    bool isLastPage() const { return _currentPage >= _totalPages - 1; }

    // 渲染当前页到屏幕
    void renderPage();

    // 进度管理
    void saveProgress();
    bool loadProgress();

    // 排版配置持久化
    void saveLayoutConfig();
    bool loadLayoutConfig();

    // 排版调整
    void setLayoutConfig(const LayoutConfig& config);
    LayoutConfig getLayoutConfig() const { return _layout; }
    void changeFontSize(int delta);
    void setFontSize(uint8_t size);
    uint8_t getFontSize() const { return _layout.fontSize; }

    // 跳转到指定字符偏移
    void gotoCharOffset(uint32_t charOffset);

    // 当前书名
    const char* getBookPath() const { return _bookPath; }

    // 设置刷新策略
    void setRefreshStrategy(const RefreshStrategy& strategy) { _refreshStrategy = strategy; _needsFullRefresh = true; }
    void setRefreshFrequency(RefreshFrequency freq) { setRefreshStrategy(RefreshStrategy::FromFrequency(freq)); }
    RefreshStrategy getRefreshStrategy() const { return _refreshStrategy; }

    // 书签管理
    bool addBookmark(const char* name = nullptr);          // 添加当前位置的书签
    bool deleteBookmark(int index);                          // 删除指定书签
    void clearBookmarks();                                   // 清空书签
    int getBookmarkCount() const { return _bookmarkCount; }
    const Bookmark* getBookmark(int index) const;
    const Bookmark* getBookmarks() const { return _bookmarks; }
    void gotoBookmark(int index);                            // 跳转到书签位置

    // 章节管理
    bool detectChapters();                          // 自动识别章节
    bool loadChaptersFromCache();                   // 从缓存加载
    void saveChaptersToCache();                     // 保存到缓存
    int getChapterCount() const { return _isEpub ? _epub.getChapterCount() : _chapterCount; }
    bool hasChapters() const;                       // 是否有章节数据
    ChapterInfo* getChapter(int index);             // 获取指定章节
    void gotoChapter(int chapterIndex);             // 跳转到指定章节
    int getCurrentChapterIndex() const;             // 获取当前所在章节
    const ChapterInfo* getChapterList() const { return _chapters; }

    // 刷新频率
    RefreshFrequency getRefreshFrequency() const;   // 获取当前刷新频率

private:
    FontManager& _font;
    File _file;
    char _bookPath[128];
    char _bookName[64];
    String _tempFilePath;   // GBK 转换后的临时文件路径
    bool _isEpub;
    EpubParser _epub;
    int _epubChapter;
    char _epubTempPath[128];

    // 排版配置
    LayoutConfig _layout;

    // 分页
    PageInfo* _pages;       // 分页表（PSRAM）
    uint16_t _currentPage;
    uint16_t _totalPages;
    uint32_t _fileSize;

    // 章节
    ChapterInfo* _chapters;     // 章节列表（PSRAM）
    int _chapterCount;
    int _chapterCapacity;
    bool _chaptersDetected;

    // 书签
    Bookmark* _bookmarks;       // 书签列表（PSRAM）
    int _bookmarkCount;
    int _bookmarkCapacity;

    // 预读缓冲区
    char* _preloadBuffer;       // 下一页文本预读缓冲区（PSRAM）
    uint16_t _preloadSize;      // 预读数据大小
    uint16_t _preloadPage;      // 已预读的页码
    bool _hasPreload;           // 是否有有效预读数据

    // 刷新策略
    RefreshStrategy _refreshStrategy;
    uint8_t _pageTurnCount;     // 翻页计数器
    bool _needsFullRefresh;     // 当前是否需要全刷
    M5Canvas* _pageCanvas;      // 阅读页离屏画布（PSRAM，统一显示提交）
    LovyanGFX* _drawTarget;     // 当前绘制目标：阅读画布或 M5.Display
    bool _readCursorValid;      // 顺序读游标是否可信，避免逐字 seek
    uint32_t _readCursor;       // 下一次顺序读取偏移

    // 构建分页表
    bool buildPages();
    void clearPages();
    bool loadPagesFromCache();
    void savePagesToCache();
    void getPageCachePath(char* out, size_t len);
    bool loadEpubChapter(int chapterIndex);

    // 章节识别
    void clearChapters();
    void getChapterCachePath(char* out, size_t len);

    // 书签管理
    void getBookmarkPath(char* out, size_t len);
    bool loadBookmarks();
    void saveBookmarks();

    // 预读下一页
    void preloadNextPage();
    void invalidatePreload() { _hasPreload = false; }

    // 渲染辅助
    bool renderPageFast();      // 局刷模式（不清屏）
    bool renderPageFull();      // 全刷模式（清屏）
    void drawTextPage();        // 正常绘制
    void drawTextPageFast();    // 从预读缓冲区绘制
    void drawStatusBar();
    bool ensurePageCanvas();
    bool waitDisplayReady(uint32_t timeoutMs);
    LovyanGFX& drawTarget();
    void invalidateReadCursor() { _readCursorValid = false; }
    bool readCharAt(uint32_t offset, uint32_t maxOffset, uint32_t& unicode, uint32_t& nextOffset);
    uint8_t charAdvanceForLayout(uint32_t unicode);
    static bool isWhitespace(uint32_t unicode);
    static bool isBreakableAfter(uint32_t unicode);
    static bool isForbiddenLineStart(uint32_t unicode);
    static bool isOpeningPairPunctuation(uint32_t unicode);

    // 行内字符结构（用于两端对齐和断行）
    struct LineChar {
        uint32_t unicode;
        uint32_t byteStart;
        uint32_t byteEnd;
        uint8_t width;
    };

    uint16_t sumLineWidth(LineChar* chars, uint8_t count);
    bool layoutNextLine(uint32_t startOffset, uint32_t maxOffset, LineChar* chars, uint8_t maxChars,
                        uint8_t& count, uint16_t& lineWidth, uint32_t& nextOffset,
                        bool paragraphStart, bool& paragraphEnded);

    // 绘制一行字符
    void drawLineChars(LineChar* chars, uint8_t count, uint16_t y,
                       uint16_t lineHeight, uint16_t lineWidth, bool isParagraphStart, bool allowJustify);

    // 字符渲染
    void drawBitmapChar(uint32_t unicode, uint16_t x, uint16_t y);
    void drawGrayChar(uint32_t unicode, uint16_t x, uint16_t y);

    // UTF-8 解码
    uint32_t decodeUTF8(const uint8_t* buf, size_t& pos, size_t len);

    // 文件路径辅助
    void getProgressPath(char* out, size_t len);
    void getLayoutConfigPath(char* out, size_t len);
};
