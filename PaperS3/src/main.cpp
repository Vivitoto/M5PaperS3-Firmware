#include <Arduino.h>
#include "App.h"

App app;

void setup() {
    if (!app.init()) {
        Serial.println("[Main] Init failed, halting");
        while (true) { delay(1000); }
    }
}

void loop() {
    app.run();
}
