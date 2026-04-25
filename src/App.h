#pragma once
#include <Arduino.h>
#include "Config.h"
#include "FontManager.h"
#include "EbookReader.h"
#include "FileBrowser.h"
#include "WiFiUploader.h"
#include "ReadingStats.h"
#include "BlePageTurner.h"
#include "RecentBooks.h"
#include "LegadoSync.h"
#include "WebDavClient.h"

class App {
public:
    App();
    
    // 初始化
    bool init();
    
    // 主循环
    void run();
    
private:
    AppState _state;
    FontManager _font;
    EbookReader _reader;
    FileBrowser _browser;
    WiFiUploader _uploader;
    ReadingStats _stats;
    BlePageTurner _ble;
    RecentBooks _recent;
    LegadoSync _legado;
    
    // 空闲计时（休眠）
    unsigned long _lastActivityTime;
    bool _sleepPending;
    
    // 触摸状态
    bool _touching;
    int _touchStartX, _touchStartY;
    unsigned long _touchStartTime;
    
    // 菜单状态
    int _menuIndex;           // 主菜单选中项
    int _layoutEditorIndex;   // 排版设置菜单选中项
    int _chapterMenuIndex;    // 章节菜单选中项
    int _chapterMenuScroll;   // 章节菜单滚动偏移
    int _bookmarkMenuIndex;   // 书签菜单选中项
    int _bookmarkMenuScroll;  // 书签菜单滚动偏移
    int _fontMenuIndex;       // 字体菜单选中项
    int _fontMenuScroll;      // 字体菜单滚动偏移
    
    // 页码跳转
    char _gotoPageInput[8];
    int _gotoPageCursor;
    
    // WiFi 配置
    char _wifiSsid[32];
    char _wifiPass[32];
    bool _wifiConfigured;
    
    // Legado 配置
    char _legadoHost[64];
    int _legadoPort;
    char _legadoUser[32];
    char _legadoPass[32];
    bool _legadoConfigured;
    
    // 全局设置
    uint8_t _sleepTimeoutMin;  // 休眠时长（分钟）
    
    // 可用字体列表
    char _fontPaths[10][128];
    char _fontNames[10][64];
    int _fontCount;
    
    // 初始化各模块
    bool initDisplay();
    bool initSD();
    bool initFont();
    void scanFonts();
    
    // 状态处理
    void handleInit();
    void handleFileBrowser();
    void handleReader();
    void handleMenu();
    void handleLayoutMenu();    // 排版设置子菜单
    void handleChapterMenu();   // 章节目录菜单
    void handleRefreshMenu();   // 残影控制菜单
    void handleBookmarkMenu();  // 书签菜单
    void handleFontMenu();      // 字体切换菜单
    void handleGotoPage();      // 页码跳转
    void handleWiFiUpload();    // WiFi传书界面
    void handleReadingStats();  // 阅读统计
    void handleEndOfBook();     // 书末处理
    void handleLegadoSync();    // Legado同步界面
    void handleWiFiConfig();    // WiFi配置
    void syncLegadoProgress();  // 执行同步
    void loadWiFiConfig();      // 加载WiFi配置
    void saveWiFiConfig();      // 保存WiFi配置
    void loadLegadoConfig();    // 加载Legado配置
    void saveLegadoConfig();    // 保存Legado配置
    void drawSleepScreen();     // 休眠屏
    
    // 触摸处理
    void processTouch();
    void onTap(int x, int y);
    void onSwipe(int dx, int dy);
    void onVerticalSwipe(int dy);  // 垂直滑动（菜单选择）
    void onLongPress(int x, int y);
    
    // 菜单执行
    void executeMenuItem(int index);
    
    // 排版参数调整
    void adjustLayoutParameter(int delta);
    int clamp(int val, int minVal, int maxVal);
    
    // 全局设置
    void loadGlobalSettings();
    void saveGlobalSettings();
    
    // 电量显示
    void drawBatteryIcon(int x, int y);
    
    // 休眠管理
    void checkAutoSleep();
    void enterSleep();
    void wakeUp();
    
    // BLE 翻页器
    void checkBleCommands();
    
    // 显示消息
    void showMessage(const char* msg, int durationMs = 2000);
};
