#include "InputService.h"
#include "../display/DisplayService.h"

namespace vink3 {

InputService g_inputService;

namespace {
constexpr uint32_t kPollDelayMs = 10;
constexpr uint32_t kDebounceMs = 120;
constexpr int16_t kSwipeThresholdPx = 80;
}

bool InputService::begin(StateMachine* stateMachine) {
    if (!stateMachine) return false;
    stateMachine_ = stateMachine;
    if (!task_) {
        BaseType_t ok = xTaskCreatePinnedToCore(taskThunk, "vink3-input", 8192, this, 2, &task_, 1);
        if (ok != pdPASS) {
            task_ = nullptr;
            return false;
        }
    }
    Serial.println("[vink3][input] service started");
    return true;
}

void InputService::taskThunk(void* arg) {
    static_cast<InputService*>(arg)->taskLoop();
}

void InputService::taskLoop() {
    for (;;) {
        M5.update();
        pollTouch();
        vTaskDelay(pdMS_TO_TICKS(kPollDelayMs));
    }
}

void InputService::pollTouch() {
    if (!stateMachine_ || g_inDisplayPush) return;

    auto detail = M5.Touch.getDetail();
    const bool pressed = detail.isPressed();
    const uint32_t now = millis();

    if (pressed && !wasPressed_) {
        if (now - lastEventMs_ < kDebounceMs) return;
        wasPressed_ = true;
        pressStartedMs_ = now;
        pressPoint_ = {static_cast<int16_t>(detail.x), static_cast<int16_t>(detail.y)};
        Message msg;
        msg.type = MessageType::TouchDown;
        msg.timestampMs = now;
        msg.touch = pressPoint_;
        stateMachine_->post(msg);
        return;
    }

    if (!pressed && wasPressed_) {
        wasPressed_ = false;
        lastEventMs_ = now;
        const TouchPoint releasePoint{static_cast<int16_t>(detail.x), static_cast<int16_t>(detail.y)};
        const int16_t dx = releasePoint.x - pressPoint_.x;
        const int16_t dy = releasePoint.y - pressPoint_.y;

        Message up;
        up.type = MessageType::TouchUp;
        up.timestampMs = now;
        up.touch = releasePoint;
        stateMachine_->post(up);

        Message semantic;
        semantic.timestampMs = now;
        semantic.touch = releasePoint;
        if (abs(dx) > abs(dy) && abs(dx) >= kSwipeThresholdPx) {
            semantic.type = dx > 0 ? MessageType::SwipeRight : MessageType::SwipeLeft;
        } else if (abs(dy) >= kSwipeThresholdPx) {
            semantic.type = dy > 0 ? MessageType::SwipeDown : MessageType::SwipeUp;
        } else if (now - pressStartedMs_ > 700) {
            semantic.type = MessageType::LongPress;
        } else {
            semantic.type = MessageType::Tap;
        }
        stateMachine_->post(semantic);
    }
}

} // namespace vink3
