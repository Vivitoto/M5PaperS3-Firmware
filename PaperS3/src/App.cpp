#include "App.h"
#include "UITheme.h"
#include <M5Unified.h>

using namespace UITheme;

static void prepareShellFrame();
static void commitShellFrame();
static void drawBackHeader(const char* title, const char* hint = "点 < / 右滑 / 上滑返回");

App::App() : _state(AppState::INIT), _reader(_font), _touching(false),
             _menuIndex(0), _layoutEditorIndex(0), _chapterMenuIndex(0), _chapterMenuScroll(0),
             _bookmarkMenuIndex(0), _bookmarkMenuScroll(0),
             _fontMenuIndex(0), _fontMenuScroll(0), _settingsScroll(0),
             _activeTab(0), _libraryPage(0), _libraryTotalPages(1), _librarySelected(0),
             _gotoPageCursor(0),
             _lastActivityTime(0), _sleepPending(false), _powerButtonArmed(false),
             _shutdownInProgress(false), _powerButtonPressStart(0),
             _sleepTimeoutMin(AUTO_SLEEP_DEFAULT_MIN),
             _fontCount(0) {
    memset(_gotoPageInput, 0, sizeof(_gotoPageInput));
    memset(_wifiSsid, 0, sizeof(_wifiSsid));
    memset(_wifiPass, 0, sizeof(_wifiPass));
    memset(_legadoHost, 0, sizeof(_legadoHost));
    memset(_legadoUser, 0, sizeof(_legadoUser));
    memset(_legadoPass, 0, sizeof(_legadoPass));
    _wifiConfigured = false;
    _legadoConfigured = false;
    _legadoPort = 80;
    for (int i = 0; i < 4; i++) _tabNeedsRender[i] = true;
}

bool App::init() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[Vink-PaperS3] Starting...");
    Serial.printf("[Boot] PSRAM size=%u free=%u\n", ESP.getPsramSize(), ESP.getFreePsram());
    
    auto cfg = M5.config();
    // Follow the proven ReadPaper/PaperS3 startup path: let M5Unified/M5GFX
    // autodetect the built-in PaperS3 e-paper panel. Forcing the external
    // module display path can leave M5.Display uninitialized on PaperS3.
    cfg.clear_display = false;
    M5.begin(cfg);
    delay(50);
    
    if (!initDisplay()) {
        Serial.println("[App] Display init failed");
        return false;
    }
    
    bool sdReady = initSD();
    if (!sdReady) {
        showMessage("SD卡异常，继续启动", 3000);
        Serial.println("[App] SD init failed, continuing for boot diagnostics");
    }
    
    if (!initFont()) {
        showMessage("字体异常，使用备用字体", 2000);
        Serial.println("[App] Font init failed, continuing with built-in fallback if available");
    }
    
    if (sdReady) {
        if (!SD.exists(BOOKS_DIR)) SD.mkdir(BOOKS_DIR);
        if (!SD.exists(PROGRESS_DIR)) SD.mkdir(PROGRESS_DIR);
    }
    
    // 扫描可用字体
    if (sdReady) scanFonts();
    
    if (sdReady) _browser.scan(BOOKS_DIR);
    _state = AppState::TAB_READING;
    _activeTab = 0;
    for (int i = 0; i < 4; i++) _tabNeedsRender[i] = true;
    
    loadGlobalSettings();
    _lastActivityTime = millis();
    _sleepPending = false;
    _powerButtonArmed = false;
    
    _stats.load();
    _recent.load();
    loadWiFiConfig();
    loadLegadoConfig();
    
    Serial.println("[App] Init complete");
    return true;
}

bool App::initDisplay() {
    auto& display = M5.Display;
    display.powerSaveOff();
    display.setEpdMode(epd_mode_t::epd_fastest);
    display.setColorDepth(4); // ReadPaper TEXT_COLORDEPTH
    display.setRotation(2);   // ReadPaper default portrait rotation
    delay(20);

    if (display.width() == 0 || display.height() == 0) {
        Serial.println("[Display] Display not detected");
        return false;
    }
    Serial.printf("[Display] %dx%d\n", display.width(), display.height());

    // ReadPaper-style draw path: render into a 4bpp M5Canvas and pushSprite,
    // instead of drawing directly to M5.Display then calling display().
    M5Canvas canvas(&display);
    canvas.setColorDepth(4);
    canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas.fillSprite(15);
    canvas.setTextColor(0, 15);
    canvas.setTextSize(2);
    canvas.setCursor(24, 32);
    canvas.println("Vink-PaperS3");
    canvas.setTextSize(1);
    canvas.setCursor(24, 76);
    canvas.println("v1.2.8 中文界面测试");
    canvas.setCursor(24, 104);
    canvas.printf("Display %dx%d", display.width(), display.height());
    canvas.setCursor(24, 132);
    canvas.printf("PSRAM %u KB", ESP.getPsramSize() / 1024);
    canvas.drawRect(24, 180, 180, 80, 0);
    canvas.fillRect(230, 180, 180, 80, 0);
    display.waitDisplay();
    canvas.pushSprite(0, 0);
    display.waitDisplay();
    delay(4000);
    return true;
}

bool App::initSD() {
    Serial.println("[SD] Initializing PaperS3 SD SPI bus...");
    // PaperS3 matches ReadPaper's verified pinout:
    // CS=47, SCK=39, MOSI=38, MISO=40. The old CS=4/SCK=14 path is for
    // other ESP32 boards and can make startup look dead after flashing.
    constexpr int SD_CS = 47;
    constexpr int SD_SCK = 39;
    constexpr int SD_MOSI = 38;
    constexpr int SD_MISO = 40;
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    const uint32_t freqs[] = {25000000u, 8000000u, 4000000u};
    for (uint32_t freq : freqs) {
        Serial.printf("[SD] Trying SPI freq=%u...\n", freq);
        if (SD.begin(SD_CS, SPI, freq)) {
            Serial.println("[SD] OK");
            return true;
        }
        delay(50);
    }
    Serial.println("[SD] PaperS3 SPI config failed");
    return false;
}

bool App::initFont() {
    if (_font.loadFont(FONT_FILE_24)) {
        if (strncmp(_font.getCurrentFontPath(), "builtin://", 10) == 0) {
            showMessage("使用内置字体", 1000);
        } else {
            Serial.printf("[Font] Loaded: %s\n", _font.getCurrentFontPath());
        }
        return true;
    }
    if (_font.loadFont(FONT_FILE_16)) {
        if (strncmp(_font.getCurrentFontPath(), "builtin://", 10) == 0) {
            showMessage("使用内置字体", 1000);
        } else {
            Serial.printf("[Font] Loaded: %s\n", _font.getCurrentFontPath());
        }
        return true;
    }
    Serial.println("[Font] No font available!");
    return false;
}

void App::scanFonts() {
    _fontCount = FontManager::scanFonts(_fontPaths, _fontNames, 10);
    Serial.printf("[Font] Scanned %d fonts\n", _fontCount);
}

void App::run() {
    while (true) {
        M5.update();
        handlePowerButton();
        processTouch();
        checkAutoSleep();
        checkBleCommands();
        _uploader.handleClient();
        
        switch (_state) {
            case AppState::INIT:
                handleInit();
                break;
            case AppState::TAB_READING:
                handleTabReading();
                break;
            case AppState::TAB_LIBRARY:
                handleTabLibrary();
                break;
            case AppState::TAB_TRANSFER:
                handleTabTransfer();
                break;
            case AppState::TAB_SETTINGS:
                handleTabSettings();
                break;
            case AppState::READER:
                handleReader();
                break;
            case AppState::READER_MENU:
                handleReaderMenu();
                break;
            case AppState::CHAPTER_LIST:
                handleChapterList();
                break;
            case AppState::BOOKMARK_LIST:
                handleBookmarkList();
                break;
            case AppState::GOTO_PAGE:
                handleGotoPage();
                break;
            case AppState::WIFI_UPLOAD:
                handleWiFiUpload();
                break;
            case AppState::READING_STATS:
                handleReadingStats();
                break;
            case AppState::SETTINGS_LAYOUT:
                handleSettingsLayout();
                break;
            case AppState::SETTINGS_REFRESH:
                handleSettingsRefresh();
                break;
            case AppState::SETTINGS_FONT:
                handleSettingsFont();
                break;
            case AppState::SETTINGS_WIFI:
                handleSettingsWiFi();
                break;
            case AppState::SETTINGS_LEGADO:
                handleSettingsLegado();
                break;
            default:
                break;
        }
        delay(50);
    }
}

void App::handleInit() {
    _browser.scan(BOOKS_DIR);
    _state = AppState::TAB_READING;
    _activeTab = 0;
    for (int i = 0; i < 4; i++) _tabNeedsRender[i] = true;
}

void App::handleFileBrowser() {
    static bool needsRender = true;
    if (needsRender) {
        auto& display = M5.Display;
        display.clear();
        
        // 标题
        display.setTextSize(2);
        display.setCursor(380, 15);
        display.println("书架");
        
        // 最近阅读
        int recentCount = _recent.getCount();
        if (recentCount > 0) {
            display.setTextSize(1);
            display.setCursor(40, 55);
            display.println("最近阅读:");
            
            for (int i = 0; i < recentCount && i < 3; i++) {
                const RecentBook* book = _recent.getBook(i);
                if (!book) continue;
                int y = 75 + i * 35;
                display.setCursor(60, y);
                int pct = _recent.getProgressPercent(i);
                String name = book->name;
                if (name.length() > 25) name = name.substring(0, 22) + "...";
                display.printf("%d. %s (%d%%)", i + 1, name.c_str(), pct);
            }
        }
        
        // 分隔线
        int fileStartY = recentCount > 0 ? 190 : 70;
        display.drawLine(30, fileStartY - 10, SCREEN_WIDTH - 30, fileStartY - 10, 0);
        
        display.setTextSize(1);
        display.setCursor(40, fileStartY);
        display.println("所有书籍:");
        
        // 文件列表
        _browser.renderAt(fileStartY + 25);
        
        display.display();
        needsRender = false;
    }
}

void App::handleReader() {
    // 阅读器由触摸事件驱动
}

// ===== 主菜单（分组式扁平列表）=====
void App::handleMenu() {
    auto& display = M5.Display;
    display.clear();
    
    display.setTextSize(2);
    display.setCursor(380, 20);
    display.println("阅读菜单");
    
    const char* items[] = {
        "继续阅读",
        "[阅读] 章节目录",
        "[阅读] 页码跳转",
        "[阅读] 添加书签",
        "[设置] 排版设置",
        "[设置] 字号调大",
        "[设置] 字号调小",
        "[设置] 残影控制",
        "[工具] 我的书签",
        "[工具] 字体切换",
        "[工具] WiFi传书",
        "[工具] 阅读统计",
        "[系统] 蓝牙翻页",
        "[系统] 返回书架",
        "[系统] 关闭设备",
        "WiFi配置",
        "Legado同步"
    };
    int numItems = 17;
    
    display.setTextSize(1);
    for (int i = 0; i < numItems; i++) {
        int y = 50 + i * 32;
        if (i == _menuIndex) {
            display.fillRect(150, y - 2, 660, 28, 2);
        }
        display.setCursor(170, y + 4);
        display.println(items[i]);
    }
    
    display.display();
}

