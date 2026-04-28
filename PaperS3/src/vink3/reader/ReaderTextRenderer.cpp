#include "ReaderTextRenderer.h"
#include "../../Config.h"
#include "../ReadPaper176.h"

namespace vink3 {

ReaderTextRenderer g_readerText;

bool ReaderTextRenderer::begin(M5Canvas* canvas) {
    canvas_ = canvas;
    return canvas_ && loadDefaultFont();
}

bool ReaderTextRenderer::loadDefaultFont() {
    // Reader body font is intentionally separate from the UI subset font.
    // Prefer bundled larger CJK font, then allow future SD/ReadPaper font cache.
    if (font_.loadBundledFont(FONT_FILE_24)) {
        Serial.println("[vink3][reader] bundled 24px reader font loaded");
        return true;
    }
    if (font_.loadBundledFont(FONT_FILE_20)) {
        Serial.println("[vink3][reader] bundled 20px reader font loaded");
        return true;
    }
    if (font_.loadBundledFont(FONT_FILE_16)) {
        Serial.println("[vink3][reader] bundled 16px reader font loaded");
        return true;
    }
    Serial.println("[vink3][reader] reader font load failed");
    return false;
}

bool ReaderTextRenderer::loadFont(const char* path) {
    if (!path || !path[0]) return loadDefaultFont();
    return font_.loadFont(path);
}

bool ReaderTextRenderer::ready() const {
    return canvas_ && font_.isLoaded();
}

uint16_t ReaderTextRenderer::fontSize() const {
    return font_.isLoaded() ? font_.getFontSize() : 24;
}

uint32_t ReaderTextRenderer::decodeUtf8(const uint8_t* buf, size_t& pos, size_t len) {
    if (pos >= len) return 0;
    uint8_t c = buf[pos];
    if ((c & 0x80) == 0) { pos++; return c; }
    if ((c & 0xE0) == 0xC0 && pos + 1 < len) {
        uint32_t ch = ((c & 0x1F) << 6) | (buf[pos + 1] & 0x3F);
        pos += 2; return ch;
    }
    if ((c & 0xF0) == 0xE0 && pos + 2 < len) {
        uint32_t ch = ((c & 0x0F) << 12) | ((buf[pos + 1] & 0x3F) << 6) | (buf[pos + 2] & 0x3F);
        pos += 3; return ch;
    }
    if ((c & 0xF8) == 0xF0 && pos + 3 < len) {
        uint32_t ch = ((c & 0x07) << 18) | ((buf[pos + 1] & 0x3F) << 12) | ((buf[pos + 2] & 0x3F) << 6) | (buf[pos + 3] & 0x3F);
        pos += 4; return ch;
    }
    pos++;
    return c;
}

uint8_t ReaderTextRenderer::charAdvance(uint32_t unicode) const {
    if (!font_.isLoaded()) return unicode < 128 ? 8 : 24;
    uint8_t adv = const_cast<FontManager&>(font_).getCharAdvance(unicode);
    return adv > 0 ? adv : (unicode < 128 ? 8 : fontSize());
}

int16_t ReaderTextRenderer::textWidth(const char* text) const {
    if (!text) return 0;
    int16_t w = 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text);
    size_t pos = 0;
    const size_t len = strlen(text);
    while (pos < len) {
        uint32_t ch = decodeUtf8(bytes, pos, len);
        if (ch == '\n') break;
        w += charAdvance(ch);
    }
    return w;
}

void ReaderTextRenderer::drawGlyph(uint32_t unicode, int16_t x, int16_t y, uint16_t color) {
    if (!canvas_ || !font_.isLoaded()) return;
    if (font_.getFontType() == FontType::GRAY_4BPP) {
        uint8_t width = 0, height = 0, advance = 0;
        int8_t bearingX = 0, bearingY = 0;
        const uint8_t* bmp = font_.getCharBitmapGray(unicode, width, height, bearingX, bearingY, advance);
        if (!bmp || width == 0 || height == 0) return;
        const int16_t drawX = x + bearingX;
        const int16_t drawY = y + (font_.getFontSize() - bearingY);
        for (int row = 0; row < height; row++) {
            const int16_t py = drawY + row;
            if (py < 0 || py >= kPaperS3Height) continue;
            for (int col = 0; col < width; col++) {
                const int16_t px = drawX + col;
                if (px < 0 || px >= kPaperS3Width) continue;
                const int srcIdx = row * ((width + 1) / 2) + col / 2;
                const uint8_t nibble = (col % 2 == 0) ? ((bmp[srcIdx] >> 4) & 0x0F) : (bmp[srcIdx] & 0x0F);
                if (nibble >= 11) canvas_->drawPixel(px, py, color);
                else if (nibble >= 6) canvas_->drawPixel(px, py, 0x8410);
                else if (nibble > 0) canvas_->drawPixel(px, py, 0xC618);
            }
        }
        return;
    }

    uint8_t width = 0, height = 0;
    const uint8_t* bmp = font_.getCharBitmap(unicode, width, height);
    if (!bmp || width == 0 || height == 0) return;
    for (int row = 0; row < height; row++) {
        const int16_t py = y + row;
        if (py < 0 || py >= kPaperS3Height) continue;
        for (int col = 0; col < width; col++) {
            const int16_t px = x + col;
            if (px < 0 || px >= kPaperS3Width) continue;
            int byteIdx = row * ((width + 7) / 8) + col / 8;
            int bitIdx = 7 - (col % 8);
            if (bmp[byteIdx] & (1 << bitIdx)) canvas_->drawPixel(px, py, color);
        }
    }
}

