#include "EpubParser.h"
#include <string.h>

EpubParser::EpubParser() : _chapterCount(0) {
    memset(_title, 0, sizeof(_title));
    memset(_author, 0, sizeof(_author));
    memset(_opfPath, 0, sizeof(_opfPath));
    memset(_basePath, 0, sizeof(_basePath));
}

EpubParser::~EpubParser() {
    close();
}

bool EpubParser::open(const char* path) {
    close();
    
    if (!_zip.open(path)) {
        Serial.printf("[EPUB] Failed to open: %s\n", path);
        return false;
    }
    
    if (!parseContainer()) {
        Serial.println("[EPUB] Failed to parse container.xml");
        return false;
    }
    
    if (!parseOPF()) {
        Serial.println("[EPUB] Failed to parse content.opf");
        return false;
    }
    
    Serial.printf("[EPUB] Opened: %s, chapters: %d\n", _title, _chapterCount);
    return true;
}

void EpubParser::close() {
    _zip.close();
    _chapterCount = 0;
    memset(_title, 0, sizeof(_title));
    memset(_author, 0, sizeof(_author));
}

bool EpubParser::parseContainer() {
    char buffer[2048];
    size_t len;
    
    if (!_zip.readFile("META-INF/container.xml", buffer, sizeof(buffer), len)) {
        return false;
    }
    
    // 找到 full-path 属性
    const char* rootfileTag = strstr(buffer, "rootfile");
    if (!rootfileTag) return false;
    
    const char* fullPathAttr = strstr(rootfileTag, "full-path=\"");
    if (!fullPathAttr) return false;
    
    fullPathAttr += 11; // 跳过 full-path="
    
    int i = 0;
    while (fullPathAttr[i] != '"' && i < sizeof(_opfPath) - 1) {
        _opfPath[i] = fullPathAttr[i];
        i++;
    }
    _opfPath[i] = '\0';
    
    // 提取 base path
    char* lastSlash = strrchr(_opfPath, '/');
    if (lastSlash) {
        int baseLen = lastSlash - _opfPath + 1;
        if (baseLen < sizeof(_basePath)) {
            strncpy(_basePath, _opfPath, baseLen);
            _basePath[baseLen] = '\0';
        }
    }
    
    Serial.printf("[EPUB] OPF path: %s\n", _opfPath);
    return true;
}

bool EpubParser::parseOPF() {
    char buffer[8192];
    size_t len;
    
    if (!_zip.readFile(_opfPath, buffer, sizeof(buffer), len)) {
        return false;
    }
    
    // 提取标题
    findXmlTag(buffer, "dc:title", _title, sizeof(_title));
    
    // 提取作者
    findXmlTag(buffer, "dc:creator", _author, sizeof(_author));
    
    // 解析 spine 获取章节顺序
    const char* spinePos = strstr(buffer, "<spine");
    if (!spinePos) spinePos = buffer;
    
    _chapterCount = 0;
    const char* itemrefPos = spinePos;
    
    while ((itemrefPos = strstr(itemrefPos, "itemref")) != nullptr && _chapterCount < MAX_EPUB_CHAPTERS) {
        const char* idrefAttr = strstr(itemrefPos, "idref=\"");
        if (!idrefAttr) break;
        
        idrefAttr += 7;
        char idref[32];
        int i = 0;
        while (idrefAttr[i] != '"' && i < 31) {
            idref[i] = idrefAttr[i];
            i++;
        }
        idref[i] = '\0';
        
        // 在 manifest 中查找对应 href
        char manifestItem[256];
        snprintf(manifestItem, sizeof(manifestItem), "id=\"%s\"", idref);
        
        const char* manifestPos = strstr(buffer, manifestItem);
        if (manifestPos) {
            const char* hrefAttr = strstr(manifestPos, "href=\"");
            if (hrefAttr) {
                hrefAttr += 6;
                char href[128];
                int j = 0;
                while (hrefAttr[j] != '"' && j < 127) {
                    href[j] = hrefAttr[j];
                    j++;
                }
                href[j] = '\0';
                
                strncpy(_chapters[_chapterCount].id, idref, sizeof(_chapters[_chapterCount].id) - 1);
                
                // 构建完整路径
                snprintf(_chapters[_chapterCount].href, sizeof(_chapters[_chapterCount].href), 
                         "%s%s", _basePath, href);
                
                // 尝试从 navPoint 或 toc 获取标题
                snprintf(_chapters[_chapterCount].title, sizeof(_chapters[_chapterCount].title), 
                         "章节 %d", _chapterCount + 1);
                
                _chapterCount++;
            }
        }
        
        itemrefPos += 8; // 跳过当前 itemref
    }
    
    // 尝试从 toc.ncx 获取章节标题
    char tocPath[128];
    snprintf(tocPath, sizeof(tocPath), "%stoc.ncx", _basePath);
    
    char tocBuffer[8192];
    size_t tocLen;
    if (_zip.readFile(tocPath, tocBuffer, sizeof(tocBuffer), tocLen)) {
        // 简单解析 navPoint 标题
        const char* navLabelPos = tocBuffer;
        int chapterIdx = 0;
        while ((navLabelPos = strstr(navLabelPos, "<navLabel>")) != nullptr && chapterIdx < _chapterCount) {
            const char* textTag = strstr(navLabelPos, "<text>");
            if (textTag) {
                textTag += 6;
                int k = 0;
                while (textTag[k] != '<' && k < sizeof(_chapters[chapterIdx].title) - 1) {
                    _chapters[chapterIdx].title[k] = textTag[k];
                    k++;
                }
                _chapters[chapterIdx].title[k] = '\0';
                chapterIdx++;
            }
            navLabelPos += 10;
        }
    }
    
    return _chapterCount > 0;
}

