#include "LilyGoDriver.h"
#include <Arduino.h>
#include <display/core/Log.h>
#include <display/drivers/common/LV_Helper.h>

LilyGoDriver *LilyGoDriver::instance = nullptr;

bool LilyGoDriver::isCompatible() {
    pinMode(LILYGO_DETECT_PIN, INPUT_PULLDOWN);
    return digitalRead(LILYGO_DETECT_PIN);
}

void LilyGoDriver::init() {
    Logger.info(LOG_DRIVER, "Initializing LilyGo driver");
    if (!panel.begin()) {
        for (uint8_t i = 0; i < 20; i++) {
            Logger.error(LOG_DRIVER, "Error, failed to initialize T-RGB");
            delay(1000);
        }
        ESP.restart();
    }
    beginLvglHelper(panel);
}

bool LilyGoDriver::supportsSDCard() { return true; }

bool LilyGoDriver::installSDCard() { return panel.installSD(); }
