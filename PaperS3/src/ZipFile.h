#pragma once
#include <Arduino.h>
#include <SD.h>

// 简化的 ZIP 文件读取器（用于 EPUB）
// EPUB 实际上是 ZIP 文件，包含 META-INF/container.xml 和 OEBPS/content.opf
class ZipFile {
public:
    ZipFile();
    ~ZipFile();
    
    // 打开 ZIP 文件
    bool open(const char* path);
    void close();
    bool isOpen() const { return _file; }
    
    // 读取 ZIP 中的文件内容
    // 注意：这是简化版，不支持压缩，只支持存储（STORE）方式
    // 对于大多数 EPUB，文本文件通常是存储方式
    bool readFile(const char* filename, char* buffer, size_t bufferSize, size_t& outLen);
    
    // 检查文件是否存在
    bool hasFile(const char* filename);
    
private:
    File _file;
    
    // ZIP 本地文件头结构
    struct LocalFileHeader {
        uint32_t signature;      // 0x04034b50
        uint16_t version;
        uint16_t flags;
        uint16_t compression;    // 0=STORE, 8=DEFLATE
        uint16_t modTime;
        uint16_t modDate;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint16_t nameLen;
        uint16_t extraLen;
    };
    
    bool findFile(const char* filename, uint32_t& offset, uint32_t& size, uint16_t& compression);
};
