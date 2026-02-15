#ifndef SYSLOGPLUGIN_H
#define SYSLOGPLUGIN_H

#include <display/core/Plugin.h>

class SyslogPlugin : public Plugin {
  public:
    void setup(Controller *controller, PluginManager *pluginManager) override;
    void loop() override;

  private:
    void configureSyslog();

    Controller *controller = nullptr;
    bool syslogRegistered = false;
};

#endif // SYSLOGPLUGIN_H
