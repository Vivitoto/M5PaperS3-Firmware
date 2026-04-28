#pragma once
#include <Arduino.h>
#include <M5Unified.h>

namespace vink3 {

struct DisplayRequest {
    bool quality = false;
    bool invert = false;
    bool transparent = false;
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 540;
    int16_t h = 960;
};

class DisplayService {
public:
    bool begin(M5Canvas* canvas, uint8_t queueLen = 4);
    bool enqueue(const DisplayRequest& request, uint32_t timeoutMs = 20);
    bool enqueueFull(bool quality = false, uint32_t timeoutMs = 20);
    bool waitIdle(uint32_t timeoutMs = 3000) const;
    bool isBusy() const;
    uint32_t pushCount() const;

private:
    static void taskThunk(void* arg);
    void taskLoop();
    void push(const DisplayRequest& request);

    M5Canvas* canvas_ = nullptr;
    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
    volatile bool busy_ = false;
    volatile uint32_t pushCount_ = 0;
};

extern DisplayService g_displayService;
extern volatile bool g_inDisplayPush;

} // namespace vink3
