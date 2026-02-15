#include "Log.h"

// Global instance for WebSocket log streaming
WebLogStream webLogStream;

size_t WebLogStream::write(uint8_t c) {
    ringBuffer[writePos % BUFFER_SIZE] = (char)c;
    writePos++;
    return 1;
}

size_t WebLogStream::write(const uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        ringBuffer[writePos % BUFFER_SIZE] = (char)buffer[i];
        writePos++;
    }
    return size;
}

size_t WebLogStream::drain(char *out, size_t maxLen) {
    size_t wp = writePos;
    // If reader fell behind by more than buffer size, skip ahead
    if (wp - readPos > BUFFER_SIZE) {
        readPos = wp - BUFFER_SIZE;
    }
    size_t avail = wp - readPos;
    if (avail == 0)
        return 0;
    if (avail > maxLen)
        avail = maxLen;
    for (size_t i = 0; i < avail; i++) {
        out[i] = ringBuffer[(readPos + i) % BUFFER_SIZE];
    }
    readPos += avail;
    return avail;
}

struct LogIdConfig {
    LogId id;
    const char *serialTag;
};

static constexpr LogIdConfig LOG_CONFIGS[] = {
    {LOG_CORE, "CTL"},       {LOG_WEBUI, "WEB"},       {LOG_BLE_SCALE, "BLE"},
    {LOG_MQTT, "MQT"},       {LOG_OTA, "OTA"},         {LOG_SHOT_HIST, "SHT"},
    {LOG_PROFILE, "PRF"},    {LOG_SETTINGS, "SET"},     {LOG_DRIVER, "DRV"},
    {LOG_PLUGIN_MGR, "PLG"}, {LOG_GRIND, "GND"},       {LOG_HOMEKIT, "HMK"},
    {LOG_BOILER_FILL, "BFL"},{LOG_MDNS, "DNS"},         {LOG_WAKEUP, "WKP"},
    {LOG_LED, "LED"},
};

void initLogging(bool hasSDCard) {
    Logger.configure(100, false);

    for (const auto &cfg : LOG_CONFIGS) {
        Logger.registerSerial(cfg.id, ELOG_LEVEL_DEBUG, cfg.serialTag);
        Logger.registerSerial(cfg.id, ELOG_LEVEL_INFO, cfg.serialTag, webLogStream);
    }
}
