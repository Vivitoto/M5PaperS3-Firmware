#include "VinkUiRenderer.h"

namespace vink3 {

VinkUiRenderer g_uiRenderer;

bool VinkUiRenderer::begin(M5Canvas* canvas) {
    if (!canvas) return false;
    canvas_ = canvas;
    return true;
}

void VinkUiRenderer::clear() {
    canvas_->fillSprite(TFT_WHITE);
    canvas_->setTextColor(TFT_BLACK, TFT_WHITE);
}

void VinkUiRenderer::drawStatusBar(const char* title) {
    canvas_->fillRect(0, 0, 540, 72, TFT_WHITE);
    canvas_->drawFastHLine(24, 70, 492, TFT_BLACK);
    canvas_->setTextDatum(top_left);
    canvas_->setTextSize(2);
    canvas_->drawString(title ? title : "Vink", 28, 24);
}

void VinkUiRenderer::renderBoot() {
    if (!canvas_) return;
    clear();
    canvas_->setTextDatum(middle_center);
    canvas_->setTextSize(3);
    canvas_->drawString("Vink", 270, 410);
    canvas_->setTextSize(1);
    canvas_->drawString("v0.3.0 ReadPaper Core", 270, 470);
}

void VinkUiRenderer::renderHome(SystemState state) {
    if (!canvas_) return;
    clear();
    drawStatusBar("Vink");
    canvas_->setTextDatum(top_left);
    canvas_->setTextSize(2);
    canvas_->drawString("Reading", 48, 132);
    canvas_->drawRect(32, 112, 476, 112, TFT_BLACK);
    canvas_->drawString("Library", 48, 280);
    canvas_->drawRect(32, 260, 476, 112, TFT_BLACK);
    canvas_->drawString("Transfer / Legado", 48, 428);
    canvas_->drawRect(32, 408, 476, 112, TFT_BLACK);
    canvas_->drawString("Settings", 48, 576);
    canvas_->drawRect(32, 556, 476, 112, TFT_BLACK);
    canvas_->setTextSize(1);
    canvas_->drawString(state == SystemState::Home ? "HOME" : "STATE", 32, 900);
}

void VinkUiRenderer::renderLegadoSync(const char* status) {
    if (!canvas_) return;
    clear();
    drawStatusBar("Legado Sync");
    canvas_->setTextDatum(top_left);
    canvas_->setTextSize(2);
    canvas_->drawString(status ? status : "Waiting", 48, 160);
}

} // namespace vink3
