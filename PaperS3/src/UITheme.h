#pragma once
#include "Config.h"
#include <M5Unified.h>

// ===== Crosslink 风格 UI 主题 =====
// 配色：浅灰白纸质感，墨水屏原生风格

namespace UITheme {
    // PaperS3's 4bpp gray fill is rendered as a visible halftone pattern.
    // Keep the shell UI pure black/white until the full ReadPaper canvas path
    // is adopted; otherwise large gray backgrounds look like screen noise.
    constexpr uint8_t BG_WHITE     = 15;  // 纯白
    constexpr uint8_t BG_LIGHT     = 15;  // 主背景保持纯白
    constexpr uint8_t BG_MID       = 15;  // 卡片背景保持纯白
    constexpr uint8_t BG_DARK      = 0;   // 深色区域
    
    // 文字色
    constexpr uint8_t TEXT_BLACK   = 0;   // 纯黑（标题）
    constexpr uint8_t TEXT_DARK    = 0;   // 正文
    constexpr uint8_t TEXT_MID     = 0;   // 次要文字先保持高对比
    constexpr uint8_t TEXT_LIGHT   = 8;   // 禁用/提示
    
    // 强调色
    constexpr uint8_t ACCENT       = 0;   // 选中态黑色
    constexpr uint8_t ACCENT_LIGHT = 0;
    
    // 边框/分隔线
    constexpr uint8_t BORDER       = 0;
    constexpr uint8_t BORDER_LIGHT = 0;
    
    // 尺寸常量
    constexpr int16_t TOP_TAB_H    = 50;   // 顶部标签高度
    constexpr int16_t BOTTOM_NAV_H = 0;    // M5PaperS3 无物理按键，无底部导航栏
    constexpr int16_t CARD_RADIUS  = 6;    // 卡片圆角
    constexpr int16_t ITEM_H       = 44;   // 列表项高度
    
    // 安全区域（减去顶部标签和底部导航）
    inline int16_t contentTop()    { return TOP_TAB_H + 5; }
    inline int16_t contentBottom() { return SCREEN_HEIGHT - BOTTOM_NAV_H - 5; }
    inline int16_t contentHeight() { return contentBottom() - contentTop(); }
    inline int16_t contentWidth()  { return SCREEN_WIDTH - 20; }
    inline int16_t contentLeft()   { return 10; }
    inline int16_t contentRight()  { return SCREEN_WIDTH - 10; }
    
    // 绘制工具函数
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color);
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color);
    void drawCard(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t bgColor = BG_LIGHT, uint8_t borderColor = BORDER_LIGHT);
    void drawTabBookmark(int16_t x, int16_t y, int16_t w, int16_t h, bool active, const char* label, const char* icon);
    void drawCapsuleSwitch(int16_t x, int16_t y, int16_t w, bool on);
    void drawSlider(int16_t x, int16_t y, int16_t w, int16_t minVal, int16_t maxVal, int16_t current, const char* unit);
    void drawBottomNavItem(int16_t x, int16_t y, int16_t w, bool active, const char* icon, const char* label);
    void drawBookCover(int16_t x, int16_t y, int16_t w, int16_t h, const char* title, const char* author, int progressPercent);
    void drawSectionTitle(int16_t x, int16_t y, const char* title);
    void drawSeparator(int16_t x, int16_t y, int16_t w);
    void drawIconButton(int16_t x, int16_t y, int16_t size, const char* icon, uint8_t bgColor);
    
    // 辅助函数
    int16_t textWidth(const char* text, uint8_t textSize = 1);
    void drawTextCentered(int16_t x, int16_t y, int16_t w, int16_t h, const char* text, uint8_t textSize, uint8_t color);
    void drawTextRight(int16_t x, int16_t y, int16_t w, const char* text, uint8_t textSize, uint8_t color);
}