// ===== 章节目录菜单 =====
void App::handleChapterMenu() {
    auto& display = M5.Display;
    display.clear();
    
    display.setTextSize(2);
    display.setCursor(360, 20);
    display.println("章节目录");
    
    int chapterCount = _reader.getChapterCount();
    if (chapterCount <= 0) {
        display.setTextSize(1);
        display.setCursor(300, 250);
        display.println("未检测到章节");
        display.setCursor(250, 280);
        display.println("（返回阅读后长按菜单识别）");
        display.display();
        return;
    }
    
    const ChapterInfo* chapters = _reader.getChapterList();
    int currentChapter = _reader.getCurrentChapterIndex();
    
    int itemsPerPage = 10;
    int totalPages = (chapterCount + itemsPerPage - 1) / itemsPerPage;
    int currentPage = _chapterMenuScroll / itemsPerPage;
    int startIdx = currentPage * itemsPerPage;
    int endIdx = min(startIdx + itemsPerPage, chapterCount);
    
    display.setTextSize(1);
    for (int i = startIdx; i < endIdx; i++) {
        int y = 70 + (i - startIdx) * 40;
        if (i == _chapterMenuIndex) {
            display.fillRect(150, y - 3, 660, 35, 2);
        }
        if (i == currentChapter) {
            display.drawRect(145, y - 5, 670, 39, 0);
        }
        display.setCursor(170, y + 5);
        String title = chapters[i].title;
        if (title.length() > 30) title = title.substring(0, 27) + "...";
        display.printf("%d. %s", i + 1, title.c_str());
    }
    
    display.setCursor(400, 480);
    display.printf("%d/%d 页", currentPage + 1, totalPages);
    
    display.display();
}

// ===== 残影控制菜单 =====
void App::handleRefreshMenu() {
    auto& display = M5.Display;
    display.clear();
    
    display.setTextSize(2);
    display.setCursor(360, 20);
    display.println("残影控制");
    
    RefreshStrategy strategy = _reader.getRefreshStrategy();
    const char* items[] = {
        "极速 - DU4快刷 / 每20页全刷",
        "均衡 - DU快刷 / 每10页全刷",
        "清晰 - GL16文本 / 每5页全刷"
    };
    int selected = (int)strategy.frequency;
    
    display.setTextSize(1);
    for (int i = 0; i < 3; i++) {
        int y = 100 + i * 60;
        if (i == selected) {
            display.fillRect(150, y - 5, 660, 50, 2);
        }
        display.setCursor(170, y + 10);
        display.println(items[i]);
    }
    
    display.setCursor(200, 400);
    display.println("Vink 刷新策略：速度优先 / 均衡 / 显示优先");
    display.setCursor(200, 450);
    display.println("← → 切换 | 点击确认 | 上滑返回");
    
    display.display();
}

// ===== 书签菜单 =====
void App::handleBookmarkMenu() {
    auto& display = M5.Display;
    display.clear();
    
    display.setTextSize(2);
    display.setCursor(360, 20);
    display.println("我的书签");
    
    int bmCount = _reader.getBookmarkCount();
    if (bmCount <= 0) {
        display.setTextSize(1);
        display.setCursor(320, 250);
        display.println("暂无书签");
        display.setCursor(250, 280);
        display.println("（阅读时点击菜单 → 添加书签）");
        display.display();
        return;
    }
    
    const Bookmark* bookmarks = _reader.getBookmarks();
    int itemsPerPage = 10;
    int totalPages = (bmCount + itemsPerPage - 1) / itemsPerPage;
    int currentPage = _bookmarkMenuScroll / itemsPerPage;
    int startIdx = currentPage * itemsPerPage;
    int endIdx = min(startIdx + itemsPerPage, bmCount);
    
    display.setTextSize(1);
    for (int i = startIdx; i < endIdx; i++) {
        int y = 70 + (i - startIdx) * 40;
        if (i == _bookmarkMenuIndex) {
            display.fillRect(150, y - 3, 660, 35, 2);
        }
        display.setCursor(170, y + 5);
        String name = bookmarks[i].name;
        if (name.length() > 30) name = name.substring(0, 27) + "...";
        display.printf("%d. %s (P%d)", i + 1, name.c_str(), bookmarks[i].pageNum + 1);
    }
    
    display.setCursor(400, 480);
    display.printf("%d 个书签 | %d/%d 页", bmCount, currentPage + 1, totalPages);
    
    display.display();
}

// ===== 排版设置子菜单 =====
void App::handleLayoutMenu() {
    auto& display = M5.Display;
    display.clear();
    
    display.setTextSize(2);
    display.setCursor(360, 20);
    display.println("排版设置");
    
    LayoutConfig layout = _reader.getLayoutConfig();
    
    struct LayoutItem {
        const char* name;
        int value;
        const char* unit;
        int minVal;
        int maxVal;
        int step;
    };
    
    LayoutItem items[] = {
        {"字号", layout.fontSize, "px", 12, 48, 1},
        {"行间距", layout.lineSpacing, "%", 50, 200, 10},
        {"段间距", layout.paragraphSpacing, "%", 0, 100, 10},
        {"左页边", layout.marginLeft, "px", 0, 120, 5},
        {"右页边", layout.marginRight, "px", 0, 120, 5},
        {"上页边", layout.marginTop, "px", 0, 100, 5},
        {"下页边", layout.marginBottom, "px", 0, 100, 5},
        {"首行缩进", layout.indentFirstLine, "字", 0, 4, 1},
        {"休眠时长", (int)_sleepTimeoutMin, "分", AUTO_SLEEP_MIN_MIN, AUTO_SLEEP_MAX_MIN, 1},
    };
    int numItems = 9;
    
    display.setTextSize(1);
    for (int i = 0; i < numItems; i++) {
        int y = 70 + i * 45;
        if (i == _layoutEditorIndex) {
            display.fillRect(150, y - 3, 660, 38, 2);
        }
        display.setCursor(170, y + 5);
        display.printf("%s: %d %s", items[i].name, items[i].value, items[i].unit);
    }
    
    display.setCursor(200, 480);
    display.println("← → 调整 | 点击确认 | 上滑返回");
    
    display.display();
}

void App::processTouch() {
    auto& touch = M5.Touch;
    
    if (touch.getCount() > 0) {
        _lastActivityTime = millis();
        _sleepPending = false;
        auto t = touch.getDetail();
        int x = t.x;
        int y = t.y;
        
        if (!_touching) {
            _touching = true;
            _touchStartX = x;
            _touchStartY = y;
            _touchStartTime = millis();
        } else {
            int dx = x - _touchStartX;
            int dy = y - _touchStartY;
            unsigned long dt = millis() - _touchStartTime;
            
            if (dt > 1000 && abs(dx) < 20 && abs(dy) < 20) {
                onLongPress(x, y);
                _touching = false;
                return;
            }
        }
    } else {
        if (_touching) {
            _touching = false;
            int dx = touch.getDetail().x - _touchStartX;
            int dy = touch.getDetail().y - _touchStartY;
            unsigned long dt = millis() - _touchStartTime;
            
            if (abs(dx) < 20 && abs(dy) < 20 && dt < 500) {
                onTap(touch.getDetail().x, touch.getDetail().y);
            } else if (dt < 500 && abs(dx) > 50) {
                onSwipe(dx, dy);
            } else if (dt < 500 && abs(dy) > 50) {
                onVerticalSwipe(dy);
            }
        }
    }
}

