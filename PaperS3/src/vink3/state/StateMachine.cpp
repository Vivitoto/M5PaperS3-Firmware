#include "StateMachine.h"
#include "../display/DisplayService.h"
#include "../ui/VinkUiRenderer.h"

namespace vink3 {

StateMachine g_stateMachine;

bool StateMachine::begin(uint8_t queueLen) {
    if (!queue_) {
        queue_ = xQueueCreate(queueLen, sizeof(Message));
        if (!queue_) return false;
    }
    if (!task_) {
        BaseType_t ok = xTaskCreatePinnedToCore(taskThunk, "vink3-state", 8192, this, 3, &task_, 1);
        if (ok != pdPASS) {
            task_ = nullptr;
            return false;
        }
    }
    Serial.println("[vink3][state] service started");
    return true;
}

bool StateMachine::post(const Message& message, uint32_t timeoutMs) {
    if (!queue_) return false;
    return xQueueSend(queue_, &message, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

SystemState StateMachine::state() const {
    return state_;
}

void StateMachine::taskThunk(void* arg) {
    static_cast<StateMachine*>(arg)->taskLoop();
}

void StateMachine::taskLoop() {
    Message message;
    for (;;) {
        if (xQueueReceive(queue_, &message, portMAX_DELAY) == pdTRUE) {
            handle(message);
        }
    }
}

void StateMachine::handle(const Message& message) {
    switch (message.type) {
        case MessageType::BootComplete:
            state_ = SystemState::Home;
            g_uiRenderer.renderHome(state_);
            g_displayService.enqueueFull(false, 100);
            break;

        case MessageType::Tap:
            // Initial v0.3 Vink shell hit zones. This is deliberately simple; the
            // important part is ReadPaper-style state ownership rather than UI-loop mutation.
            if (state_ == SystemState::Home) {
                if (message.touch.y >= 408 && message.touch.y < 520) {
                    state_ = SystemState::LegadoSync;
                    g_uiRenderer.renderLegadoSync("Legado sync service ready");
                } else {
                    g_uiRenderer.renderHome(state_);
                }
                g_displayService.enqueueFull(false, 100);
            } else {
                state_ = SystemState::Home;
                g_uiRenderer.renderHome(state_);
                g_displayService.enqueueFull(false, 100);
            }
            break;

        case MessageType::LegadoSyncStart:
            state_ = SystemState::LegadoSync;
            g_uiRenderer.renderLegadoSync("Syncing reading progress...");
            g_displayService.enqueueFull(false, 100);
            break;

        case MessageType::LegadoSyncDone:
            state_ = SystemState::LegadoSync;
            g_uiRenderer.renderLegadoSync("Sync complete");
            g_displayService.enqueueFull(false, 100);
            break;

        case MessageType::LegadoSyncFailed:
            state_ = SystemState::LegadoSync;
            g_uiRenderer.renderLegadoSync("Sync failed");
            g_displayService.enqueueFull(true, 100);
            break;

        case MessageType::DisplayDone:
        case MessageType::TouchDown:
        case MessageType::TouchUp:
        case MessageType::None:
        default:
            break;
    }
}

} // namespace vink3
