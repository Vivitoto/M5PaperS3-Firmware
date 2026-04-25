#include "App.h"
#include <M5Unified.h>

App::App() : _state(AppState::INIT), _reader(_font), _touching(false),
             _menuIndex(0), _layoutEditorIndex(0), _chapterMenuIndex(0), _chapterMenuScroll(0),
             _bookmarkMenuIndex(0), _bookmarkMenuScroll(0),
             _fontMenuIndex(0), _fontMenuScroll(0),
             _gotoPageCursor(0),
             _lastActivityTime(0), _sleepPending(false),
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
}

bool App::init() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[M5PaperS3-Ebook] Starting...");
    
    auto cfg = M5.config();
    cfg.external_display.module_display = true;
    M5.begin(cfg);
    
    if (!initDisplay()) {
        Serial.println("[App] Display init failed");
        return false;
    }
    
    if (!initSD()) {
        showMessage("SD Card Error!", 5000);
        Serial.println("[App] SD init failed");
        return false;
    }
    
    if (!initFont()) {
        showMessage("Font Error!", 5000);
        Serial.println("[App] Font init failed");
        return false;
    }
    
    if (!SD.exists(BOOKS_DIR)) SD.mkdir(BOOKS_DIR);
    if (!SD.exists(PROGRESS_DIR)) SD.mkdir(PROGRESS_DIR);
    
    // 扫描可用字体
    scanFonts();
    
    _browser.scan(BOOKS_DIR);
    _state = AppState::FILE_BROWSER;
    
    loadGlobalSettings();
    _lastActivityTime = millis();
    _sleepPending = false;
    
    _stats.load();
    _recent.load();
    loadWiFiConfig();
    loadLegadoConfig();
    
    Serial.println("[App] Init complete");
    return true;
}

bool App::initDisplay() {
    auto& display = M5.Display;
    if (display.width() == 0 || display.height() == 0) {
        Serial.println("[Display] Display not detected");
        return false;
    }
    Serial.printf("[Display] %dx%d\n", display.width(), display.height());
    display.clear();
    display.setTextSize(1);
    display.setTextColor(0, 15);
    showMessage("正在启动...", 1000);
    return true;
}

bool App::initSD() {
    Serial.println("[SD] Initializing...");
    SPI.begin(14, 39, 38, 4);
    if (!SD.begin(4, SPI, 4000000)) {
        Serial.println("[SD] SD.begin(4) failed, trying default...");
        SPI.end();
        if (!SD.begin()) {
            Serial.println("[SD] Default config failed");
            return false;
        }
    }
    Serial.println("[SD] OK");
    return true;
}

bool App::initFont() {
    if (SD.exists(FONT_FILE_24)) {
        if (_font.loadFont(FONT_FILE_24)) {
            if (strncmp(_font.getCurrentFontPath(), "builtin://", 10) == 0) {
                showMessage("使用内置字体", 1000);
            } else {
                Serial.println("[Font] Loaded 24px");
            }
            return true;
        }
    }
    if (SD.exists(FONT_FILE_16)) {
        if (_font.loadFont(FONT_FILE_16)) {
            if (strncmp(_font.getCurrentFontPath(), "builtin://", 10) == 0) {
                showMessage("使用内置字体", 1000);
            } else {
                Serial.println("[Font] Loaded 16px");
            }
            return true;
        }
    }
    Serial.println("[Font] No SD font file found, trying builtin");
    if (_font.loadBuiltinFont()) {
        showMessage("使用内置字体", 1000);
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
        processTouch();
        checkAutoSleep();
        checkBleCommands();
        _uploader.handleClient();
        
        switch (_state) {
            case AppState::INIT:
                handleInit();
                break;
            case AppState::FILE_BROWSER:
                handleFileBrowser();
                break;
            case AppState::READER:
                handleReader();
                break;
            case AppState::MENU:
                handleMenu();
                break;
            case AppState::MENU_LAYOUT:
                handleLayoutMenu();
                break;
            case AppState::MENU_CHAPTER:
                handleChapterMenu();
                break;
            case AppState::MENU_REFRESH:
                handleRefreshMenu();
                break;
            case AppState::MENU_BOOKMARK:
                handleBookmarkMenu();
                break;
            case AppState::MENU_FONT:
                handleFontMenu();
                break;
            case AppState::MENU_GOTO_PAGE:
                handleGotoPage();
                break;
            case AppState::WIFI_UPLOAD:
                handleWiFiUpload();
                break;
            case AppState::READING_STATS:
                handleReadingStats();
                break;
            default:
                break;
        }
        delay(50);
    }
}

