#ifndef LOG_H
#define LOG_H

#include <Elog.h>

enum LogId : uint8_t {
    LOG_CORE = 0,
    LOG_WEBUI,
    LOG_BLE_SCALE,
    LOG_MQTT,
    LOG_OTA,
    LOG_SHOT_HIST,
    LOG_PROFILE,
    LOG_SETTINGS,
    LOG_DRIVER,
    LOG_PLUGIN_MGR,
    LOG_GRIND,
    LOG_HOMEKIT,
    LOG_BOILER_FILL,
    LOG_MDNS,
    LOG_WAKEUP,
    LOG_LED,
    LOG_ID_COUNT
};

// Ring buffer stream that captures Elog output for streaming to WebSocket clients
class WebLogStream : public Stream {
  public:
    static constexpr size_t BUFFER_SIZE = 8192;

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }

    // Returns new content since last drain (up to maxLen). Caller provides buffer.
    size_t drain(char *out, size_t maxLen);

  private:
    char ringBuffer[BUFFER_SIZE]{};
    volatile size_t writePos = 0;
    size_t readPos = 0;
};

extern WebLogStream webLogStream;

void initLogging(bool hasSDCard);

#endif // LOG_H
