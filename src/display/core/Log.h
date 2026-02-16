#ifndef LOG_H
#define LOG_H

#include <Elog.h>
#include <FS.h>
#include <atomic>

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

// Ring buffer stream that captures Elog output for streaming to WebSocket clients.
// All reads are non-consuming — multiple readers see the same content.
class WebLogStream : public Stream {
  public:
    static constexpr size_t BUFFER_SIZE = 8192;

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }

    // Read all buffer content since last clear (up to maxLen). Non-consuming.
    size_t snapshot(char *out, size_t maxLen);

    // Read content written since fromPos (up to maxLen). Updates fromPos for next call.
    size_t contentSince(size_t &fromPos, char *out, size_t maxLen);

    // Current write position — use to initialize a reader's tracking position.
    size_t getWritePos() const { return writePos.load(); }

    // Mark the buffer as cleared. Future snapshot/contentSince calls only return content after this point.
    void clear();

  private:
    char ringBuffer[BUFFER_SIZE]{};
    std::atomic<size_t> writePos{0};
    std::atomic<size_t> clearPos{0};
};

// Buffered file stream that writes Elog output to SD_MMC or SPIFFS
class FileLogStream : public Stream {
  public:
    static constexpr size_t WRITE_BUFFER_SIZE = 2048;

    bool begin(FS &fs, const char *logPath, const char *oldPath, size_t maxFileSize);

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }

  private:
    void flushBuffer();
    void rotate();

    FS *_fs = nullptr;
    const char *_logPath = nullptr;
    const char *_oldPath = nullptr;
    size_t _maxFileSize = 0;
    size_t _fileSize = 0;
    uint8_t _buffer[WRITE_BUFFER_SIZE]{};
    size_t _bufferPos = 0;
    bool _ready = false;
};

extern WebLogStream webLogStream;
extern FileLogStream fileLogStream;

void initLogging(bool hasSDCard);

#endif // LOG_H
