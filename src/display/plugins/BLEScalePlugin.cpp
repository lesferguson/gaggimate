#include "BLEScalePlugin.h"
#include "remote_scales.h"
#include "remote_scales_plugin_registry.h"
#include <cmath> // For isfinite()
#include <display/core/Controller.h>
#include <display/core/Log.h>
#include <scales/acaia.h>
#include <scales/bookoo.h>
#include <scales/decent.h>
#include <scales/difluid.h>
#include <scales/eclair.h>
#include <scales/eureka.h>
#include <scales/felicitaScale.h>
#include <scales/myscale.h>
#include <scales/timemore.h>
#include <scales/varia.h>
#include <scales/weighmybru.h>

void on_ble_measurement(float value) {
    if (&BLEScales != nullptr) {
        BLEScales.onMeasurement(value);
    }
}

BLEScalePlugin BLEScales;

BLEScalePlugin::BLEScalePlugin() = default;

BLEScalePlugin::~BLEScalePlugin() {
    // Disable active flag first to stop processing
    active = false;

    // Give any running callbacks time to complete
    delay(100);

    // Ensure proper cleanup
    disconnect();

    if (scanner != nullptr) {
        // Stop scanning first
        scanner->stopAsyncScan();
        // Give it time to actually stop
        delay(50);
        delete scanner;
        scanner = nullptr;
    }
}

void BLEScalePlugin::setup(Controller *controller, PluginManager *manager) {
    if (controller == nullptr || manager == nullptr) {
        Logger.error(LOG_BLE_SCALE, "Invalid controller or manager passed to setup");
        return;
    }

    this->controller = controller;
    this->pluginRegistry = RemoteScalesPluginRegistry::getInstance();

    // Apply scale plugins with error checking
    AcaiaScalesPlugin::apply();
    BookooScalesPlugin::apply();
    DecentScalesPlugin::apply();
    DifluidScalesPlugin::apply();
    EclairScalesPlugin::apply();
    EurekaScalesPlugin::apply();
    FelicitaScalePlugin::apply();
    TimemoreScalesPlugin::apply();
    VariaScalesPlugin::apply();
    WeighMyBrewScalePlugin::apply();
    myscalePlugin::apply();

    // Initialize scanner with error handling
    this->scanner = new (std::nothrow) RemoteScalesScanner();
    if (this->scanner == nullptr) {
        Logger.error(LOG_BLE_SCALE, "Failed to create RemoteScalesScanner - out of memory");
        return;
    }

    manager->on("controller:ready", [this](Event const &) {
        if (this->controller != nullptr && this->controller->getMode() != MODE_STANDBY) {
            Logger.info(LOG_BLE_SCALE, "Resuming scanning");
            scan();
            active = true;
        }
    });
    manager->on("controller:brew:prestart", [this](Event const &) { onProcessStart(); });
    manager->on("controller:grind:start", [this](Event const &) { onProcessStart(); });
    manager->on("controller:mode:change", [this](Event const &event) {
        if (event.getInt("value") != MODE_STANDBY) {
            Logger.info(LOG_BLE_SCALE, "Resuming scanning");
            scan();
            active = true;
        } else {
            active = false;
            disconnect();
            if (scanner != nullptr) {
                scanner->stopAsyncScan();
            }
            Logger.info(LOG_BLE_SCALE, "Stopping scanning, disconnecting");
        }
    });
}

void BLEScalePlugin::loop() {
    if (doConnect && scale == nullptr) {
        establishConnection();
    }
    const unsigned long now = millis();
    if (now - lastUpdate > UPDATE_INTERVAL_MS) {
        lastUpdate = now;
        update();
    }
}

void BLEScalePlugin::update() {
    // Graceful failure - if controller is null, just disable ourselves
    if (controller == nullptr) {
        Logger.warning(LOG_BLE_SCALE, "Controller is null, disabling BLE scale");
        active = false;
        return;
    }

    // Don't update volumetric override if scale access might fail
    bool hasConnectedScale = false;
    if (scale != nullptr) {
        // Check if scale pointer is valid before accessing
        hasConnectedScale = scale->isConnected();
    }

    if (controller->isVolumetricAvailable())
        controller->setVolumetricOverride(hasConnectedScale);

    if (!active)
        return;

    if (scale != nullptr) {
        // Call scale update with error checking
        scale->update();
        if (!hasConnectedScale) {
            reconnectionTries++;
            Logger.info(LOG_BLE_SCALE, "Scale disconnected, reconnection attempt %d/%d", reconnectionTries, RECONNECTION_TRIES);
            if (reconnectionTries > RECONNECTION_TRIES) {
                Logger.warning(LOG_BLE_SCALE, "Max reconnection attempts reached, disconnecting and restarting scan");
                disconnect();
                if (scanner != nullptr) {
                    scanner->initializeAsyncScan();
                }
            }
        }
    } else if (controller->getSettings().getSavedScale() != "" && scanner != nullptr) {
        // Protected scanner access with null checks
        auto discoveredScales = scanner->getDiscoveredScales();
        for (const auto &d : discoveredScales) {
            if (d.getAddress().toString() == controller->getSettings().getSavedScale().c_str()) {
                Logger.info(LOG_BLE_SCALE, "Connecting to last known scale");
                connect(d.getAddress().toString());
                break;
            }
        }
    }
}