void App::onTap(int x, int y) {
    Serial.printf("[Touch] Tap at (%d, %d)\n", x, y);

    // 子页面左上角返回按钮：所有设置/统计/菜单页统一可退回上一级。
    if (!isTabState(_state) && _state != AppState::READER && y < 92 && x < 120) {
        navigateBack();
        return;
    }

    if (_state == AppState::SETTINGS_REFRESH) {
        int startY = 116;
        int itemH = 128;
        int gap = 20;
        int idx = (y - startY) / (itemH + gap);
        if (idx >= 0 && idx < 3) {
            _reader.setRefreshFrequency((RefreshFrequency)idx);
            handleSettingsRefresh();
        }
        return;
    }

    if (_state == AppState::SETTINGS_FONT) {
        int idx = (y - 112) / 92;
        if (idx >= 0 && idx < _fontCount) {
            if (_font.loadFont(_fontPaths[idx])) showMessage("字体已切换", 800);
            _state = AppState::TAB_SETTINGS;
            _tabNeedsRender[3] = true;
        }
        return;
    }

    if (_state == AppState::SETTINGS_LAYOUT) {
        _layoutEditorIndex = ((_layoutEditorIndex + 1) % 5);
        handleSettingsLayout();
        return;
    }

    if (_state == AppState::SETTINGS_WIFI || _state == AppState::SETTINGS_LEGADO) {
        navigateBack();
        return;
    }
    
    // ===== 新 UI 标签页处理 =====
    if (_state == AppState::TAB_READING || _state == AppState::TAB_LIBRARY ||
        _state == AppState::TAB_TRANSFER || _state == AppState::TAB_SETTINGS) {
        
        // 顶部标签切换
        if (y < TOP_TAB_H) {
            int tabW = SCREEN_WIDTH / 4;
            int clickedTab = x / tabW;
            if (clickedTab >= 0 && clickedTab < 4) {
                switchTab(clickedTab);
            }
            return;
        }
        
        // 底部导航区域（不做处理，只是提示）
        if (y > SCREEN_HEIGHT - BOTTOM_NAV_H) {
            return;
        }
        
        // 根据标签页处理
        switch (_state) {
            case AppState::TAB_READING: {
                int recentCount = _recent.getCount();
                int cardY = contentTop() + 34;
                if (recentCount > 0 && x >= contentLeft() && x <= contentRight() && y >= cardY && y <= cardY + 210) {
                    const RecentBook* book = _recent.getBook(0);
                    if (book && _reader.openBook(book->path)) {
                        _stats.startReading(book->name);
                        _state = AppState::READER;
                        _reader.renderPage();
                    }
                    return;
                }
                return;
            }
            
            case AppState::TAB_LIBRARY: {
                int cols = 2;
                int cardW = (contentWidth() - 14) / 2;
                int cardH = 156;
                int gapX = 14;
                int gapY = 16;
                int startX = contentLeft();
                int cy = contentTop() + 42;
                
                int clickedCol = (x - startX) / (cardW + gapX);
                int clickedRow = (y - cy) / (cardH + gapY);
                if (clickedCol >= 0 && clickedCol < cols && clickedRow >= 0 && clickedRow < 4) {
                    int localIdx = clickedRow * cols + clickedCol;
                    int idx = _libraryPage * 8 + localIdx;
                    if (idx < _browser.getItemCount()) {
                        _librarySelected = idx;
                        const FileItem* item = _browser.getItem(idx);
                        if (item && item->isDirectory) {
                            if (_browser.enterDirectory(idx)) {
                                _libraryPage = 0;
                                _librarySelected = 0;
                                _tabNeedsRender[1] = true;
                            }
                        } else if (item && _reader.openBook(item->path)) {
                            _stats.startReading(item->name);
                            _recent.addBook(item->path, item->name, 0, _reader.getTotalPages());
                            _state = AppState::READER;
                            _reader.renderPage();
                        }
                    }
                }
                return;
            }
            
            case AppState::TAB_TRANSFER: {
                const int itemH = 84;
                const int gap = 10;
                int drawY = contentTop() + 44;
                int clickedIdx = -1;
                const int groupBefore[] = {2, 3, 4};
                for (int i = 0; i < 6; i++) {
                    bool newGroup = (i == 0 || i == groupBefore[0] || i == groupBefore[1] || i == groupBefore[2]);
                    if (newGroup) drawY += 26;
                    if (y >= drawY && y <= drawY + itemH) {
                        clickedIdx = i;
                        break;
                    }
                    drawY += itemH + gap;
                }

                switch (clickedIdx) {
                    case 0:
                        if (_uploader.isRunning()) {
                            _uploader.stop();
                            showMessage("WiFi传书已关闭", 1500);
                        } else {
                            _uploader.start(_wifiSsid, _wifiPass);
                            showMessage("WiFi传书已开启", 1500);
                        }
                        _tabNeedsRender[2] = true;
                        break;
                    case 1:
                        _state = AppState::SETTINGS_WIFI;
                        break;
                    case 2:
                        _state = AppState::SETTINGS_LEGADO;
                        break;
                    case 3:
                        if (_ble.isRunning()) {
                            _ble.stop();
                            showMessage("蓝牙翻页已关闭", 1500);
                        } else {
                            _ble.start();
                            showMessage("蓝牙翻页已开启", 1500);
                        }
                        _tabNeedsRender[2] = true;
                        break;
                    case 4:
                        _state = AppState::READING_STATS;
                        break;
                    case 5:
                        switchTab(1);
                        break;
                }
                return;
            }
            
            case AppState::TAB_SETTINGS: {
                const int itemH = 76;
                const int gap = 10;
                int drawY = contentTop() + 44 - _settingsScroll;
                int clickedIdx = -1;
                const int groupBefore[] = {0, 3, 5};
                for (int i = 0; i < 7; i++) {
                    bool newGroup = (i == groupBefore[0] || i == groupBefore[1] || i == groupBefore[2]);
                    if (newGroup) drawY += 26;
                    if (y >= drawY && y <= drawY + itemH) {
                        clickedIdx = i;
                        break;
                    }
                    drawY += itemH + gap;
                }

                if (clickedIdx >= 0 && clickedIdx < 7) {
                    switch (clickedIdx) {
                        case 0: _layoutEditorIndex = 0; _state = AppState::SETTINGS_LAYOUT; break;
                        case 1: _state = AppState::SETTINGS_REFRESH; break;
                        case 2: _fontMenuIndex = 0; _fontMenuScroll = 0; _state = AppState::SETTINGS_FONT; break;
                        case 3: _state = AppState::SETTINGS_WIFI; break;
                        case 4: _state = AppState::SETTINGS_LEGADO; break;
                        case 5:
                            _sleepTimeoutMin = clamp(_sleepTimeoutMin + 5, AUTO_SLEEP_MIN_MIN, AUTO_SLEEP_MAX_MIN);
                            if (_sleepTimeoutMin > AUTO_SLEEP_MAX_MIN) _sleepTimeoutMin = AUTO_SLEEP_MIN_MIN;
                            saveGlobalSettings();
                            _tabNeedsRender[3] = true;
                            break;
                        case 6: showMessage("Vink-PaperS3 v1.2.8", 3000); break;
                    }
                }
                return;
            }
            
            default:
                break;
        }
        return;
    }
    
    // ===== 原有状态处理 =====
    switch (_state) {
        case AppState::FILE_BROWSER: {
            // 点击最近阅读区域
            if (y < 190 && _recent.getCount() > 0) {
                int recentIdx = (y - 75) / 35;
                if (recentIdx >= 0 && recentIdx < _recent.getCount() && recentIdx < 3) {
                    const RecentBook* book = _recent.getBook(recentIdx);
                    if (book && _reader.openBook(book->path)) {
                        _stats.startReading(book->name);
                        _state = AppState::READER;
                        _reader.renderPage();
                        return;
                    }
                }
            }
            // 右侧打开选中项目
            if (x >= ZONE_RIGHT_X) {
                const FileItem* item = _browser.getSelectedItem();
                if (item && item->isDirectory) {
                    if (_browser.enterDirectory(_browser.getSelectedIndex())) {
                        _browser.render();
                    } else {
                        showMessage("进入目录失败!");
                    }
                } else if (item && _reader.openBook(item->path)) {
                    _stats.startReading(item->name);
                    _recent.addBook(item->path, item->name, 0, _reader.getTotalPages());
                    _state = AppState::READER;
                    _reader.renderPage();
                } else {
                    showMessage("打开失败!");
                }
            }
            break;
        }
        
        case AppState::READER: {
            if (x < ZONE_LEFT_X + ZONE_LEFT_W) {
                _reader.prevPage();
                _stats.recordPageTurn();
            } else if (x >= ZONE_RIGHT_X) {
                if (!_reader.nextPage()) {
                    // 书末
                    handleEndOfBook();
                    return;
                }
                _stats.recordPageTurn();
                _recent.updateProgress(_reader.getBookPath(), _reader.getCurrentPage(), _reader.getTotalPages());
            } else {
                _menuIndex = 0;
                _state = AppState::MENU;
                handleMenu();
            }
            break;
        }
        
        case AppState::MENU: {
            int clickedIndex = (y - 50) / 32;
            if (clickedIndex < 0) clickedIndex = 0;
            if (clickedIndex > 16) clickedIndex = 16;
            _menuIndex = clickedIndex;
            executeMenuItem(clickedIndex);
            break;
        }
        
        case AppState::MENU_CHAPTER: {
            if (_reader.getChapterCount() > 0) {
                _reader.gotoChapter(_chapterMenuIndex);
                _state = AppState::READER;
            }
            break;
        }
        
        case AppState::MENU_REFRESH: {
            _state = AppState::READER;
            _reader.renderPage();
            break;
        }
        
        case AppState::MENU_BOOKMARK: {
            if (_reader.getBookmarkCount() > 0) {
                _reader.gotoBookmark(_bookmarkMenuIndex);
                _state = AppState::READER;
            }
            break;
        }
        
        case AppState::MENU_FONT: {
            if (_fontCount > 0 && _fontMenuIndex < _fontCount) {
                if (_font.loadFont(_fontPaths[_fontMenuIndex])) {
                    showMessage("字体已切换", 1000);
                    if (_reader.isOpen()) {
                        _reader.renderPage();
                    }
                }
            }
            _state = AppState::READER;
            break;
        }
        
        case AppState::MENU_GOTO_PAGE: {
            // 数字键盘区域: 300-620 x, 300-450 y
            if (y >= 300 && y < 450 && x >= 300 && x < 660) {
                int col = (x - 300) / 120;
                int row = (y - 300) / 50;
                int keyIdx = row * 3 + col;
                
                if (keyIdx == 9) { // 删除
                    if (_gotoPageCursor > 0) {
                        _gotoPageCursor--;
                        _gotoPageInput[_gotoPageCursor] = '\0';
                    }
                } else if (keyIdx == 11) { // 跳转
                    if (_gotoPageCursor > 0) {
                        int page = atoi(_gotoPageInput) - 1; // 用户输入1-based
                        if (page >= 0 && page < _reader.getTotalPages()) {
                            _reader.gotoPage(page);
                            _state = AppState::READER;
                        } else {
                            showMessage("页码超出范围");
                        }
                    }
                } else if (keyIdx >= 0 && keyIdx < 9) { // 1-9
                    if (_gotoPageCursor < 6) {
                        _gotoPageInput[_gotoPageCursor++] = '1' + keyIdx;
                        _gotoPageInput[_gotoPageCursor] = '\0';
                    }
                } else if (keyIdx == 10) { // 0
                    if (_gotoPageCursor < 6) {
                        _gotoPageInput[_gotoPageCursor++] = '0';
                        _gotoPageInput[_gotoPageCursor] = '\0';
                    }
                }
                handleGotoPage();
            } else if (y < 200) {
                // 点击输入框上方=取消
                _state = AppState::READER;
                _reader.renderPage();
            }
            break;
        }
        
        case AppState::WIFI_UPLOAD:
        case AppState::READING_STATS: {
            _state = AppState::READER;
            _reader.renderPage();
            break;
        }
        
        case AppState::MENU_LAYOUT: {
            _state = AppState::READER;
            _reader.renderPage();
            break;
        }
        
        default:
            break;
    }
}

void App::onSwipe(int dx, int dy) {
    Serial.printf("[Touch] Swipe dx=%d, dy=%d\n", dx, dy);

    // 子页面右滑返回上一级；比“上滑返回”更符合电子书/手机习惯。
    if (!isTabState(_state) && _state != AppState::READER && dx > 90) {
        navigateBack();
        return;
    }
    
    // 新UI标签页：左右滑动切换标签
    if (isTabState(_state)) {
        if (dx > 80) {
            // 右滑：上一个标签
            switchTab(_activeTab - 1);
        } else if (dx < -80) {
            // 左滑：下一个标签
            switchTab(_activeTab + 1);
        }
        return;
    }

    if (_state == AppState::SETTINGS_LAYOUT) {
        adjustLayoutParameter(dx > 0 ? 1 : -1);
        handleSettingsLayout();
        return;
    }
    
    switch (_state) {
        case AppState::FILE_BROWSER: {
            if (dy > 30) {
                _browser.selectNext();
                _browser.render();
            } else if (dy < -30) {
                _browser.selectPrev();
                _browser.render();
            }
            break;
        }
        
        case AppState::READER: {
            if (dx < -50) _reader.nextPage();
            else if (dx > 50) _reader.prevPage();
            break;
        }
        
        case AppState::MENU_LAYOUT: {
            adjustLayoutParameter(dx > 0 ? 1 : -1);
            handleLayoutMenu();
            break;
        }
        
        case AppState::MENU_CHAPTER: {
            if (dx < -50 && _chapterMenuScroll < _reader.getChapterCount() - 1) {
                _chapterMenuScroll += 10;
                handleChapterMenu();
            } else if (dx > 50 && _chapterMenuScroll > 0) {
                _chapterMenuScroll -= 10;
                handleChapterMenu();
            }
            break;
        }
        
        case AppState::MENU_FONT: {
            if (dx < -50 && _fontMenuScroll < _fontCount - 1) {
                _fontMenuScroll += 10;
                handleFontMenu();
            } else if (dx > 50 && _fontMenuScroll > 0) {
                _fontMenuScroll -= 10;
                handleFontMenu();
            }
            break;
        }
        
        case AppState::MENU_BOOKMARK: {
            int bmCount = _reader.getBookmarkCount();
            if (dx < -50 && _bookmarkMenuScroll < bmCount - 1) {
                _bookmarkMenuScroll += 10;
                handleBookmarkMenu();
            } else if (dx > 50 && _bookmarkMenuScroll > 0) {
                _bookmarkMenuScroll -= 10;
                handleBookmarkMenu();
            }
            break;
        }
        
        case AppState::MENU_REFRESH: {
            RefreshStrategy current = _reader.getRefreshStrategy();
            RefreshFrequency newFreq;
            if (dx > 0) {
                newFreq = (current.frequency == RefreshFrequency::FREQ_LOW) ? RefreshFrequency::FREQ_MEDIUM :
                          (current.frequency == RefreshFrequency::FREQ_MEDIUM) ? RefreshFrequency::FREQ_HIGH :
                          RefreshFrequency::FREQ_HIGH;
            } else {
                newFreq = (current.frequency == RefreshFrequency::FREQ_HIGH) ? RefreshFrequency::FREQ_MEDIUM :
                          (current.frequency == RefreshFrequency::FREQ_MEDIUM) ? RefreshFrequency::FREQ_LOW :
                          RefreshFrequency::FREQ_LOW;
            }
            _reader.setRefreshFrequency(newFreq);
            handleRefreshMenu();
            break;
        }
        
        default:
            break;
    }
}

