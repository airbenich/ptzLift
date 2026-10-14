#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include "relay.h"
#include "preset.h"
#include "state.h"

void configureWebServer(DeviceStateManager& stateManager,
                       RelayController& relayUp,
                       RelayController& relayDown,
                       RelayController& relayPreset1,
                       RelayController& relayPreset2,
                       RelayController& relayPreset3,
                       PresetManager& presetManager);
void handleWebServer();

#endif
