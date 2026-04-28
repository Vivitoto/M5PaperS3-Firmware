#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include "../state/Messages.h"

namespace vink3 {

class VinkUiRenderer {
public:
    bool begin(M5Canvas* canvas);
    void renderBoot();
    void renderHome(SystemState state);
    void renderLegadoSync(const char* status);

private:
    void clear();
    void drawStatusBar(const char* title);

    M5Canvas* canvas_ = nullptr;
};

extern VinkUiRenderer g_uiRenderer;

} // namespace vink3