void App::onVerticalSwipe(int dy) {
    if (!isTabState(_state) && _state != AppState::READER && dy < -90) {
        navigateBack();
        return;
    }

    // 新UI：设置页面滚动
    if (_state == AppState::TAB_SETTINGS) {
        if (dy > 30) {
            _settingsScroll += 20;
            _tabNeedsRender[3] = true;
        } else if (dy < -30) {
            _settingsScroll -= 20;
            if (_settingsScroll < 0) _settingsScroll = 0;
            _tabNeedsRender[3] = true;
        }
        return;
    }
    
    // 新UI：阅读器菜单
    if (_state == AppState::READER_MENU) {
        if (dy < -30 && _menuIndex > 0) {
            _menuIndex--;
            handleReaderMenu();
        } else if (dy > 30 && _menuIndex < 11) {
            _menuIndex++;
            handleReaderMenu();
        } else if (dy > 100) {
            _state = AppState::READER;
            _reader.renderPage();
        }
        return;
    }
    
    switch (_state) {
        case AppState::MENU: {
            if (dy < -30 && _menuIndex > 0) {
                _menuIndex--;
                handleMenu();
            } else if (dy > 30 && _menuIndex < 16) {
                _menuIndex++;
                handleMenu();
            }
            break;
        }
        
        case AppState::MENU_CHAPTER: {
            int chapterCount = _reader.getChapterCount();
            if (dy < -30 && _chapterMenuIndex > 0) {
                _chapterMenuIndex--;
                if (_chapterMenuIndex < _chapterMenuScroll) {
                    _chapterMenuScroll = (_chapterMenuIndex / 10) * 10;
                }
                handleChapterMenu();
            } else if (dy > 30 && _chapterMenuIndex < chapterCount - 1) {
                _chapterMenuIndex++;
                if (_chapterMenuIndex >= _chapterMenuScroll + 10) {
                    _chapterMenuScroll = (_chapterMenuIndex / 10) * 10;
                }
                handleChapterMenu();
            } else if (dy > 100) {
                _state = AppState::READER;
                _reader.renderPage();
            }
            break;
        }
        
        case AppState::MENU_BOOKMARK: {
            int bmCount = _reader.getBookmarkCount();
            if (dy < -30 && _bookmarkMenuIndex > 0) {
                _bookmarkMenuIndex--;
                if (_bookmarkMenuIndex < _bookmarkMenuScroll) {
                    _bookmarkMenuScroll = (_bookmarkMenuIndex / 10) * 10;
                }
                handleBookmarkMenu();
            } else if (dy > 30 && _bookmarkMenuIndex < bmCount - 1) {
                _bookmarkMenuIndex++;
                if (_bookmarkMenuIndex >= _bookmarkMenuScroll + 10) {
                    _bookmarkMenuScroll = (_bookmarkMenuIndex / 10) * 10;
                }
                handleBookmarkMenu();
            } else if (dy > 100) {
                _state = AppState::READER;
                _reader.renderPage();
            }
            break;
        }
        
        case AppState::MENU_FONT: {
            if (dy < -30 && _fontMenuIndex > 0) {
                _fontMenuIndex--;
                handleFontMenu();
            } else if (dy > 30 && _fontMenuIndex < _fontCount - 1) {
                _fontMenuIndex++;
                handleFontMenu();
            } else if (dy > 100) {
                _state = AppState::READER;
                _reader.renderPage();
            }
            break;
        }
        
        case AppState::MENU_LAYOUT: {
            if (dy < -30 && _layoutEditorIndex > 0) {
                _layoutEditorIndex--;
                handleLayoutMenu();
            } else if (dy > 30) {
                if (_layoutEditorIndex < 8) {
                    _layoutEditorIndex++;
                    handleLayoutMenu();
                } else {
                    _state = AppState::READER;
                    _reader.renderPage();
                }
            }
            break;
        }
        
        default:
            break;
    }
}

void App::onLongPress(int x, int y) {
    Serial.printf("[Touch] Long press at (%d, %d)\n", x, y);
    if (_state == AppState::READER) {
        _menuIndex = 0;
        _state = AppState::READER_MENU;
        handleReaderMenu();
    }
}

void App::executeMenuItem(int index) {
    // 新UI：阅读器弹出菜单处理
    if (_state == AppState::READER_MENU) {
        switch (index) {
            case 0: { // 章节目录
                if (_reader.getChapterCount() <= 0) _reader.detectChapters();
                _chapterMenuIndex = max(0, _reader.getCurrentChapterIndex());
                _chapterMenuScroll = (_chapterMenuIndex / 10) * 10;
                _state = AppState::CHAPTER_LIST;
                break;
            }
            case 1: // 添加书签
                if (_reader.addBookmark()) showMessage("书签已添加", 1500);
                else showMessage("添加失败", 1500);
                _state = AppState::READER;
                _reader.renderPage();
                break;
            case 2: // 我的书签
                _bookmarkMenuIndex = 0;
                _bookmarkMenuScroll = 0;
                _state = AppState::BOOKMARK_LIST;
                break;
            case 3: // 页码跳转
                memset(_gotoPageInput, 0, sizeof(_gotoPageInput));
                _gotoPageCursor = 0;
                _state = AppState::GOTO_PAGE;
                handleGotoPage();
                break;
            case 4: // 排版设置
                _layoutEditorIndex = 0;
                _state = AppState::SETTINGS_LAYOUT;
                break;
            case 5: // 字体切换
                _fontMenuIndex = 0;
                _fontMenuScroll = 0;
                _state = AppState::SETTINGS_FONT;
                break;
            case 6: // 残影控制
                _state = AppState::SETTINGS_REFRESH;
                break;
            case 7: // WiFi传书
                if (_uploader.isRunning()) {
                    _uploader.stop();
                    showMessage("WiFi传书已关闭", 1500);
                } else {
                    _uploader.start(_wifiSsid, _wifiPass);
                    showMessage("WiFi传书已开启", 1500);
                }
                _state = AppState::READER;
                _reader.renderPage();
                break;
            case 8: // 阅读统计
                _state = AppState::READING_STATS;
                break;
            case 9: // 蓝牙翻页
                if (_ble.isRunning()) {
                    _ble.stop();
                    showMessage("蓝牙翻页已关闭", 1500);
                } else {
                    _ble.start();
                    showMessage("蓝牙翻页已开启", 1500);
                }
                _state = AppState::READER;
                _reader.renderPage();
                break;
            case 10: // 返回书架
                _stats.stopReading();
                _stats.save();
                _reader.closeBook();
                _browser.scan(BOOKS_DIR);
                _state = AppState::TAB_READING;
                _activeTab = 0;
                for (int i = 0; i < 4; i++) _tabNeedsRender[i] = true;
                break;
            case 11: // 关闭设备
                shutdownDevice("正在关机...");
                break;
        }
        return;
    }
    
    switch (index) {
        case 0: // 继续阅读
            _state = AppState::READER;
            _reader.renderPage();
            break;
            
        case 1: { // 章节目录
            if (_reader.getChapterCount() <= 0) {
                _reader.detectChapters();
            }
            _chapterMenuIndex = max(0, _reader.getCurrentChapterIndex());
            _chapterMenuScroll = (_chapterMenuIndex / 10) * 10;
            _state = AppState::MENU_CHAPTER;
            handleChapterMenu();
            break;
        }
        
        case 2: { // 页码跳转
            memset(_gotoPageInput, 0, sizeof(_gotoPageInput));
            _gotoPageCursor = 0;
            _state = AppState::MENU_GOTO_PAGE;
            handleGotoPage();
            break;
        }
        
        case 3: { // 添加书签
            if (_reader.addBookmark()) {
                showMessage("书签已添加", 1500);
            } else {
                showMessage("添加失败", 1500);
            }
            _state = AppState::READER;
            break;
        }
        
        case 4: // 排版设置
            _layoutEditorIndex = 0;
            _state = AppState::MENU_LAYOUT;
            handleLayoutMenu();
            break;
            
        case 5: // 字号调大
            _reader.changeFontSize(+2);
            _state = AppState::READER;
            break;
            
        case 6: // 字号调小
            _reader.changeFontSize(-2);
            _state = AppState::READER;
            break;
            
        case 7: // 残影控制
            _state = AppState::MENU_REFRESH;
            handleRefreshMenu();
            break;
            
        case 8: // 我的书签
            _bookmarkMenuIndex = 0;
            _bookmarkMenuScroll = 0;
            _state = AppState::MENU_BOOKMARK;
            handleBookmarkMenu();
            break;
            
        case 9: // 字体切换
            _fontMenuIndex = 0;
            _fontMenuScroll = 0;
            _state = AppState::MENU_FONT;
            handleFontMenu();
            break;
            
        case 10: { // WiFi传书
            if (_uploader.isRunning()) {
                _uploader.stop();
                showMessage("WiFi传书已关闭", 1500);
            } else {
                showMessage("请在配置中设置WiFi", 2000);
            }
            _state = AppState::READER;
            break;
        }
        
        case 11: // 阅读统计
            _state = AppState::READING_STATS;
            handleReadingStats();
            break;
            
        case 12: { // 蓝牙翻页
            if (_ble.isRunning()) {
                _ble.stop();
                showMessage("蓝牙翻页已关闭", 1500);
            } else {
                _ble.start();
                showMessage("蓝牙翻页已开启", 1500);
            }
            _state = AppState::READER;
            break;
        }
        
        case 13: // 返回书架
            _stats.stopReading();
            _stats.save();
            _reader.closeBook();
            _browser.scan(BOOKS_DIR);
            _state = AppState::TAB_READING;
            _activeTab = 0;
            for (int i = 0; i < 4; i++) _tabNeedsRender[i] = true;
            break;
            
        case 14: // 关闭设备
            shutdownDevice("正在关机...");
            break;
            
        case 15: // WiFi配置
            handleWiFiConfig();
            break;
            
        case 16: // Legado同步
            handleLegadoSync();
            break;
    }
}

void App::adjustLayoutParameter(int delta) {
    if (_layoutEditorIndex < 8) {
        LayoutConfig layout = _reader.getLayoutConfig();
        switch (_layoutEditorIndex) {
            case 0: layout.fontSize = clamp(layout.fontSize + delta, 12, 48); break;
            case 1: layout.lineSpacing = clamp(layout.lineSpacing + delta * 10, 50, 200); break;
            case 2: layout.paragraphSpacing = clamp(layout.paragraphSpacing + delta * 10, 0, 100); break;
            case 3: layout.marginLeft = clamp(layout.marginLeft + delta * 5, 0, 120); break;
            case 4: layout.marginRight = clamp(layout.marginRight + delta * 5, 0, 120); break;
            case 5: layout.marginTop = clamp(layout.marginTop + delta * 5, 0, 100); break;
            case 6: layout.marginBottom = clamp(layout.marginBottom + delta * 5, 0, 100); break;
            case 7: layout.indentFirstLine = clamp(layout.indentFirstLine + delta, 0, 4); break;
        }
        _reader.setLayoutConfig(layout);
    } else if (_layoutEditorIndex == 8) {
        // 休眠时长
        _sleepTimeoutMin = clamp((int)_sleepTimeoutMin + delta, AUTO_SLEEP_MIN_MIN, AUTO_SLEEP_MAX_MIN);
        saveGlobalSettings();
    }
}

