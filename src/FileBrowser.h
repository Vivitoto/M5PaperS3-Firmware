#pragma once
#include <Arduino.h>
#include <SD.h>
#include <vector>

// 文件或目录项
struct FileItem {
    char name[64];
    char path[128];
    uint32_t size;
    bool isDirectory;
};

class FileBrowser {
public:
    FileBrowser();
    
    // 扫描当前目录
    bool scan(const char* dirPath);
    
    // 进入子目录 / 返回上级
    bool enterDirectory(int index);
    bool goUp();
    
    // 当前路径
    const char* getCurrentPath() const { return _currentPath; }
    bool isAtRoot() const;
    
    // 获取列表
    const std::vector<FileItem>& getItems() const { return _items; }
    
    // 选择项
    void selectNext();
    void selectPrev();
    int getSelectedIndex() const { return _selectedIndex; }
    const FileItem* getSelectedItem() const;
    
    // 渲染
    void render();
    void renderAt(int startY);
    
    // 清空
    void clear();
    
private:
    char _currentPath[128];
    std::vector<FileItem> _items;
    int _selectedIndex;
    int _scrollOffset;
    
    static bool compareItems(const FileItem& a, const FileItem& b);
};
