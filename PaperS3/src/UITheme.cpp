#include "UITheme.h"
#include <M5Unified.h>

static LovyanGFX* gThemeDrawTarget = nullptr;

void UITheme::setDrawTarget(LovyanGFX* target) {
    gThemeDrawTarget = target;
}

LovyanGFX& UITheme::drawTarget() {
    return gThemeDrawTarget ? *gThemeDrawTarget : M5.Display;
}


void UITheme::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    auto& display = UITheme::drawTarget();
    display.drawRoundRect(x, y, w, h, r, color);
}

void UITheme::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    auto& display = UITheme::drawTarget();
    display.fillRoundRect(x, y, w, h, r, color);
}

void UITheme::drawCard(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t bgColor, uint16_t borderColor) {
    auto& display = UITheme::drawTarget();
    // 填充背景
    fillRoundRect(x, y, w, h, CARD_RADIUS, bgColor);
    // PaperS3 上 1px 细线太虚，卡片边框统一加粗到 2px。
    drawRoundRect(x, y, w, h, CARD_RADIUS, borderColor);
    if (w > 4 && h > 4) {
        const int16_t innerRadius = CARD_RADIUS > 0 ? CARD_RADIUS - 1 : 0;
        drawRoundRect(x + 1, y + 1, w - 2, h - 2, innerRadius, borderColor);
    }
}

void UITheme::drawTabBookmark(int16_t x, int16_t y, int16_t w, int16_t h, bool active, const char* label, const char* icon) {
    auto& display = UITheme::drawTarget();
    
    if (active) {
        // 激活态：黑底白字，顶部区域足够高，适合手指点击。
        fillRoundRect(x + 5, y + 8, w - 10, h - 16, 8, ACCENT);
        display.setTextColor(BG_WHITE, ACCENT);
    } else {
        display.drawRoundRect(x + 5, y + 8, w - 10, h - 16, 8, BORDER_LIGHT);
        display.setTextColor(TEXT_MID, BG_LIGHT);
    }
    
    display.setTextSize(2);
    if (icon) {
        display.setCursor(x + 16, y + 26);
        display.print(icon);
    }
    int16_t tw = textWidth(label, 2);
    display.setCursor(x + (w - tw) / 2, y + 27);
    display.print(label);
    
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
}

void UITheme::drawCapsuleSwitch(int16_t x, int16_t y, int16_t w, bool on) {
    auto& display = UITheme::drawTarget();
    int16_t h = 28;
    int16_t r = h / 2;
    
    // 背景轨道
    fillRoundRect(x, y, w, h, r, on ? ACCENT : BG_MID);
    // 滑块
    int16_t knobX = on ? x + w - h + 2 : x + 2;
    fillRoundRect(knobX, y + 2, h - 4, h - 4, r - 2, BG_WHITE);
    // 文字
    display.setTextSize(1);
    if (on) {
        display.setTextColor(BG_WHITE, ACCENT);
        display.setCursor(x + 8, y + 8);
        display.print("开");
    } else {
        display.setTextColor(TEXT_LIGHT, BG_MID);
        display.setCursor(x + w - 24, y + 8);
        display.print("关");
    }
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
}

void UITheme::drawSlider(int16_t x, int16_t y, int16_t w, int16_t minVal, int16_t maxVal, int16_t current, const char* unit) {
    auto& display = UITheme::drawTarget();
    int16_t trackH = 6;
    int16_t knobR = 10;
    
    // 轨道背景
    display.fillRoundRect(x, y + (knobR - trackH/2), w, trackH, trackH/2, BG_MID);
    // 已选部分
    int16_t filledW = (w * (current - minVal)) / (maxVal - minVal);
    if (filledW > 0) {
        display.fillRoundRect(x, y + (knobR - trackH/2), filledW, trackH, trackH/2, ACCENT);
    }
    // 滑块
    display.fillCircle(x + filledW, y + knobR, knobR, BG_WHITE);
    display.drawCircle(x + filledW, y + knobR, knobR, ACCENT);
    // 数值
    display.setTextSize(1);
    display.setTextColor(TEXT_DARK, BG_LIGHT);
    display.setCursor(x + w + 10, y + knobR - 4);
    display.printf("%d%s", current, unit);
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
}

void UITheme::drawBottomNavItem(int16_t x, int16_t y, int16_t w, bool active, const char* icon, const char* label) {
    auto& display = UITheme::drawTarget();
    
    if (active) {
        display.setTextColor(ACCENT, BG_LIGHT);
    } else {
        display.setTextColor(TEXT_MID, BG_LIGHT);
    }
    
    display.setTextSize(1);
    // 图标
    if (icon) {
        display.setCursor(x + (w - 16) / 2, y + 4);
        display.print(icon);
    }
    // 标签
    int16_t tw = textWidth(label, 1);
    display.setCursor(x + (w - tw) / 2, y + 22);
    display.print(label);
    
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
}