int App::clamp(int val, int minVal, int maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

void App::showMessage(const char* msg, int durationMs) {
    auto& display = M5.Display;
    display.clear();
    display.setTextSize(2);
    
    // 简单估算文本宽度: 每个字符约12px (textSize=2)
    int w = strlen(msg) * 12;
    int h = 16;
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.println(msg);
    
    display.display();
    
    if (durationMs > 0) {
        delay(durationMs);
    }
}

// ===== 全局设置 =====

void App::loadGlobalSettings() {
    File f = SD.open(CONFIG_PATH, FILE_READ);
    if (!f) {
        Serial.println("[Settings] No config file, using defaults");
        _sleepTimeoutMin = AUTO_SLEEP_DEFAULT_MIN;
        return;
    }
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        _sleepTimeoutMin = AUTO_SLEEP_DEFAULT_MIN;
        return;
    }
    _sleepTimeoutMin = doc["sleepTimeoutMin"] | AUTO_SLEEP_DEFAULT_MIN;
    if (_sleepTimeoutMin < AUTO_SLEEP_MIN_MIN) _sleepTimeoutMin = AUTO_SLEEP_MIN_MIN;
    if (_sleepTimeoutMin > AUTO_SLEEP_MAX_MIN) _sleepTimeoutMin = AUTO_SLEEP_MAX_MIN;
    Serial.printf("[Settings] Loaded: sleep=%d min\n", _sleepTimeoutMin);
}

void App::saveGlobalSettings() {
    File f = SD.open(CONFIG_PATH, FILE_WRITE);
    if (!f) return;
    DynamicJsonDocument doc(2048);
    doc["sleepTimeoutMin"] = _sleepTimeoutMin;
    doc["timestamp"] = millis();
    serializeJson(doc, f);
    f.close();
    Serial.printf("[Settings] Saved: sleep=%d min\n", _sleepTimeoutMin);
}

// ===== 电量显示 =====

void App::drawBatteryIcon(int x, int y) {
#if BATTERY_ICON_ENABLED
    auto& display = M5.Display;
    BatteryInfo bat = BatteryInfo::read();
    if (!bat.valid) return;
    
    display.drawRect(x, y, 24, 12, 0);
    display.drawRect(x + 24, y + 3, 3, 6, 0);
    
    int fillW = (bat.level * 20) / 100;
    uint16_t color = TFT_BLACK;
    
    if (fillW > 0) {
        display.fillRect(x + 2, y + 2, fillW, 8, color);
    }
    
    if (bat.charging) {
        display.drawPixel(x + 12, y + 6, 15);
    }
#endif
}

// BatteryInfo::read() 外部实现（Config.h 中只是声明，因 M5 尚未 include）
BatteryInfo BatteryInfo::read() {
    BatteryInfo info;
    info.level = M5.Power.getBatteryLevel();
    info.charging = M5.Power.isCharging();
    info.valid = (info.level >= 0 && info.level <= 100);
    return info;
}

// ===== 电源 / 休眠管理 =====

static constexpr int PAPER_S3_POWER_OFF_PIN = 44;
static constexpr int PAPER_S3_POWER_KEY_PIN = 36;

static void pulsePaperS3PowerOffPin() {
    // CrossPoint/ReadPaper 类 PaperS3 关机路径：GPIO44 给 PMIC 一个高脉冲。
    // M5Unified 的 powerOff 在部分情况下会退到 deep sleep/重启观感；这里显式
    // 先走 PaperS3 硬件关机脉冲，再让 M5Unified 做兜底。
    pinMode(PAPER_S3_POWER_OFF_PIN, OUTPUT);
    digitalWrite(PAPER_S3_POWER_OFF_PIN, HIGH);
    delay(150);
    digitalWrite(PAPER_S3_POWER_OFF_PIN, LOW);
    delay(100);
}

void App::handlePowerButton() {
    if (_shutdownInProgress) return;

    // 目标行为：按一次硬件开机；系统起来后，再按一次才关机。
    // 因此启动后的前几秒必须忽略“开机那次按键”的残留状态，等电源键
    // 释放后再正式接管，避免刚开机就被固件误判为关机请求。
    if (!_powerButtonArmed) {
        if (millis() > 3000 && !M5.BtnPWR.isPressed()) {
            _powerButtonArmed = true;
            _powerButtonPressStart = 0;
            Serial.println("[Power] BtnPWR armed after boot release");
        }
        return;
    }

    // 不等 wasClicked() 释放后才处理：PaperS3 的 PMIC 长按可能先触发硬件重启。
    // 接管后只要检测到一次稳定按下，就立即进入保存+关机流程。
    if (M5.BtnPWR.isPressed()) {
        if (_powerButtonPressStart == 0) {
            _powerButtonPressStart = millis();
        }
        if (millis() - _powerButtonPressStart > 80) {
            Serial.println("[Power] BtnPWR pressed, shutting down immediately");
            shutdownDevice("正在关机...");
        }
        return;
    }

    _powerButtonPressStart = 0;
    if (M5.BtnPWR.wasClicked() || M5.BtnPWR.wasHold()) {
        Serial.println("[Power] BtnPWR click/hold event, shutting down");
        shutdownDevice("正在关机...");
    }
}

void App::shutdownDevice(const char* reason) {
    if (_shutdownInProgress) return;
    _shutdownInProgress = true;

    if (_reader.isOpen()) {
        _reader.saveProgress();
    }
    _stats.save();
    if (_uploader.isRunning()) {
        _uploader.stop();
    }
    if (_ble.isRunning()) {
        _ble.stop();
    }

    // 参考 ReadPaper：先完整刷新关机提示并等待电子纸完成刷新，再断电。
    showMessage(reason ? reason : "正在关机...", 1200);
    delay(300);
    M5.Display.waitDisplay();
    delay(500);

    // 若用户仍按着电源键，先等释放，避免 deep sleep 兜底被 GPIO36 立即唤醒，
    // 看起来像“按电源键重启”。最多等 3 秒，防止异常卡死。
    unsigned long waitStart = millis();
    while (M5.BtnPWR.isPressed() && millis() - waitStart < 3000) {
        M5.update();
        delay(30);
    }

    pulsePaperS3PowerOffPin();
    M5.Display.waitDisplay();
    M5.Power.powerOff();

    // 正常不会走到这里。若 PMIC 未真正断电，退回深睡；进入深睡前再次确保
    // 电源键已释放，否则 ext0 低电平会立刻唤醒，表现成重启。
    waitStart = millis();
    while (digitalRead(PAPER_S3_POWER_KEY_PIN) == LOW && millis() - waitStart < 3000) {
        delay(30);
    }
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PAPER_S3_POWER_KEY_PIN, 0);
    esp_deep_sleep_start();
}

void App::checkAutoSleep() {
#if AUTO_SLEEP_ENABLED
    unsigned long timeoutMs = (unsigned long)_sleepTimeoutMin * 60 * 1000;
    unsigned long idle = millis() - _lastActivityTime;
    
    if (idle > timeoutMs) {
        if (!_sleepPending) {
            _sleepPending = true;
            Serial.printf("[Sleep] Idle %d min, entering sleep...\n", _sleepTimeoutMin);
            enterSleep();
        }
    }
#endif
}

void App::enterSleep() {
    if (_reader.isOpen()) {
        _reader.saveProgress();
    }
    
    // 绘制休眠屏
    drawSleepScreen();
    
    delay(500);
    
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, 0);
    esp_light_sleep_start();
    
    wakeUp();
}

void App::drawSleepScreen() {
    auto& display = M5.Display;
    display.clear();
    
    // 书名
    display.setTextSize(2);
    display.setCursor(300, 80);
    if (_reader.isOpen()) {
        String name = _reader.getBookPath();
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0) name = name.substring(lastSlash + 1);
        if (name.length() > 20) name = name.substring(0, 17) + "...";
        display.println(name.c_str());
        
        // 进度条
        int pct = (_reader.getTotalPages() > 0) ? 
            (_reader.getCurrentPage() * 100 / _reader.getTotalPages()) : 0;
        display.setTextSize(1);
        display.setCursor(400, 140);
        display.printf("阅读进度: %d%%", pct);
        
        display.drawRect(280, 170, 400, 20, 0);
        display.fillRect(282, 172, (pct * 396) / 100, 16, 0);
    } else {
        display.println("Vink-PaperS3");
    }
    
    // 电量
    BatteryInfo bat = BatteryInfo::read();
    if (bat.valid) {
        display.setTextSize(1);
        display.setCursor(420, 260);
        display.printf("电量: %d%%", bat.level);
    }
    
    display.setCursor(350, 350);
    display.println("轻触屏幕唤醒");
    
    display.display();
}

void App::handleEndOfBook() {
    auto& display = M5.Display;
    display.clear();
    
    display.setTextSize(2);
    display.setCursor(340, 180);
    display.println("本书已读完");
    
    display.setTextSize(1);
    display.setCursor(360, 240);
    display.println("感谢阅读!");
    
    display.setCursor(300, 320);
    display.println("点击返回书架 | 左滑继续停留");
    
    display.display();
    
    // 等待用户操作
    unsigned long start = millis();
    while (millis() - start < 10000) {
        M5.update();
        auto& touch = M5.Touch;
        if (touch.getCount() > 0) {
            auto t = touch.getDetail();
            int dx = t.x - _touchStartX;
            if (t.x >= ZONE_RIGHT_X) {
                // 返回书架
                _stats.stopReading();
                _stats.save();
                _reader.closeBook();
                _browser.scan(BOOKS_DIR);
                _state = AppState::TAB_READING;
                _activeTab = 0;
                for (int i = 0; i < 4; i++) _tabNeedsRender[i] = true;
                return;
            } else if (t.x < ZONE_LEFT_X + ZONE_LEFT_W) {
                // 左滑=停留
                _state = AppState::READER;
                _reader.renderPage();
                return;
            }
        }
        delay(50);
    }
    
    // 超时自动返回书架
    _stats.stopReading();
    _stats.save();
    _reader.closeBook();
    _browser.scan(BOOKS_DIR);
    _state = AppState::TAB_READING;
    _activeTab = 0;
    for (int i = 0; i < 4; i++) _tabNeedsRender[i] = true;
}

void App::handleGotoPage() {
    auto& display = M5.Display;
    display.clear();
    
    display.setTextSize(2);
    display.setCursor(340, 60);
    display.println("页码跳转");
    
    display.setTextSize(1);
    display.setCursor(300, 140);
    display.printf("共 %d 页", _reader.getTotalPages());
    
    // 输入框
    display.drawRect(350, 200, 260, 60, 0);
    display.setTextSize(2);
    display.setCursor(370, 220);
    display.printf("%s", _gotoPageInput[0] ? _gotoPageInput : "_");
    
    // 数字键盘
    display.setTextSize(1);
    const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "删除", "0", "跳转"};
    for (int i = 0; i < 12; i++) {
        int row = i / 3;
        int col = i % 3;
        int kx = 300 + col * 120;
        int ky = 300 + row * 50;
        display.drawRect(kx, ky, 100, 40, 0);
        display.setCursor(kx + 35, ky + 15);
        display.println(keys[i]);
    }
    
    display.display();
}

void App::wakeUp() {
    Serial.println("[Sleep] Woken up");
    _lastActivityTime = millis();
    _sleepPending = false;
    
    M5.Touch.begin(&M5.Display);
    
    if (_state == AppState::READER && _reader.isOpen()) {
        _reader.renderPage();
    } else if (isTabState(_state)) {
        _browser.render();
    } else {
        auto& display = M5.Display;
        display.clear();
        display.display();
    }
}

// ===== 字体切换菜单 =====

