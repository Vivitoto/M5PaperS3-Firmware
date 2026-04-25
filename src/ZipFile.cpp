#include "ZipFile.h"
#include <string.h>

ZipFile::ZipFile() : _file() {
}

ZipFile::~ZipFile() {
    close();
}

bool ZipFile::open(const char* path) {
    close();
    _file = SD.open(path, FILE_READ);
    return _file != false;
}

void ZipFile::close() {
    if (_file) {
        _file.close();
    }
}

bool ZipFile::findFile(const char* filename, uint32_t& offset, uint32_t& size, uint16_t& compression) {
    if (!_file) return false;
    
    // 简化版：线性扫描 ZIP 中央目录
    // 实际应该解析中央目录，但这里为了节省内存简化处理
    
    _file.seek(0);
    
    while (_file.available() >= 30) {
        uint32_t sig = 0;
        _file.read((uint8_t*)&sig, 4);
        
        if (sig == 0x04034b50) { // Local file header
            LocalFileHeader hdr;
            _file.read((uint8_t*)&hdr.version, 2);
            _file.read((uint8_t*)&hdr.flags, 2);
            _file.read((uint8_t*)&hdr.compression, 2);
            _file.read((uint8_t*)&hdr.modTime, 2);
            _file.read((uint8_t*)&hdr.modDate, 2);
            _file.read((uint8_t*)&hdr.crc32, 4);
            _file.read((uint8_t*)&hdr.compressedSize, 4);
            _file.read((uint8_t*)&hdr.uncompressedSize, 4);
            _file.read((uint8_t*)&hdr.nameLen, 2);
            _file.read((uint8_t*)&hdr.extraLen, 2);
            
            // 读取文件名
            char name[256];
            int nameLen = min((int)hdr.nameLen, 255);
            _file.read((uint8_t*)name, nameLen);
            name[nameLen] = '\0';
            
            // 跳过 extra field
            _file.seek(_file.position() + hdr.extraLen);
            
            // 检查是否匹配
            if (strcmp(name, filename) == 0) {
                offset = _file.position();
                size = hdr.uncompressedSize;
                compression = hdr.compression;
                return true;
            }
            
            // 跳过文件数据
            _file.seek(_file.position() + hdr.compressedSize);
        } else if (sig == 0x02014b50 || sig == 0x06054b50) {
            // Central directory or end of central directory
            break;
        } else {
            // 不是有效的 ZIP 结构，跳过一字节重试
            _file.seek(_file.position() - 3);
        }
    }
    
    return false;
}

bool ZipFile::readFile(const char* filename, char* buffer, size_t bufferSize, size_t& outLen) {
    uint32_t offset, size;
    uint16_t compression;
    
    if (!findFile(filename, offset, size, compression)) {
        return false;
    }
    
    if (compression != 0) {
        // 不支持压缩
        Serial.printf("[Zip] Compression %d not supported\n", compression);
        return false;
    }
    
    if (size >= bufferSize) {
        size = bufferSize - 1;
    }
    
    _file.seek(offset);
    int bytesRead = _file.read((uint8_t*)buffer, size);
    buffer[bytesRead] = '\0';
    outLen = bytesRead;
    
    return bytesRead > 0;
}

bool ZipFile::hasFile(const char* filename) {
    uint32_t offset, size;
    uint16_t compression;
    return findFile(filename, offset, size, compression);
}