void ReaderTextRenderer::drawText(int16_t x, int16_t y, const char* text, uint16_t color) {
    if (!canvas_ || !text) return;
    int16_t cx = x;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text);
    size_t pos = 0;
    const size_t len = strlen(text);
    while (pos < len && cx < kPaperS3Width) {
        uint32_t ch = decodeUtf8(bytes, pos, len);
        if (ch == '\n') break;
        drawGlyph(ch, cx, y, color);
        cx += charAdvance(ch);
    }
}

size_t ReaderTextRenderer::findWrapBreak(const char* text, size_t start, int16_t maxWidth) const {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text);
    const size_t len = strlen(text);
    size_t pos = start;
    size_t lastGood = start;
    int16_t width = 0;
    while (pos < len) {
        size_t before = pos;
        uint32_t ch = decodeUtf8(bytes, pos, len);
        if (ch == '\n') return before;
        uint8_t adv = charAdvance(ch);
        if (width + adv > maxWidth) return lastGood > start ? lastGood : before;
        width += adv;
        lastGood = pos;
    }
    return len;
}

void ReaderTextRenderer::renderTextPage(const char* title, const char* body, uint16_t page, uint16_t totalPages, const ReaderRenderOptions& options) {
    if (!canvas_) return;
    if (!ready()) loadDefaultFont();
    canvas_->fillSprite(options.dark ? TFT_BLACK : TFT_WHITE);
    const uint16_t fg = options.dark ? TFT_WHITE : TFT_BLACK;
    const uint16_t mid = options.dark ? 0xC618 : 0x8410;

    drawText(options.marginLeft, 28, title ? title : "未命名书籍", mid);
    canvas_->drawFastHLine(options.marginLeft, 64, kPaperS3Width - options.marginLeft - options.marginRight, mid);

    const char* text = body ? body : "";
    size_t pos = 0;
    const size_t len = strlen(text);
    int16_t y = options.marginTop;
    const int16_t maxWidth = kPaperS3Width - options.marginLeft - options.marginRight;
    const int16_t lineHeight = fontSize() + options.lineGap;
    const int16_t bottom = kPaperS3Height - options.marginBottom;
    while (pos < len && y + lineHeight < bottom) {
        while (pos < len && (text[pos] == '\n' || text[pos] == '\r')) pos++;
        size_t end = findWrapBreak(text, pos, maxWidth);
        if (end <= pos) break;
        char line[256];
        size_t n = end - pos;
        if (n >= sizeof(line)) n = sizeof(line) - 1;
        memcpy(line, text + pos, n);
        line[n] = '\0';
        drawText(options.marginLeft, y, line, fg);
        pos = end;
        y += lineHeight;
    }

    char footer[48];
    snprintf(footer, sizeof(footer), "%u / %u", static_cast<unsigned>(page), static_cast<unsigned>(totalPages));
    drawText(kPaperS3Width - options.marginRight - textWidth(footer), kPaperS3Height - 34, footer, mid);
}

void ReaderTextRenderer::renderPlaceholderPage() {
    static const char* sample =
        "这是 Vink v0.3 的正文渲染层。界面文字使用 ReadPaper UI 子集字体，正文阅读使用独立的阅读字体路径。\n"
        "下一步会把本地 TXT、分页、书签和 Legado 进度映射接到这里，避免 UI 字体和正文阅读互相污染。";
    renderTextPage("示例正文", sample, 1, 1);
}

} // namespace vink3
