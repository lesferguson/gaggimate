#include "Log.h"
#include <SD_MMC.h>
#include <SPIFFS.h>

// Global instances
WebLogStream webLogStream;
FileLogStream fileLogStream;

// --- WebLogStream ---

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

size_t WebLogStream::snapshot(char *out, size_t maxLen) {
    size_t wp = writePos.load();
    size_t cp = clearPos.load();
    // Oldest data still in the ring buffer
    size_t oldest = (wp > BUFFER_SIZE) ? wp - BUFFER_SIZE : 0;
    // Effective start is the later of oldest data or clear position
    size_t start = (cp > oldest) ? cp : oldest;
    size_t avail = wp - start;
    if (avail == 0)
        return 0;
    if (avail > maxLen)
        avail = maxLen;
    // Read from the most recent `avail` bytes
    size_t offset = wp - avail;
    for (size_t i = 0; i < avail; i++) {
        out[i] = ringBuffer[(offset + i) % BUFFER_SIZE];
    }
    return avail;
}

size_t WebLogStream::contentSince(size_t &fromPos, char *out, size_t maxLen) {
    size_t wp = writePos.load();
    size_t cp = clearPos.load();
    // If fromPos is before clear point, jump forward
    if (fromPos < cp) {
        fromPos = cp;
    }
    // If reader fell behind by more than buffer size, skip ahead
    size_t oldest = (wp > BUFFER_SIZE) ? wp - BUFFER_SIZE : 0;
    if (fromPos < oldest) {
        fromPos = oldest;
    }
    size_t avail = wp - fromPos;
    if (avail == 0)
        return 0;
    if (avail > maxLen)
        avail = maxLen;
    for (size_t i = 0; i < avail; i++) {
        out[i] = ringBuffer[(fromPos + i) % BUFFER_SIZE];
    }
    fromPos += avail;
    return avail;
}

void WebLogStream::clear() {
    clearPos.store(writePos.load());
}

// --- FileLogStream ---

bool FileLogStream::begin(FS &fs, const char *logPath, const char *oldPath, size_t maxFileSize) {
    _fs = &fs;
    _logPath = logPath;
    _oldPath = oldPath;
    _maxFileSize = maxFileSize;

    // Check existing file size so we can continue appending
    if (_fs->exists(_logPath)) {
        File f = _fs->open(_logPath, FILE_READ);
        if (f) {
            _fileSize = f.size();
            f.close();
        }
    }
    _ready = true;
    return true;
}

size_t FileLogStream::write(uint8_t c) {
    if (!_ready) return 1;
    // Flush before writing if buffer is full (prevents out-of-bounds write)
    if (_bufferPos >= WRITE_BUFFER_SIZE) {
        flushBuffer();
    }
    _buffer[_bufferPos++] = c;
    // Also flush on newline to ensure every log line is persisted promptly
    if (c == '\n') {
        flushBuffer();
    }
    return 1;
}

size_t FileLogStream::write(const uint8_t *buffer, size_t size) {
    if (!_ready) return size;
    for (size_t i = 0; i < size; i++) {
        if (_bufferPos >= WRITE_BUFFER_SIZE) {
            flushBuffer();
        }
        _buffer[_bufferPos++] = buffer[i];
        if (buffer[i] == '\n') {
            flushBuffer();
        }
    }
    return size;
}

void FileLogStream::flushBuffer() {
    if (_bufferPos == 0 || !_fs) return;

    // Rotate if needed before writing
    if (_fileSize + _bufferPos > _maxFileSize) {
        rotate();
    }

    File f = _fs->open(_logPath, FILE_APPEND);
    if (f) {
        f.write(_buffer, _bufferPos);
        _fileSize += _bufferPos;
        f.close();
    }
    _bufferPos = 0;
}

void FileLogStream::rotate() {
    // Delete old backup, rename current to backup
    if (_fs->exists(_oldPath)) {
        _fs->remove(_oldPath);
    }
    if (_fs->exists(_logPath)) {
        _fs->rename(_logPath, _oldPath);
    }
    _fileSize = 0;
}

// --- Logging config ---

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

    if (hasSDCard) {
        // SD card: log INFO+ to /logs/system.log (500KB max, rotates to .old)
        if (!SD_MMC.exists("/logs")) {
            SD_MMC.mkdir("/logs");
        }
        fileLogStream.begin(SD_MMC, "/logs/system.log", "/logs/system.old", 500 * 1024);
    } else {
        // No SD: log WARNING+ to SPIFFS (50KB max to conserve space)
        fileLogStream.begin(SPIFFS, "/system.log", "/system.old", 50 * 1024);
    }

    for (const auto &cfg : LOG_CONFIGS) {
        Logger.registerSerial(cfg.id, ELOG_LEVEL_DEBUG, cfg.serialTag);
        Logger.registerSerial(cfg.id, ELOG_LEVEL_DEBUG, cfg.serialTag, webLogStream);
        // File: INFO on SD, WARNING on SPIFFS
        Logger.registerSerial(cfg.id, hasSDCard ? ELOG_LEVEL_INFO : ELOG_LEVEL_WARNING, cfg.serialTag, fileLogStream);
    }
}
