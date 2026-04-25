#include "FileBrowser.h"
#include <M5Unified.h>
#include "Config.h"
#include <string.h>

FileBrowser::FileBrowser() : _selectedIndex(0), _scrollOffset(0) {
    memset(_currentPath, 0, sizeof(_currentPath));
}

bool FileBrowser::scan(const char* dirPath) {
    clear();
    strlcpy(_currentPath, dirPath, sizeof(_currentPath));
    
    File dir = SD.open(dirPath);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("[Browser] Failed to open dir: %s\n", dirPath);
        return false;
    }
    
    // 如果不是根目录，添加 ".." 返回上级
    if (strcmp(dirPath, "/") != 0 && strcmp(dirPath, BOOKS_DIR) != 0) {
        FileItem upItem;
        strlcpy(upItem.name, "📁 ..", sizeof(upItem.name));
        strlcpy(upItem.path, "..", sizeof(upItem.path));
        upItem.size = 0;
        upItem.isDirectory = true;
        _items.push_back(upItem);
    }
    
    File entry = dir.openNextFile();
    while (entry) {
        FileItem item;
        const char* name = entry.name();
        strlcpy(item.name, name, sizeof(item.name));
        
        // 构建完整路径
        if (strcmp(dirPath, "/") == 0) {
            snprintf(item.path, sizeof(item.path), "/%s", name);
        } else {
            snprintf(item.path, sizeof(item.path), "%s/%s", dirPath, name);
        }
        
        item.size = entry.size();
        item.isDirectory = entry.isDirectory();
        
        // 只显示 .txt 文件和目录
        if (item.isDirectory) {
            // 目录名前面加图标
            char displayName[64];
            snprintf(displayName, sizeof(displayName), "📁 %s", item.name);
            strlcpy(item.name, displayName, sizeof(item.name));
            _items.push_back(item);
        } else {
            size_t nameLen = strlen(name);
            if (nameLen > 4 && strcasecmp(name + nameLen - 4, ".txt") == 0) {
                _items.push_back(item);
            } else if (nameLen > 5 && strcasecmp(name + nameLen - 5, ".epub") == 0) {
                _items.push_back(item);
            }
        }
        
        entry.close();
        entry = dir.openNextFile();
    }
    
    dir.close();
    std::sort(_items.begin(), _items.end(), compareItems);
    
    _selectedIndex = 0;
    _scrollOffset = 0;
    
    Serial.printf("[Browser] Found %d items in %s\n", _items.size(), dirPath);
    return true;
}

bool FileBrowser::enterDirectory(int index) {
    if (index < 0 || index >= (int)_items.size()) return false;
    const FileItem& item = _items[index];
    if (!item.isDirectory) return false;
    
    if (strcmp(item.path, "..") == 0) {
        return goUp();
    }
    
    return scan(item.path);
}

bool FileBrowser::goUp() {
    char parent[128];
    strlcpy(parent, _currentPath, sizeof(parent));
    
    // 去掉末尾的 /
    size_t len = strlen(parent);
    if (len > 0 && parent[len-1] == '/') {
        parent[len-1] = '\0';
        len--;
    }
    
    // 找到最后一个 /
    char* lastSlash = strrchr(parent, '/');
    if (lastSlash) {
        if (lastSlash == parent) {
            // 根目录
            parent[1] = '\0';
        } else {
            *lastSlash = '\0';
        }
    }
    
    return scan(parent);
}

bool FileBrowser::isAtRoot() const {
    return strcmp(_currentPath, "/") == 0 || strcmp(_currentPath, BOOKS_DIR) == 0;
}

void FileBrowser::selectNext() {
    if (_items.empty()) return;
    _selectedIndex++;
    if (_selectedIndex >= (int)_items.size()) _selectedIndex = 0;
    
    if (_selectedIndex >= _scrollOffset + 8) _scrollOffset = _selectedIndex - 7;
    if (_selectedIndex < _scrollOffset) _scrollOffset = _selectedIndex;
}

void FileBrowser::selectPrev() {
    if (_items.empty()) return;
    _selectedIndex--;
    if (_selectedIndex < 0) _selectedIndex = _items.size() - 1;
    
    if (_selectedIndex >= _scrollOffset + 8) _scrollOffset = _selectedIndex - 7;
    if (_selectedIndex < _scrollOffset) _scrollOffset = _selectedIndex;
}

const FileItem* FileBrowser::getSelectedItem() const {
    if (_selectedIndex >= 0 && _selectedIndex < (int)_items.size()) {
        return &_items[_selectedIndex];
    }
    return nullptr;
}

void FileBrowser::render() {
    auto& display = M5.Display;
    display.clear();
    
    // 标题 + 路径
    display.setTextSize(2);
    display.setCursor(20, 10);
    display.println("= 书架 =");
    
    display.setTextSize(1);
    display.setCursor(20, 40);
    String path = _currentPath;
    if (path.length() > 50) path = "..." + path.substring(path.length() - 47);
    display.println(path.c_str());
    
    renderAt(55);
    
    // 底部提示
    display.setTextColor(0, 15);
    display.setCursor(20, SCREEN_HEIGHT - 30);
    display.print("滑动:选择  点击右侧:打开/进入  左滑:返回");
    
    display.display();
}

void FileBrowser::renderAt(int startY) {
    auto& display = M5.Display;
    
    display.setTextSize(1);
    int y = startY;
    int itemHeight = 28;
    int maxItems = (SCREEN_HEIGHT - startY - 40) / itemHeight;
    
    for (int i = _scrollOffset; i < (int)_items.size() && i < _scrollOffset + maxItems; i++) {
        const FileItem& item = _items[i];
        
        if (i == _selectedIndex) {
            display.fillRect(10, y - 2, SCREEN_WIDTH - 20, itemHeight, 0);
            display.setTextColor(15, 0);
        } else {
            display.setTextColor(0, 15);
        }
        
        display.setCursor(20, y);
        
        // 截断显示
        char displayName[40];
        strlcpy(displayName, item.name, sizeof(displayName));
        if (strlen(displayName) > 30) {
            displayName[27] = '.';
            displayName[28] = '.';
            displayName[29] = '.';
            displayName[30] = '\0';
        }
        display.print(displayName);
        
        if (!item.isDirectory) {
            char sizeStr[16];
            if (item.size < 1024) snprintf(sizeStr, sizeof(sizeStr), "%dB", item.size);
            else if (item.size < 1024 * 1024) snprintf(sizeStr, sizeof(sizeStr), "%dKB", item.size / 1024);
            else snprintf(sizeStr, sizeof(sizeStr), "%.1fMB", item.size / (1024.0 * 1024.0));
            
            display.setCursor(SCREEN_WIDTH - 120, y);
            display.print(sizeStr);
        }
        
        y += itemHeight;
    }
    
    display.setTextColor(0, 15);
}

void FileBrowser::clear() {
    _items.clear();
    _selectedIndex = 0;
    _scrollOffset = 0;
}

bool FileBrowser::compareItems(const FileItem& a, const FileItem& b) {
    // 目录排在前面
    if (a.isDirectory != b.isDirectory) {
        return a.isDirectory > b.isDirectory;
    }
    return strcasecmp(a.name, b.name) < 0;
}