void App::handleFontMenu() {
    auto& display = M5.Display;
    display.clear();
    
    display.setTextSize(2);
    display.setCursor(360, 20);
    display.println("字体切换");
    
    if (_fontCount <= 0) {
        display.setTextSize(1);
        display.setCursor(300, 250);
        display.println("未找到字体文件");
        display.setCursor(250, 280);
        display.println("（请放入 /fonts/*.fnt）");
        display.display();
        return;
    }
    
    display.setTextSize(1);
    for (int i = 0; i < _fontCount; i++) {
        int y = 70 + i * 40;
        if (i == _fontMenuIndex) {
            display.fillRect(150, y - 3, 660, 35, 2);
        }
        // 当前字体标记
        bool isCurrent = (strcmp(_font.getCurrentFontPath(), _fontPaths[i]) == 0);
        display.setCursor(170, y + 5);
        display.printf("%s %s", _fontNames[i], isCurrent ? "[当前]" : "");
    }
    
    display.setCursor(200, 480);
    display.println("点击切换 | 上滑返回");
    
    display.display();
}

// ===== WiFi传书界面 =====

void App::handleWiFiUpload() {
    auto& display = M5.Display;
    display.clear();
    
    display.setTextSize(2);
    display.setCursor(320, 20);
    display.println("WiFi传书");
    
    display.setTextSize(1);
    if (_uploader.isRunning()) {
        display.setCursor(200, 100);
        display.println("WiFi 已连接");
        display.setCursor(200, 140);
        display.printf("IP: %s", _uploader.getIP().c_str());
        display.setCursor(200, 180);
        display.println("端口: 8080");
        display.setCursor(200, 240);
        display.println("手机/电脑浏览器访问上方地址");
        display.setCursor(200, 280);
        display.println("选择 .txt 文件上传");
    } else {
        display.setCursor(250, 200);
        display.println("WiFi传书未启动");
        display.setCursor(200, 240);
        display.println("请到菜单中开启");
    }
    
    if (_uploader.hasNewUpload()) {
        display.setCursor(200, 350);
        display.printf("新上传: %s", _uploader.getLastUploadName().c_str());
        _uploader.clearNewUpload();
        _browser.scan(BOOKS_DIR);
    }
    
    display.setCursor(250, 450);
    display.println("点击返回阅读");
    
    display.display();
}

// ===== 阅读统计 =====

void App::handleReadingStats() {
    auto& display = M5.Display;
    prepareShellFrame();
    drawBackHeader("阅读统计", "统计当前书籍和累计阅读");

    int x = contentLeft();
    int y = 120;
    UITheme::drawCard(x, y, contentWidth(), 280, BG_WHITE, BORDER_LIGHT);
    display.setTextSize(1);
    display.setTextColor(TEXT_BLACK, BG_WHITE);
    if (_reader.isOpen()) {
        BookStats book = _stats.getCurrentBookStats();
        display.setCursor(x + 18, y + 24);
        display.printf("当前书翻页：%d 页", book.totalPagesRead);
        display.setCursor(x + 18, y + 60);
        display.printf("当前书阅读：%s", _stats.formatTime(book.totalSeconds).c_str());
        display.setCursor(x + 18, y + 96);
        display.printf("打开次数：%d", book.readCount);
    } else {
        display.setCursor(x + 18, y + 24);
        display.print("当前没有打开书籍");
    }
    display.setCursor(x + 18, y + 154);
    display.printf("累计阅读：%s", _stats.formatTime(_stats.getTotalReadingSeconds()).c_str());
    display.setCursor(x + 18, y + 190);
    display.printf("累计翻页：%d 页", _stats.getTotalPagesRead());
    display.setCursor(x + 18, y + 226);
    display.printf("今日阅读：%s / %d 页", _stats.formatTime(_stats.getTodaySeconds()).c_str(), _stats.getTodayPages());
    commitShellFrame();
}

// ===== BLE 翻页器检查 =====

void App::checkBleCommands() {
    if (!_ble.isRunning()) return;
    int cmd = _ble.checkCommand();
    if (cmd == 1) {
        _reader.nextPage();
    } else if (cmd == -1) {
        _reader.prevPage();
    }
}

void App::handleLegadoSync() {
    auto& display = M5.Display;
    prepareShellFrame();
    drawBackHeader("Legado同步", "WebDAV 阅读进度同步");

    int x = contentLeft();
    int y = 124;
    UITheme::drawCard(x, y, contentWidth(), 220, BG_WHITE, BORDER_LIGHT);
    display.setTextSize(2);
    display.setTextColor(TEXT_BLACK, BG_WHITE);
    display.setCursor(x + 18, y + 22);
    display.print(_legadoConfigured ? "已配置" : "未配置");
    display.setTextSize(1);
    display.setCursor(x + 18, y + 76);
    display.print(_legadoConfigured ? _legadoHost : "请在SD卡根目录放置 /legado_config.json");

    if (_legadoConfigured) {
        syncLegadoProgress();
        display.setCursor(x + 18, y + 128);
        display.print("同步完成");
    } else {
        display.setCursor(x + 18, y + 128);
        display.print("用于和阅读/Legado同步阅读进度");
    }
    commitShellFrame();
}

void App::handleWiFiConfig() {
    auto& display = M5.Display;
    prepareShellFrame();
    drawBackHeader("WiFi配置", "在SD卡根目录编辑配置文件");

    int x = contentLeft();
    int y = 124;
    UITheme::drawCard(x, y, contentWidth(), 270, BG_WHITE, BORDER_LIGHT);
    display.setTextSize(2);
    display.setTextColor(TEXT_BLACK, BG_WHITE);
    display.setCursor(x + 18, y + 20);
    display.print(_wifiConfigured ? "已配置" : "未配置");
    display.setTextSize(1);
    display.setCursor(x + 18, y + 72);
    display.print("请在SD卡根目录创建：");
    display.setCursor(x + 18, y + 104);
    display.print("/wifi_config.json");
    display.setCursor(x + 18, y + 148);
    display.print("{\"ssid\":\"你的WiFi\",");
    display.setCursor(x + 18, y + 178);
    display.print(" \"pass\":\"密码\"}");
    display.setCursor(x + 18, y + 222);
    display.print("保存后重启或重新进入页面生效");

    if (_wifiConfigured) {
        UITheme::drawCard(x, y + 300, contentWidth(), 90, BG_WHITE, BORDER_LIGHT);
        display.setTextSize(1);
        display.setTextColor(TEXT_BLACK, BG_WHITE);
        display.setCursor(x + 18, y + 330);
        display.printf("当前WiFi：%s", _wifiSsid);
    }
    commitShellFrame();
}

void App::syncLegadoProgress() {
    if (!_reader.isOpen() || !_legadoConfigured) return;
    // push current progress
    LegadoProgress prog;
    memset(&prog, 0, sizeof(prog));
    strlcpy(prog.bookUrl, _reader.getBookPath(), sizeof(prog.bookUrl));
    prog.durChapterIndex = _reader.getCurrentChapterIndex();
    prog.durChapterPos = _reader.getCurrentPage();
    _legado.pushProgress(prog);
}

void App::loadWiFiConfig() {
    File f = SD.open("/wifi_config.json");
    if (!f) return;
    DynamicJsonDocument doc(512);
    deserializeJson(doc, f);
    f.close();
    strlcpy(_wifiSsid, doc["ssid"] | "", sizeof(_wifiSsid));
    strlcpy(_wifiPass, doc["pass"] | "", sizeof(_wifiPass));
    _wifiConfigured = _wifiSsid[0] != '\0';
}

void App::saveWiFiConfig() {
    DynamicJsonDocument doc(512);
    doc["ssid"] = _wifiSsid;
    doc["pass"] = _wifiPass;
    File f = SD.open("/wifi_config.json", FILE_WRITE);
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
}

void App::loadLegadoConfig() {
    File f = SD.open("/legado_config.json");
    if (!f) return;
    DynamicJsonDocument doc(512);
    deserializeJson(doc, f);
    f.close();
    strlcpy(_legadoHost, doc["host"] | "", sizeof(_legadoHost));
    _legadoPort = doc["port"] | 80;
    strlcpy(_legadoUser, doc["user"] | "", sizeof(_legadoUser));
    strlcpy(_legadoPass, doc["pass"] | "", sizeof(_legadoPass));
    _legadoConfigured = _legadoHost[0] != '\0';
    if (_legadoConfigured) {
        _legado.config(_legadoHost, _legadoPort, _legadoUser, _legadoPass);
    }
}

void App::saveLegadoConfig() {
    DynamicJsonDocument doc(512);
    doc["host"] = _legadoHost;
    doc["port"] = _legadoPort;
    doc["user"] = _legadoUser;
    doc["pass"] = _legadoPass;
    File f = SD.open("/legado_config.json", FILE_WRITE);
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
}

// ===== PaperS3 竖屏 UI =====

static void prepareShellFrame() {
    auto& display = M5.Display;
    display.waitDisplay();
    display.setEpdMode(epd_mode_t::epd_fastest);
    display.fillScreen(BG_LIGHT);
}