void BLEScalePlugin::connect(const std::string &uuid) {
    if (uuid.empty()) {
        Logger.error(LOG_BLE_SCALE, "Cannot connect with empty UUID");
        return;
    }
    if (controller == nullptr) {
        Logger.error(LOG_BLE_SCALE, "Controller is null, cannot save scale setting");
        return;
    }

    doConnect = true;
    this->uuid = uuid;
    controller->getSettings().setSavedScale(uuid.data());
}

void BLEScalePlugin::scan() const {
    if (scale != nullptr && scale->isConnected()) {
        return;
    }
    if (scanner == nullptr) {
        Logger.error(LOG_BLE_SCALE, "Scanner not initialized, cannot start scan");
        return;
    }
    scanner->initializeAsyncScan();
}

void BLEScalePlugin::disconnect() {
    if (scale != nullptr) {
        // Add small delay to let any pending callbacks complete
        delay(50);

        // Check if scale is still valid before calling disconnect
        if (scale) {
            scale->disconnect();
        }

        scale = nullptr;
        uuid = "";
        doConnect = false;
        reconnectionTries = 0;
    }
}

void BLEScalePlugin::onProcessStart() const {
    if (scale != nullptr && scale->isConnected()) {
        // Double tare with validation
        scale->tare();
        delay(50);

        // Check if scale is still connected before second tare
        if (scale != nullptr && scale->isConnected()) {
            scale->tare();
        }
    }
}

void BLEScalePlugin::tare() const { onProcessStart(); }

void BLEScalePlugin::establishConnection() {
    if (uuid.empty()) {
        Logger.error(LOG_BLE_SCALE, "Cannot establish connection with empty UUID");
        return;
    }

    Logger.info(LOG_BLE_SCALE, "Connecting to %s", uuid.c_str());
    if (scanner == nullptr) {
        Logger.error(LOG_BLE_SCALE, "Scanner not initialized, cannot establish connection");
        return;
    }

    scanner->stopAsyncScan();

    auto discoveredScales = scanner->getDiscoveredScales();
    bool deviceFound = false;

    for (const auto &d : discoveredScales) {
        if (d.getAddress().toString() == uuid) {
            deviceFound = true;
            reconnectionTries = 0;

            Logger.info(LOG_BLE_SCALE, "Found device: name=%s addr=%s",
                        d.getName().c_str(), d.getAddress().toString().c_str());

            auto factory = RemoteScalesFactory::getInstance();
            if (factory == nullptr) {
                Logger.error(LOG_BLE_SCALE, "RemoteScalesFactory instance is null");
                return;
            }

            scale = factory->create(d);
            if (!scale) {
                Logger.error(LOG_BLE_SCALE, "Factory failed to create scale for device %s", d.getName().c_str());
                return;
            }

            scale->setLogCallback([](std::string message) {
                if (!message.empty()) {
                    Logger.info(LOG_BLE_SCALE, "[scale] %s", message.c_str());
                }
            });

            scale->setWeightUpdatedCallback([](float weight) {
                // Check if we're in an ISR context
                if (xPortInIsrContext()) {
                    // Skip measurement to avoid FreeRTOS deadlocks from interrupt context
                    return;
                }
                // Safe to call directly from task context with null check
                if (&BLEScales != nullptr) {
                    BLEScales.onMeasurement(weight);
                }
            });

            Logger.info(LOG_BLE_SCALE, "Attempting BLE connect to %s...", d.getName().c_str());
            bool connectResult = scale->connect();
            if (!connectResult) {
                Logger.warning(LOG_BLE_SCALE, "Connect failed for %s (addr=%s), retrying scan",
                               d.getName().c_str(), d.getAddress().toString().c_str());
                disconnect();
                if (scanner != nullptr) {
                    scanner->initializeAsyncScan();
                }
            } else {
                Logger.info(LOG_BLE_SCALE, "Successfully connected to %s", d.getName().c_str());
            }
            break;
        }
    }

    if (!deviceFound) {
        Logger.warning(LOG_BLE_SCALE, "Device %s not found in discovered scales", uuid.c_str());
        if (scanner != nullptr) {
            scanner->initializeAsyncScan();
        }
    }
}

void BLEScalePlugin::onMeasurement(float value) const {
    // Rate limiting to prevent callback flooding
    unsigned long now = millis();
    if (now - lastMeasurementTime < MIN_MEASUREMENT_INTERVAL_MS) {
        return; // Drop measurement to prevent flooding
    }
    lastMeasurementTime = now;

    // Multiple safety checks to prevent crashes
    if (controller == nullptr) {
        return; // Silently ignore if controller is null
    }

    // Check if we're being destroyed or in an unsafe state
    if (!active) {
        return; // Don't process measurements when not active
    }

    // Validate the measurement value
    if (!isfinite(value) || value < -1000.0f || value > 10000.0f) {
        Logger.warning(LOG_BLE_SCALE, "Invalid measurement value: %f, ignoring", value);
        return;
    }

    // Safe to call controller method
    controller->onVolumetricMeasurement(value, VolumetricMeasurementSource::BLUETOOTH);
}

std::vector<DiscoveredDevice> BLEScalePlugin::getDiscoveredScales() const {
    if (scanner == nullptr) {
        Logger.warning(LOG_BLE_SCALE, "Scanner not initialized, returning empty device list");
        return std::vector<DiscoveredDevice>();
    }
    return scanner->getDiscoveredScales();
}
