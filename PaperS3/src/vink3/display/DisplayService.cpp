#include "DisplayService.h"

namespace vink3 {

DisplayService g_displayService;
volatile bool g_inDisplayPush = false;

bool DisplayService::begin(M5Canvas* canvas, uint8_t queueLen) {
    if (!canvas) {
        Serial.println("[vink3][display] begin failed: canvas is null");
        return false;
    }
    canvas_ = canvas;
    if (!queue_) {
        queue_ = xQueueCreate(queueLen, sizeof(DisplayRequest));
        if (!queue_) {
            Serial.println("[vink3][display] begin failed: queue create failed");
            return false;
        }
    }
    if (!task_) {
        BaseType_t ok = xTaskCreatePinnedToCore(
            taskThunk,
            "vink3-display",
            8192,
            this,
            2,
            &task_,
            1);
        if (ok != pdPASS) {
            Serial.println("[vink3][display] begin failed: task create failed");
            task_ = nullptr;
            return false;
        }
    }
    Serial.println("[vink3][display] service started");
    return true;
}

bool DisplayService::enqueue(const DisplayRequest& request, uint32_t timeoutMs) {
    if (!queue_) return false;
    return xQueueSend(queue_, &request, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

bool DisplayService::enqueueFull(bool quality, uint32_t timeoutMs) {
    DisplayRequest request;
    request.quality = quality;
    request.x = 0;
    request.y = 0;
    request.w = 540;
    request.h = 960;
    return enqueue(request, timeoutMs);
}

bool DisplayService::waitIdle(uint32_t timeoutMs) const {
    const uint32_t start = millis();
    while (isBusy()) {
        if (millis() - start >= timeoutMs) return false;
        delay(10);
    }
    return true;
}

bool DisplayService::isBusy() const {
    const bool queued = queue_ && uxQueueMessagesWaiting(queue_) > 0;
    return busy_ || queued;
}

uint32_t DisplayService::pushCount() const {
    return pushCount_;
}

void DisplayService::taskThunk(void* arg) {
    static_cast<DisplayService*>(arg)->taskLoop();
}

void DisplayService::taskLoop() {
    DisplayRequest request;
    for (;;) {
        if (xQueueReceive(queue_, &request, portMAX_DELAY) == pdTRUE) {
            push(request);
        }
    }
}

void DisplayService::push(const DisplayRequest& request) {
    if (!canvas_) return;

    busy_ = true;
    g_inDisplayPush = true;

    M5.Display.powerSaveOff();
    M5.Display.waitDisplay();

    M5.Display.setEpdMode(request.quality ? epd_mode_t::epd_quality : epd_mode_t::epd_text);
    canvas_->pushSprite(&M5.Display, request.x, request.y);
    M5.Display.waitDisplay();

    pushCount_++;
    g_inDisplayPush = false;
    busy_ = false;
}

} // namespace vink3