static void commitShellFrame() {
    auto& display = M5.Display;
    display.display(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static void drawBackHeader(const char* title, const char* hint) {
    auto& display = M5.Display;
    display.setTextSize(2);
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    display.setCursor(20, 22);
    display.print("<");
    display.setCursor(64, 22);
    display.print(title);
    display.setTextSize(1);
    display.setTextColor(TEXT_MID, BG_LIGHT);
    display.setCursor(20, 58);
    display.print(hint);
    display.drawLine(20, 82, SCREEN_WIDTH - 20, 82, BORDER);
    display.drawLine(20, 83, SCREEN_WIDTH - 20, 83, BORDER);
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
}

void App::navigateBack() {
    switch (_state) {
        case AppState::SETTINGS_LAYOUT:
        case AppState::SETTINGS_REFRESH:
        case AppState::SETTINGS_FONT:
        case AppState::SETTINGS_WIFI:
        case AppState::SETTINGS_LEGADO:
            _activeTab = 3;
            _state = AppState::TAB_SETTINGS;
            _tabNeedsRender[3] = true;
            break;
        case AppState::WIFI_UPLOAD:
        case AppState::READING_STATS:
            if (_reader.isOpen()) {
                _state = AppState::READER;
                _reader.renderPage();
            } else {
                _activeTab = 2;
                _state = AppState::TAB_TRANSFER;
                _tabNeedsRender[2] = true;
            }
            break;
        case AppState::CHAPTER_LIST:
        case AppState::BOOKMARK_LIST:
        case AppState::GOTO_PAGE:
        case AppState::READER_MENU:
            _state = AppState::READER;
            if (_reader.isOpen()) _reader.renderPage();
            break;
        default:
            if (!isTabState(_state)) {
                _state = AppState::TAB_READING;
                _activeTab = 0;
                _tabNeedsRender[0] = true;
            }
            break;
    }
}

void App::renderTopTabs() {
    auto& display = M5.Display;
    const char* tabs[] = {"阅读", "书架", "传输", "设置"};
    const char* icons[] = {nullptr, nullptr, nullptr, nullptr};
    int16_t tabW = SCREEN_WIDTH / 4;

    for (int i = 0; i < 4; i++) {
        bool active = (i == _activeTab);
        UITheme::drawTabBookmark(i * tabW, 0, tabW, TOP_TAB_H, active, tabs[i], icons[i]);
    }
}

void App::renderBottomNav() {
    // M5PaperS3 无物理按键，底部导航栏留空
    // 如需显示电量/时间，可在此添加
}

void App::switchTab(int tab) {
    if (tab < 0) tab = 0;
    if (tab > 3) tab = 3;
    _activeTab = tab;
    switch (tab) {
        case 0: _state = AppState::TAB_READING; break;
        case 1: _state = AppState::TAB_LIBRARY; break;
        case 2: _state = AppState::TAB_TRANSFER; break;
        case 3: _state = AppState::TAB_SETTINGS; break;
    }
    for (int i = 0; i < 4; i++) _tabNeedsRender[i] = true;
}

bool App::isTabState(AppState state) {
    return state == AppState::TAB_READING || 
           state == AppState::TAB_LIBRARY || 
           state == AppState::TAB_TRANSFER || 
           state == AppState::TAB_SETTINGS;
}

// ===== 📖 读书主页（PaperS3 540x960 竖屏优化）=====
void App::handleTabReading() {
    static bool needsRender = true;
    if (!needsRender && !_tabNeedsRender[0]) return;

    auto& display = M5.Display;
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    prepareShellFrame();
    renderTopTabs();

    const int16_t x = contentLeft();
    const int16_t w = contentWidth();
    int16_t y = contentTop();

    UITheme::drawSectionTitle(x, y, "继续阅读");
    y += 42;

    int recentCount = _recent.getCount();
    if (recentCount > 0) {
        const RecentBook* book = _recent.getBook(0);
        int pct = _recent.getProgressPercent(0);
        UITheme::drawCard(x, y, w, 218, BG_WHITE, BORDER);
        display.setTextSize(2);
        display.setTextColor(TEXT_BLACK, BG_WHITE);
        display.setCursor(x + 18, y + 22);
        display.print(book ? book->name : "最近阅读");
        display.setTextSize(1);
        display.setCursor(x + 18, y + 76);
        display.printf("阅读进度：%d%%", pct);
        display.setCursor(x + 18, y + 108);
        display.print("点击继续阅读");
        display.drawRect(x + 18, y + 154, w - 36, 18, TEXT_BLACK);
        display.fillRect(x + 20, y + 156, max(1, (w - 40) * pct / 100), 14, TEXT_BLACK);
        display.setTextColor(TEXT_BLACK, BG_LIGHT);
    } else {
        UITheme::drawCard(x, y, w, 218, BG_WHITE, BORDER);
        UITheme::drawTextCentered(x, y + 46, w, 80, "暂无阅读记录", 2, TEXT_MID);
        UITheme::drawTextCentered(x, y + 124, w, 40, "到“书架”选择一本书开始阅读", 1, TEXT_MID);
    }
    y += 252;

    UITheme::drawSectionTitle(x, y, "阅读统计");
    y += 42;
    UITheme::drawCard(x, y, w, 122, BG_WHITE, BORDER_LIGHT);
    display.setTextSize(1);
    display.setTextColor(TEXT_BLACK, BG_WHITE);
    display.setCursor(x + 18, y + 18);
    display.printf("今日阅读：%s", _stats.formatTime(_stats.getTodaySeconds()).c_str());
    display.setCursor(x + 18, y + 50);
    display.printf("累计阅读：%s", _stats.formatTime(_stats.getTotalReadingSeconds()).c_str());
    display.setCursor(x + 18, y + 82);
    display.printf("累计翻页：%d 页", _stats.getTotalPagesRead());
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    y += 154;

    if (recentCount > 1) {
        UITheme::drawSectionTitle(x, y, "最近书籍");
        y += 42;
        const int cardH = 78;
        int maxShow = min(4, recentCount - 1);
        for (int i = 0; i < maxShow; i++) {
            const RecentBook* book = _recent.getBook(i + 1);
            int iy = y + i * (cardH + 10);
            UITheme::drawCard(x, iy, w, cardH, BG_WHITE, BORDER_LIGHT);
            display.setTextSize(1);
            display.setTextColor(TEXT_BLACK, BG_WHITE);
            display.setCursor(x + 16, iy + 14);
            display.print(book ? book->name : "书籍");
            display.setTextColor(TEXT_MID, BG_WHITE);
            display.setCursor(x + 16, iy + 44);
            display.printf("进度 %d%%", _recent.getProgressPercent(i + 1));
        }
    }

    commitShellFrame();
    needsRender = false;
    _tabNeedsRender[0] = false;
}

// ===== 📚 书架（2列 x 4行，适配 540 竖屏）=====
void App::handleTabLibrary() {
    static bool needsRender = true;
    if (!needsRender && !_tabNeedsRender[1]) return;

    auto& display = M5.Display;
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    prepareShellFrame();
    renderTopTabs();

    const int totalBooks = _browser.getItemCount();
    const int booksPerPage = 8;
    _libraryTotalPages = (totalBooks + booksPerPage - 1) / booksPerPage;
    if (_libraryTotalPages < 1) _libraryTotalPages = 1;
    if (_libraryPage >= _libraryTotalPages) _libraryPage = _libraryTotalPages - 1;

    int16_t x = contentLeft();
    int16_t y = contentTop();
    UITheme::drawSectionTitle(x, y, "书架");
    display.setTextSize(1);
    display.setTextColor(TEXT_MID, BG_LIGHT);
    display.setCursor(SCREEN_WIDTH - 96, y + 8);
    display.printf("第 %d/%d 页", _libraryPage + 1, _libraryTotalPages);
    y += 42;

    const int cols = 2;
    const int cardW = (contentWidth() - 14) / 2;
    const int cardH = 156;
    const int gapX = 14;
    const int gapY = 16;
    int startIdx = _libraryPage * booksPerPage;

    for (int i = 0; i < booksPerPage; i++) {
        int idx = startIdx + i;
        if (idx >= totalBooks) break;
        const FileItem* item = _browser.getItem(idx);
        if (!item) continue;

        int col = i % cols;
        int row = i / cols;
        int bx = x + col * (cardW + gapX);
        int by = y + row * (cardH + gapY);
        UITheme::drawCard(bx, by, cardW, cardH, BG_WHITE, idx == _librarySelected ? ACCENT : BORDER_LIGHT);

        display.setTextSize(2);
        display.setTextColor(TEXT_BLACK, BG_WHITE);
        display.setCursor(bx + 14, by + 18);
        display.print(item->isDirectory ? "文件夹" : "书籍");
        display.setTextSize(1);
        display.setCursor(bx + 14, by + 66);
        String name = item->name;
        if (name.length() > 18) name = name.substring(0, 16) + "..";
        display.print(name);
        display.setTextColor(TEXT_MID, BG_WHITE);
        display.setCursor(bx + 14, by + 108);
        display.print(item->isDirectory ? "点击打开" : "点击阅读");
        display.setTextColor(TEXT_BLACK, BG_LIGHT);
    }

    if (totalBooks == 0) {
        UITheme::drawTextCentered(0, y + 180, SCREEN_WIDTH, 100, "书架为空", 2, TEXT_MID);
        UITheme::drawTextCentered(0, y + 255, SCREEN_WIDTH, 50, "请将 TXT 放入 /books", 1, TEXT_MID);
    }

    commitShellFrame();
    needsRender = false;
    _tabNeedsRender[1] = false;
}

// ===== 🛜 传输中心 =====
void App::handleTabTransfer() {
    static bool needsRender = true;
    if (!needsRender && !_tabNeedsRender[2]) return;

    auto& display = M5.Display;
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    prepareShellFrame();
    renderTopTabs();

    int16_t x = contentLeft();
    int16_t y = contentTop();
    UITheme::drawSectionTitle(x, y, "传输与工具");
    y += 44;

    struct TransferItem { const char* group; const char* title; const char* desc; bool enabled; };
    TransferItem items[] = {
        {"传书", "WiFi传书", _uploader.isRunning() ? "已开启，可用浏览器上传" : "未开启，点击启动传书服务", _uploader.isRunning()},
        {"传书", "WiFi配置", _wifiConfigured ? _wifiSsid : "未配置，点击查看配置方法", _wifiConfigured},
        {"同步", "Legado同步", _legadoConfigured ? _legadoHost : "未配置，点击查看配置方法", _legadoConfigured},
        {"外设", "蓝牙翻页", _ble.isRunning() ? "已开启" : "未开启", _ble.isRunning()},
        {"工具", "阅读统计", "查看当前书籍和累计统计", true},
        {"工具", "打开书架", "浏览SD卡中的书籍", true},
    };

    const int itemH = 84;
    const int gap = 10;
    const char* lastGroup = "";
    for (int i = 0; i < 6; i++) {
        if (strcmp(lastGroup, items[i].group) != 0) {
            display.setTextSize(1);
            display.setTextColor(TEXT_MID, BG_LIGHT);
            display.setCursor(x, y + 4);
            display.print(items[i].group);
            y += 26;
            lastGroup = items[i].group;
        }
        UITheme::drawCard(x, y, contentWidth(), itemH, BG_WHITE, BORDER_LIGHT);
        display.setTextSize(2);
        display.setTextColor(TEXT_BLACK, BG_WHITE);
        display.setCursor(x + 18, y + 12);
        display.print(items[i].title);
        display.setTextSize(1);
        display.setTextColor(TEXT_MID, BG_WHITE);
        display.setCursor(x + 18, y + 50);
        display.print(items[i].desc);
        UITheme::drawCapsuleSwitch(contentRight() - 78, y + 26, 58, items[i].enabled);
        y += itemH + gap;
    }

    commitShellFrame();
    needsRender = false;
    _tabNeedsRender[2] = false;
}

// ===== ⚙️ 设置页（大字号列表）=====
void App::handleTabSettings() {
    static bool needsRender = true;
    if (!needsRender && !_tabNeedsRender[3]) return;

    auto& display = M5.Display;
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    prepareShellFrame();
    renderTopTabs();

    int16_t x = contentLeft();
    int16_t y = contentTop();
    UITheme::drawSectionTitle(x, y, "设置");
    y += 44;

    struct SettingItem { const char* group; const char* title; const char* value; AppState nextState; };
    RefreshStrategy rs = _reader.getRefreshStrategy();
    SettingItem items[] = {
        {"阅读显示", "排版设置", "字号 / 边距 / 行距", AppState::SETTINGS_LAYOUT},
        {"阅读显示", "刷新策略", rs.getLabel(), AppState::SETTINGS_REFRESH},
        {"阅读显示", "字体切换", _fontNames[0], AppState::SETTINGS_FONT},
        {"连接同步", "WiFi配置", _wifiConfigured ? _wifiSsid : "未配置", AppState::SETTINGS_WIFI},
        {"连接同步", "Legado同步", _legadoConfigured ? _legadoHost : "未配置", AppState::SETTINGS_LEGADO},
        {"系统", "休眠时间", String(String(_sleepTimeoutMin) + " 分钟").c_str(), AppState::TAB_SETTINGS},
        {"系统", "关于", "Vink-PaperS3 v1.2.8", AppState::TAB_SETTINGS},
    };

    const int numItems = 7;
    const int itemH = 76;
    const int gap = 10;
    int scrollMax = max(0, (numItems * (itemH + gap) + 90) - contentHeight() + 20);
    if (_settingsScroll > scrollMax) _settingsScroll = scrollMax;
    if (_settingsScroll < 0) _settingsScroll = 0;

    const char* lastGroup = "";
    int drawY = y - _settingsScroll;
    for (int i = 0; i < numItems; i++) {
        if (strcmp(lastGroup, items[i].group) != 0) {
            if (drawY >= contentTop() - 24 && drawY <= contentBottom()) {
                display.setTextSize(1);
                display.setTextColor(TEXT_MID, BG_LIGHT);
                display.setCursor(x, drawY + 4);
                display.print(items[i].group);
            }
            drawY += 26;
            lastGroup = items[i].group;
        }
        if (drawY + itemH >= contentTop() && drawY <= contentBottom()) {
            UITheme::drawCard(x, drawY, contentWidth(), itemH, BG_WHITE, BORDER_LIGHT);
            display.setTextSize(2);
            display.setTextColor(TEXT_BLACK, BG_WHITE);
            display.setCursor(x + 18, drawY + 10);
            display.print(items[i].title);
            display.setTextSize(1);
            display.setTextColor(TEXT_MID, BG_WHITE);
            display.setCursor(x + 18, drawY + 48);
            display.print(items[i].value);
            display.setTextSize(2);
            display.setTextColor(TEXT_BLACK, BG_WHITE);
            display.setCursor(contentRight() - 36, drawY + 22);
            display.print(">");
            display.setTextColor(TEXT_BLACK, BG_LIGHT);
        }
        drawY += itemH + gap;
    }

    commitShellFrame();
    needsRender = false;
    _tabNeedsRender[3] = false;
}

// ===== 阅读弹出菜单（半透明遮罩）=====
void App::handleReaderMenu() {
    auto& display = M5.Display;
    
    // 如果菜单已经显示过，不需要重绘（底层阅读页还在）
    // 这里只做一次性绘制
    
    // 半透明遮罩（墨水屏模拟：用浅灰填充区域）
    display.fillRect(100, 60, SCREEN_WIDTH - 200, SCREEN_HEIGHT - 120, BG_LIGHT);
    UITheme::drawCard(100, 60, SCREEN_WIDTH - 200, SCREEN_HEIGHT - 120, BG_WHITE, BORDER);
    
    display.setTextSize(2);
    display.setTextColor(TEXT_BLACK, BG_WHITE);
    display.setCursor(380, 75);
    display.println("菜单");
    
    const char* items[] = {
        "📑 章节目录",
        "🔖 添加书签",
        "📌 我的书签",
        "📄 页码跳转",
        "📝 排版设置",
        "🔤 字体切换",
        "🔄 残影控制",
        "📶 WiFi传书",
        "📊 阅读统计",
        "📡 蓝牙翻页",
        "📚 返回书架",
        "⚡ 关闭设备"
    };
    int numItems = 12;
    
    display.setTextSize(1);
    for (int i = 0; i < numItems; i++) {
        int y = 110 + i * 32;
        if (i == _menuIndex) {
            display.fillRect(120, y - 2, SCREEN_WIDTH - 240, 28, BG_MID);
            display.setTextColor(TEXT_BLACK, BG_MID);
        } else {
            display.setTextColor(TEXT_DARK, BG_WHITE);
        }
        display.setCursor(140, y + 4);
        display.print(items[i]);
    }
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    
    display.display();
}

// ===== 排版设置（卡片式 + 滑块）=====
void App::handleSettingsLayout() {
    auto& display = M5.Display;
    prepareShellFrame();
    drawBackHeader("排版设置", "点 < / 右滑 / 上滑返回");

    LayoutConfig layout = _reader.getLayoutConfig();
    struct Row { const char* name; int minVal; int maxVal; int value; const char* unit; };
    Row rows[] = {
        {"字号", 12, 48, layout.fontSize, "px"},
        {"行距", 50, 200, layout.lineSpacing, "%"},
        {"左右边距", 0, 120, layout.marginLeft, "px"},
        {"顶部边距", 0, 100, layout.marginTop, "px"},
        {"首行缩进", 0, 4, layout.indentFirstLine, "字"},
    };

    int y = 108;
    for (int i = 0; i < 5; i++) {
        UITheme::drawCard(contentLeft(), y, contentWidth(), 112, BG_WHITE, i == _layoutEditorIndex ? ACCENT : BORDER_LIGHT);
        display.setTextSize(2);
        display.setTextColor(TEXT_BLACK, BG_WHITE);
        display.setCursor(contentLeft() + 18, y + 14);
        if (i == _layoutEditorIndex) display.print("* ");
        display.print(rows[i].name);
        UITheme::drawSlider(contentLeft() + 18, y + 64, contentWidth() - 130, rows[i].minVal, rows[i].maxVal, rows[i].value, rows[i].unit);
        y += 126;
    }
    UITheme::drawTextCentered(0, SCREEN_HEIGHT - 54, SCREEN_WIDTH, 34, "点击切换项目，左右滑动调整", 1, TEXT_MID);
    commitShellFrame();
}

// ===== 残影控制（胶囊开关式）=====
void App::handleSettingsRefresh() {
    auto& display = M5.Display;
    prepareShellFrame();
    drawBackHeader("刷新策略", "点击档位切换，点 < 返回");

    RefreshStrategy strategy = _reader.getRefreshStrategy();
    const char* labels[] = {"极速", "均衡", "清晰"};
    const char* descs[] = {
        "局部快刷为主，每20页全刷一次",
        "局部快刷为主，每10页全刷一次",
        "文字更清晰，每5页全刷一次"
    };
    int selected = (int)strategy.frequency;

    int y = 116;
    for (int i = 0; i < 3; i++) {
        UITheme::drawCard(contentLeft(), y, contentWidth(), 128, BG_WHITE, i == selected ? ACCENT : BORDER_LIGHT);
        display.setTextSize(2);
        display.setTextColor(TEXT_BLACK, BG_WHITE);
        display.setCursor(contentLeft() + 18, y + 18);
        display.print(labels[i]);
        display.setTextSize(1);
        display.setTextColor(TEXT_MID, BG_WHITE);
        display.setCursor(contentLeft() + 18, y + 64);
        display.print(descs[i]);
        UITheme::drawCapsuleSwitch(contentRight() - 78, y + 44, 58, i == selected);
        y += 148;
    }
    commitShellFrame();
}

// ===== 字体切换（列表式）=====
void App::handleSettingsFont() {
    auto& display = M5.Display;
    prepareShellFrame();
    drawBackHeader("字体切换", "点击字体切换，点 < 返回");

    if (_fontCount <= 0) {
        UITheme::drawTextCentered(0, 260, SCREEN_WIDTH, 80, "未找到字体", 2, TEXT_MID);
        UITheme::drawTextCentered(0, 330, SCREEN_WIDTH, 50, "请将 .fnt 放入 /fonts", 1, TEXT_MID);
        commitShellFrame();
        return;
    }

    int y = 112;
    for (int i = 0; i < _fontCount && i < 9; i++) {
        bool isCurrent = (strcmp(_font.getCurrentFontPath(), _fontPaths[i]) == 0);
        UITheme::drawCard(contentLeft(), y, contentWidth(), 78, BG_WHITE, isCurrent ? ACCENT : BORDER_LIGHT);
        display.setTextSize(2);
        display.setTextColor(TEXT_BLACK, BG_WHITE);
        display.setCursor(contentLeft() + 18, y + 22);
        if (isCurrent) display.print("* ");
        display.print(_fontNames[i]);
        y += 92;
    }
    commitShellFrame();
}

// ===== WiFi配置 =====
void App::handleSettingsWiFi() {
    handleWiFiConfig();
}

// ===== Legado同步 =====
void App::handleSettingsLegado() {
    handleLegadoSync();
}

// ===== 章节目录（改进版）=====
void App::handleChapterList() {
    auto& display = M5.Display;
    display.clear();
    display.fillScreen(BG_LIGHT);
    
    UITheme::drawSectionTitle(20, 15, "章节目录");
    
    int chapterCount = _reader.getChapterCount();
    if (chapterCount <= 0) {
        UITheme::drawTextCentered(0, 200, SCREEN_WIDTH, 100, 
            "未检测到章节", 2, TEXT_MID);
        UITheme::drawTextCentered(0, 280, SCREEN_WIDTH, 50, 
            "长按菜单可自动识别章节", 1, TEXT_MID);
        display.display();
        return;
    }
    
    const ChapterInfo* chapters = _reader.getChapterList();
    int currentChapter = _reader.getCurrentChapterIndex();
    
    int itemsPerPage = 12;
    int totalPages = (chapterCount + itemsPerPage - 1) / itemsPerPage;
    int currentPage = _chapterMenuScroll / itemsPerPage;
    int startIdx = currentPage * itemsPerPage;
    int endIdx = min(startIdx + itemsPerPage, chapterCount);
    
    int itemH = 36;
    int cy = 55;
    for (int i = startIdx; i < endIdx; i++) {
        int y = cy + (i - startIdx) * (itemH + 4);
        UITheme::drawCard(10, y, SCREEN_WIDTH - 20, itemH, BG_WHITE, BORDER_LIGHT);
        
        if (i == _chapterMenuIndex) {
            display.fillRoundRect(10, y, SCREEN_WIDTH - 20, itemH, UITheme::CARD_RADIUS, BG_MID);
        }
        if (i == currentChapter) {
            display.setTextColor(ACCENT, i == _chapterMenuIndex ? BG_MID : BG_WHITE);
        } else {
            display.setTextColor(TEXT_DARK, i == _chapterMenuIndex ? BG_MID : BG_WHITE);
        }
        
        display.setTextSize(1);
        display.setCursor(25, y + 10);
        String title = chapters[i].title;
        if (title.length() > 35) title = title.substring(0, 32) + "...";
        display.printf("%d. %s", i + 1, title.c_str());
    }
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    
    display.setTextSize(1);
    display.setTextColor(TEXT_MID, BG_LIGHT);
    display.setCursor(400, SCREEN_HEIGHT - 35);
    display.printf("%d/%d 页", currentPage + 1, totalPages);
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    
    renderBottomNav();
    display.display();
}

// ===== 书签列表（改进版）=====
void App::handleBookmarkList() {
    auto& display = M5.Display;
    display.clear();
    display.fillScreen(BG_LIGHT);
    
    UITheme::drawSectionTitle(20, 15, "我的书签");
    
    int bmCount = _reader.getBookmarkCount();
    if (bmCount <= 0) {
        UITheme::drawTextCentered(0, 200, SCREEN_WIDTH, 100, 
            "暂无书签", 2, TEXT_MID);
        UITheme::drawTextCentered(0, 280, SCREEN_WIDTH, 50, 
            "阅读时点击菜单 → 添加书签", 1, TEXT_MID);
        display.display();
        return;
    }
    
    const Bookmark* bookmarks = _reader.getBookmarks();
    int itemsPerPage = 12;
    int totalPages = (bmCount + itemsPerPage - 1) / itemsPerPage;
    int currentPage = _bookmarkMenuScroll / itemsPerPage;
    int startIdx = currentPage * itemsPerPage;
    int endIdx = min(startIdx + itemsPerPage, bmCount);
    
    int itemH = 36;
    int cy = 55;
    for (int i = startIdx; i < endIdx; i++) {
        int y = cy + (i - startIdx) * (itemH + 4);
        UITheme::drawCard(10, y, SCREEN_WIDTH - 20, itemH, BG_WHITE, BORDER_LIGHT);
        
        if (i == _bookmarkMenuIndex) {
            display.fillRoundRect(10, y, SCREEN_WIDTH - 20, itemH, UITheme::CARD_RADIUS, BG_MID);
        }
        
        display.setTextSize(1);
        display.setTextColor(TEXT_DARK, i == _bookmarkMenuIndex ? BG_MID : BG_WHITE);
        display.setCursor(25, y + 10);
        String name = bookmarks[i].name;
        if (name.length() > 30) name = name.substring(0, 27) + "...";
        display.printf("%d. %s (P%d)", i + 1, name.c_str(), bookmarks[i].pageNum + 1);
    }
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    
    display.setTextSize(1);
    display.setTextColor(TEXT_MID, BG_LIGHT);
    display.setCursor(350, SCREEN_HEIGHT - 35);
    display.printf("%d 个书签 | %d/%d", bmCount, currentPage + 1, totalPages);
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    
    renderBottomNav();
    display.display();
}

