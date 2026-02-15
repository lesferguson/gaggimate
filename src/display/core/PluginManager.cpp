#include "PluginManager.h"
#include <display/core/Log.h>

void PluginManager::registerPlugin(Plugin *plugin) { plugins.push_back(plugin); }

void PluginManager::setup(Controller *controller) {
    Logger.verbose(LOG_PLUGIN_MGR, "Setting up PluginManager");
    on("system:dummy", [](const Event &) {
        // Register a dummy event so the event map is initialized properly
    });
    for (const auto &plugin : plugins) {
        plugin->setup(controller, this);
    }
    initialized = true;
}

void PluginManager::loop() {
    if (!initialized)
        return;
    for (auto &plugin : plugins) {
        plugin->loop();
    }
}

void PluginManager::on(const String &eventId, const EventCallback &callback) {
    Logger.verbose(LOG_PLUGIN_MGR, "Registering listener: %s", eventId.c_str());
    listeners[std::string(eventId.c_str())].push_back(callback);
}

Event PluginManager::trigger(const String &eventId) {
    Event event;
    event.id = eventId;
    trigger(event);
    return event;
}

Event PluginManager::trigger(const String &eventId, const String &key, const String &value) {
    Event event;
    event.id = eventId;
    event.setString(key, value);
    trigger(event);
    return event;
}

Event PluginManager::trigger(const String &eventId, const String &key, const int value) {
    Event event;
    event.id = eventId;
    event.setInt(key, value);
    trigger(event);
    return event;
}

Event PluginManager::trigger(const String &eventId, const String &key, const float value) {
    Event event;
    event.id = eventId;
    event.setFloat(key, value);
    trigger(event);
    return event;
}

void PluginManager::trigger(Event &event) {
    Logger.verbose(LOG_PLUGIN_MGR, "Triggering event: %s", event.id.c_str());
    if (listeners.count(std::string(event.id.c_str()))) {
        for (auto const &callback : listeners[std::string(event.id.c_str())]) {
            callback(event);
            if (event.stopPropagation) {
                break;
            }
        }
    }
}
