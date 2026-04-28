#include "VinkRuntime.h"
#include "../display/DisplayService.h"
#include "../input/InputService.h"
#include "../state/StateMachine.h"
#include "../sync/LegadoService.h"
#include "../ui/VinkUiRenderer.h"

namespace vink3 {

VinkRuntime g_runtime;

bool VinkRuntime::begin() {
    Serial.println("[vink3][runtime] starting v0.3.0 ReadPaper-style runtime scaffold");
    if (!beginCanvas()) return false;
    if (!g_uiRenderer.begin(&canvas_)) return false;
    if (!g_displayService.begin(&canvas_)) return false;
    if (!g_stateMachine.begin()) return false;
    if (!g_inputService.begin(&g_stateMachine)) return false;
    if (!g_legadoService.begin(&g_stateMachine)) return false;
    drawBootAndHome();
    return true;
}

bool VinkRuntime::beginCanvas() {
    if (canvasReady_) return true;
    canvas_.setColorDepth(4);
    if (!canvas_.createSprite(540, 960)) {
        Serial.println("[vink3][runtime] full-screen canvas allocation failed");
        return false;
    }
    canvasReady_ = true;
    Serial.println("[vink3][runtime] full-screen canvas ready");
    return true;
}

void VinkRuntime::drawBootAndHome() {
    g_uiRenderer.renderBoot();
    g_displayService.enqueueFull(true, 100);

    Message bootDone;
    bootDone.type = MessageType::BootComplete;
    bootDone.timestampMs = millis();
    g_stateMachine.post(bootDone, 100);

    g_uiRenderer.renderHome(SystemState::Home);
    g_displayService.enqueueFull(false, 100);
}

M5Canvas* VinkRuntime::canvas() {
    return canvasReady_ ? &canvas_ : nullptr;
}

} // namespace vink3
