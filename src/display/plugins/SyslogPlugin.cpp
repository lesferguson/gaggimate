#include "SyslogPlugin.h"
#include <display/core/Controller.h>
#include <display/core/Log.h>

static constexpr const char *SYSLOG_APP_NAMES[] = {"ctl", "web", "ble", "mqt", "ota", "sht", "prf", "set", "drv", "plg",
                                                    "gnd", "hmk", "bfl", "dns", "wkp", "led"};

void SyslogPlugin::setup(Controller *_controller, PluginManager *_pluginManager) {
    this->controller = _controller;

    _pluginManager->on("controller:wifi:connect", [this](Event const &event) {
        if (event.getInt("AP"))
            return; // Syslog only works in station mode
        configureSyslog();
    });

    _pluginManager->on("settings:changed", [this](Event const &) { configureSyslog(); });
}

void SyslogPlugin::loop() {}

void SyslogPlugin::configureSyslog() {
    const Settings &settings = controller->getSettings();

    if (!settings.isSyslogEnabled() || settings.getSyslogHost().isEmpty()) {
        return;
    }

    const char *host = settings.getSyslogHost().c_str();
    uint16_t port = settings.getSyslogPort();
    const char *hostname = settings.getMdnsName().c_str();

    Logger.configureSyslog(host, port, hostname, false, 2000);
    Logger.info(LOG_CORE, "Syslog configured: %s:%d", host, port);

    if (!syslogRegistered) {
        for (uint8_t id = 0; id < LOG_ID_COUNT; id++) {
            Logger.registerSyslog(id, ELOG_LEVEL_INFO, ELOG_FAC_LOCAL0, SYSLOG_APP_NAMES[id]);
        }
        syslogRegistered = true;
        Logger.info(LOG_CORE, "Syslog destinations registered for %d log IDs", LOG_ID_COUNT);
    }
}