void App::handleInit() {
    _browser.scan(BOOKS_DIR);
    _state = AppState::FILE_BROWSER;
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
        "低 - 每10页全刷（最流畅）",
        "中 - 每5页全刷（平衡）",
        "高 - 每3页全刷（最清晰）"
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
    display.println("低=残影多但翻页快 | 高=清晰但略慢");
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
        _state = AppState::MENU;
        handleMenu();
    }
}

void App::executeMenuItem(int index) {
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
            _state = AppState::FILE_BROWSER;
            break;
            
        case 14: // 关闭设备
            showMessage("正在关机...", 1000);
            M5.Power.powerOff();
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
    uint8_t color;
    if (bat.level > 50) color = 0;
    else if (bat.level > 20) color = 8;
    else color = 4;
    
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

// ===== 休眠管理 =====

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
        display.println("M5PaperS3 阅读器");
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
                _state = AppState::FILE_BROWSER;
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
    _state = AppState::FILE_BROWSER;
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
    } else if (_state == AppState::FILE_BROWSER) {
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
    display.clear();
    
    display.setTextSize(2);
    display.setCursor(340, 20);
    display.println("阅读统计");
    
    display.setTextSize(1);
    int y = 80;
    
    // 当前书
    if (_reader.isOpen()) {
        BookStats book = _stats.getCurrentBookStats();
        display.setCursor(150, y);
        display.printf("当前书籍: %s", _reader.getBookPath());
        y += 35;
        display.setCursor(150, y);
        display.printf("累计翻页: %d 页", book.totalPagesRead);
        y += 35;
        display.setCursor(150, y);
        display.printf("累计阅读: %s", _stats.formatTime(book.totalSeconds).c_str());
        y += 35;
        display.setCursor(150, y);
        display.printf("打开次数: %d", book.readCount);
        y += 50;
    }
    
    // 总统计
    display.setCursor(150, y);
    display.println("--- 全局统计 ---");
    y += 35;
    display.setCursor(150, y);
    display.printf("总阅读时长: %s", _stats.formatTime(_stats.getTotalReadingSeconds()).c_str());
    y += 35;
    display.setCursor(150, y);
    display.printf("总翻页数: %d", _stats.getTotalPagesRead());
    y += 35;
    display.setCursor(150, y);
    display.printf("今日阅读: %s", _stats.formatTime(_stats.getTodaySeconds()).c_str());
    y += 35;
    display.setCursor(150, y);
    display.printf("今日翻页: %d", _stats.getTodayPages());
    y += 50;
    
    display.setCursor(250, 450);
    display.println("点击返回阅读");
    
    display.display();
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
    display.clear();
    display.setTextSize(2);
    display.setCursor(320, 20);
    display.println("Legado同步");
    display.setTextSize(1);
    if (!_legadoConfigured) {
        display.setCursor(250, 200);
        display.println("未配置WebDav服务器");
        display.setCursor(200, 240);
        display.println("请在系统设置中配置");
    } else {
        display.setCursor(300, 100);
        display.println("正在同步...");
        syncLegadoProgress();
        display.setCursor(300, 150);
        display.println("同步完成");
    }
    display.setCursor(300, 400);
    display.println("点击返回阅读");
    display.display();
}

void App::handleWiFiConfig() {
    // simplified - just show status
    auto& display = M5.Display;
    display.clear();
    display.setTextSize(2);
    display.setCursor(320, 20);
    display.println("WiFi配置");
    display.setTextSize(1);
    display.setCursor(300, 100);
    display.println("请在SD卡根目录放置");
    display.setCursor(280, 140);
    display.println("/wifi_config.json");
    display.setCursor(300, 200);
    display.println("格式: {");
    display.setCursor(320, 240);
    display.println("\"ssid\":\"你的WiFi\",");
    display.setCursor(320, 280);
    display.println("\"pass\":\"密码\"");
    display.setCursor(300, 320);
    display.println("}");
    display.display();
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
