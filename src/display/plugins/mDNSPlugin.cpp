#include "mDNSPlugin.h"
#include "../core/Controller.h"
#include "../core/Event.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <display/core/Log.h>
#include <version.h>

void mDNSPlugin::setup(Controller *controller, PluginManager *pluginManager) {
    this->controller = controller;
    pluginManager->on("controller:wifi:connect", [this](Event const &event) { start(event); });
}
void mDNSPlugin::start(Event const &event) const {
    const int apMode = event.getInt("AP");
    if (apMode)
        return;
    if (!MDNS.begin(controller->getSettings().getMdnsName().c_str())) {
        Logger.error(LOG_MDNS, "Error setting up mDNS responder");
        return;
    }

    // Advertise HTTP service for web interface
    MDNS.addService("http", "tcp", 80);

    // Advertise custom gaggimate service for Home Assistant discovery
    MDNS.addService("gaggimate", "tcp", 80);

    // Add service metadata as TXT records
    MDNS.addServiceTxt("gaggimate", "tcp", "version", BUILD_GIT_VERSION);
    MDNS.addServiceTxt("gaggimate", "tcp", "type", "espresso_machine");

    Logger.info(LOG_MDNS, "mDNS responder started with service advertisement");
}