void UITheme::drawBookCover(int16_t x, int16_t y, int16_t w, int16_t h, const char* title, const char* author, int progressPercent) {
    auto& display = UITheme::drawTarget();
    
    // 封面背景（模拟书本质感）
    fillRoundRect(x, y, w, h, 4, BG_MID);
    drawRoundRect(x, y, w, h, 4, BORDER);
    
    // 书名（居中，截断）
    display.setTextSize(1);
    display.setTextColor(TEXT_BLACK, BG_MID);
    
    char displayTitle[32];
    strlcpy(displayTitle, title, sizeof(displayTitle));
    if (strlen(displayTitle) > 12) {
        displayTitle[11] = '.';
        displayTitle[12] = '.';
        displayTitle[13] = '\0';
    }
    
    int16_t tw = textWidth(displayTitle, 1);
    display.setCursor(x + (w - tw) / 2, y + h / 2 - 8);
    display.print(displayTitle);
    
    // 作者
    if (author && strlen(author) > 0) {
        char displayAuthor[20];
        strlcpy(displayAuthor, author, sizeof(displayAuthor));
        if (strlen(displayAuthor) > 8) {
            displayAuthor[7] = '.';
            displayAuthor[8] = '.';
            displayAuthor[9] = '\0';
        }
        tw = textWidth(displayAuthor, 1);
        display.setTextColor(TEXT_MID, BG_MID);
        display.setCursor(x + (w - tw) / 2, y + h / 2 + 8);
        display.print(displayAuthor);
    }
    
    // 进度条（底部）
    if (progressPercent > 0) {
        int16_t barY = y + h - 12;
        int16_t barW = w - 16;
        int16_t filled = (barW * progressPercent) / 100;
        display.fillRoundRect(x + 8, barY, barW, 6, 3, BG_LIGHT);
        if (filled > 0) {
            display.fillRoundRect(x + 8, barY, filled, 6, 3, ACCENT);
        }
    }
    
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
}

void UITheme::drawSectionTitle(int16_t x, int16_t y, const char* title) {
    auto& display = UITheme::drawTarget();
    display.setTextSize(2);
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
    display.setCursor(x, y);
    display.print(title);
    
    int16_t tw = textWidth(title, 2);
    display.drawLine(x, y + 24, x + tw, y + 24, ACCENT);
    display.drawLine(x, y + 25, x + tw, y + 25, ACCENT);
}

void UITheme::drawSeparator(int16_t x, int16_t y, int16_t w) {
    auto& display = UITheme::drawTarget();
    display.drawLine(x, y, x + w, y, BORDER_LIGHT);
}

void UITheme::drawIconButton(int16_t x, int16_t y, int16_t size, const char* icon, uint16_t bgColor) {
    auto& display = UITheme::drawTarget();
    fillRoundRect(x, y, size, size, size/4, bgColor);
    display.setTextSize(1);
    display.setTextColor(TEXT_BLACK, bgColor);
    display.setCursor(x + (size - 16) / 2, y + (size - 8) / 2);
    display.print(icon);
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
}

int16_t UITheme::textWidth(const char* text, uint8_t textSize) {
    if (!text) return 0;
    int16_t units = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(text);
    while (*p) {
        if ((*p & 0x80) == 0) {
            units += 1;
            p += 1;
        } else {
            // UTF-8 Chinese glyph: estimate as two ASCII cells and skip sequence.
            units += 2;
            if ((*p & 0xE0) == 0xC0) p += 2;
            else if ((*p & 0xF0) == 0xE0) p += 3;
            else if ((*p & 0xF8) == 0xF0) p += 4;
            else p += 1;
        }
    }
    return units * 6 * textSize;
}

void UITheme::drawTextCentered(int16_t x, int16_t y, int16_t w, int16_t h, const char* text, uint8_t textSize, uint16_t color) {
    auto& display = UITheme::drawTarget();
    int16_t tw = textWidth(text, textSize);
    int16_t th = 8 * textSize;
    display.setTextColor(color, BG_LIGHT);
    display.setTextSize(textSize);
    display.setCursor(x + (w - tw) / 2, y + (h - th) / 2);
    display.print(text);
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
}

void UITheme::drawTextRight(int16_t x, int16_t y, int16_t w, const char* text, uint8_t textSize, uint16_t color) {
    auto& display = UITheme::drawTarget();
    int16_t tw = textWidth(text, textSize);
    display.setTextColor(color, BG_LIGHT);
    display.setTextSize(textSize);
    display.setCursor(x + w - tw, y);
    display.print(text);
    display.setTextColor(TEXT_BLACK, BG_LIGHT);
}
