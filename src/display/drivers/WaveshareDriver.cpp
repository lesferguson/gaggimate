#include "WaveshareDriver.h"
#include <display/core/Log.h>
#include <display/drivers/common/LV_Helper.h>

WaveshareDriver *WaveshareDriver::instance = nullptr;

void WaveshareDriver::init() {
    Logger.info(LOG_DRIVER, "WaveshareDriver initialzing");
    if (!panel.begin()) {
        for (uint8_t i = 0; i < 20; i++) {
            Logger.error(LOG_DRIVER, "Error, failed to initialize T-RGB");
            delay(1000);
        }
        ESP.restart();
    }
    beginLvglHelper(panel);
}

bool WaveshareDriver::supportsSDCard() { return true; }

bool WaveshareDriver::installSDCard() { return panel.installSD(); }
