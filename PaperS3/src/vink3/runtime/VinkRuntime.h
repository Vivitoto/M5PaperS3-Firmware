#pragma once
#include <Arduino.h>
#include <M5Unified.h>

namespace vink3 {

class VinkRuntime {
public:
    bool begin();
    void drawBootAndHome();
    M5Canvas* canvas();

private:
    bool beginCanvas();

    M5Canvas canvas_{&M5.Display};
    bool canvasReady_ = false;
};

extern VinkRuntime g_runtime;

} // namespace vink3
