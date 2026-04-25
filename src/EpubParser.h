#pragma once
#include <Arduino.h>
#include "ZipFile.h"

// 简化的 EPUB 解析器
// 支持 EPUB 2/3 的基本文本提取
// 注意：这是简化版，不支持 CSS、图片、复杂排版

#define MAX_EPUB_CHAPTERS 50
#define MAX_EPUB_TITLE_LEN 128

struct EpubChapter {
    char id[32];
    char href[128];
    char title[64];
};

class EpubParser {
public:
    EpubParser();
    ~EpubParser();
    
    // 打开 EPUB 文件
    bool open(const char* path);
    void close();
    bool isOpen() const { return _zip.isOpen(); }
    
    // 获取元数据
    const char* getTitle() const { return _title; }
    const char* getAuthor() const { return _author; }
    
    // 获取章节列表
    int getChapterCount() const { return _chapterCount; }
    const EpubChapter* getChapter(int index) const;
    
    // 读取指定章节内容（纯文本）
    // 会简单过滤 HTML 标签，提取文本
    bool readChapter(int index, char* buffer, size_t bufferSize, size_t& outLen);
    
private:
    ZipFile _zip;
    char _title[MAX_EPUB_TITLE_LEN];
    char _author[MAX_EPUB_TITLE_LEN];
    char _opfPath[128];       // content.opf 路径
    char _basePath[64];       // OEBPS/ 或类似
    
    EpubChapter _chapters[MAX_EPUB_CHAPTERS];
    int _chapterCount;
    
    // 解析 container.xml 找到 content.opf
    bool parseContainer();
    
    // 解析 content.opf
    bool parseOPF();
    
    // 简单 XML 解析辅助
    bool findXmlTag(const char* xml, const char* tag, char* out, size_t outLen);
    bool findXmlAttr(const char* xml, const char* tag, const char* attr, char* out, size_t outLen);
    
    // HTML 到纯文本
    void htmlToText(const char* html, char* out, size_t outLen);
};