const EpubChapter* EpubParser::getChapter(int index) const {
    if (index < 0 || index >= _chapterCount) return nullptr;
    return &_chapters[index];
}

bool EpubParser::readChapter(int index, char* buffer, size_t bufferSize, size_t& outLen) {
    if (index < 0 || index >= _chapterCount) return false;
    
    char htmlBuffer[16384];
    size_t htmlLen;
    
    if (!_zip.readFile(_chapters[index].href, htmlBuffer, sizeof(htmlBuffer), htmlLen)) {
        return false;
    }
    
    htmlToText(htmlBuffer, buffer, bufferSize);
    outLen = strlen(buffer);
    return true;
}

bool EpubParser::findXmlTag(const char* xml, const char* tag, char* out, size_t outLen) {
    char openTag[64];
    char closeTag[64];
    snprintf(openTag, sizeof(openTag), "<%s>", tag);
    snprintf(closeTag, sizeof(closeTag), "</%s>", tag);
    
    const char* start = strstr(xml, openTag);
    if (!start) {
        // 尝试带命名空间
        start = strstr(xml, tag);
        if (start) {
            start = strchr(start, '>');
            if (start) start++;
        }
    } else {
        start += strlen(openTag);
    }
    
    if (!start) return false;
    
    const char* end = strstr(start, closeTag);
    if (!end) return false;
    
    size_t len = end - start;
    if (len >= outLen) len = outLen - 1;
    strncpy(out, start, len);
    out[len] = '\0';
    
    return true;
}

bool EpubParser::findXmlAttr(const char* xml, const char* tag, const char* attr, char* out, size_t outLen) {
    // 简化实现
    return false;
}

void EpubParser::htmlToText(const char* html, char* out, size_t outLen) {
    size_t outPos = 0;
    bool inTag = false;
    bool inScript = false;
    
    for (size_t i = 0; html[i] != '\0' && outPos < outLen - 1; i++) {
        if (inScript) {
            if (html[i] == '<' && strncasecmp(&html[i], "</script>", 9) == 0) {
                inScript = false;
                i += 8;
            }
            continue;
        }
        
        if (html[i] == '<') {
            inTag = true;
            if (strncasecmp(&html[i + 1], "script", 6) == 0) {
                inScript = true;
            }
            continue;
        }
        
        if (html[i] == '>') {
            inTag = false;
            continue;
        }
        
        if (!inTag) {
            out[outPos++] = html[i];
        }
    }
    
    out[outPos] = '\0';
    
    // 合并连续换行
    size_t writePos = 0;
    int newlineCount = 0;
    for (size_t i = 0; i < outPos; i++) {
        if (out[i] == '\n') {
            newlineCount++;
            if (newlineCount <= 2) {
                out[writePos++] = out[i];
            }
        } else {
            newlineCount = 0;
            out[writePos++] = out[i];
        }
    }
    out[writePos] = '\0';
}
