#include <LittleFS.h>
#include <WebServer.h>

#include "web_server.h"

namespace {
WebServer webServer(80);
DeviceStateManager* stateManagerPtr = nullptr;
RelayController* relayUpPtr = nullptr;
RelayController* relayDownPtr = nullptr;
RelayController* relayPreset1Ptr = nullptr;
RelayController* relayPreset2Ptr = nullptr;
RelayController* relayPreset3Ptr = nullptr;
PresetManager* presetManagerPtr = nullptr;
constexpr uint16_t kRelayPulseMs = 150;
constexpr uint16_t kPresetStorePulseMs = 6000;

enum class ActionRelay : uint8_t {
  None = 0,
  Up,
  Down,
  Preset1,
  Preset2,
  Preset3,
};

ActionRelay lastTriggeredRelay = ActionRelay::None;

String actionFromRequestBody() {
  const String body = webServer.arg("plain");
  String action = "";
  const int actionPos = body.indexOf("\"action\":\"");
  if (actionPos < 0) {
    return action;
  }

  const int actionStart = actionPos + 10;
  const int actionEnd = body.indexOf("\"", actionStart);
  if (actionEnd > actionStart) {
    action = body.substring(actionStart, actionEnd);
  }
  return action;
}

String makeStatusJson(const DeviceSnapshot& snapshot) {
  String json = "{";
  json += "\"deviceId\":\"" + snapshot.deviceId + "\",";
  json += "\"relayState\":{";
  json += "\"up\":" + String(snapshot.relayUpEnabled ? 1 : 0) + ",";
  json += "\"down\":" + String(snapshot.relayDownEnabled ? 1 : 0) + ",";
  json += "\"preset1\":" + String(snapshot.relayPreset1Enabled ? 1 : 0) + ",";
  json += "\"preset2\":" + String(snapshot.relayPreset2Enabled ? 1 : 0) + ",";
  json += "\"preset3\":" + String(snapshot.relayPreset3Enabled ? 1 : 0);
  json += "},";
  json += "\"preset\":\"" + snapshot.preset + "\",";
  json += "\"healthy\":" + String(snapshot.healthy ? "true" : "false") + ",";
  json += "\"companionConnected\":" + String(snapshot.companionConnected ? "true" : "false");
  json += "}";
  return json;
}

ActionRelay relayForAction(const String& action) {
  if (action == "up") {
    return ActionRelay::Up;
  }
  if (action == "down") {
    return ActionRelay::Down;
  }
  if (action == "preset1") {
    return ActionRelay::Preset1;
  }
  if (action == "preset2") {
    return ActionRelay::Preset2;
  }
  if (action == "preset3") {
    return ActionRelay::Preset3;
  }
  return ActionRelay::None;
}

bool triggerRelay(ActionRelay relay) {
  switch (relay) {
    case ActionRelay::Up:
      if (relayUpPtr->enabled()) {
        relayUpPtr->setEnabled(false);
      } else {
        relayDownPtr->setEnabled(false);
        relayUpPtr->setEnabled(true);
      }
      return true;
    case ActionRelay::Down:
      if (relayDownPtr->enabled()) {
        relayDownPtr->setEnabled(false);
      } else {
        relayUpPtr->setEnabled(false);
        relayDownPtr->setEnabled(true);
      }
      return true;
    case ActionRelay::Preset1:
      relayPreset1Ptr->trigger(kRelayPulseMs);
      return true;
    case ActionRelay::Preset2:
      relayPreset2Ptr->trigger(kRelayPulseMs);
      return true;
    case ActionRelay::Preset3:
      relayPreset3Ptr->trigger(kRelayPulseMs);
      return true;
    case ActionRelay::None:
    default:
      return false;
  }
}

bool setRelayPressed(ActionRelay relay, bool pressed) {
  switch (relay) {
    case ActionRelay::Up:
      if (pressed) {
        relayDownPtr->setEnabled(false);
      }
      relayUpPtr->setEnabled(pressed);
      return true;
    case ActionRelay::Down:
      if (pressed) {
        relayUpPtr->setEnabled(false);
      }
      relayDownPtr->setEnabled(pressed);
      return true;
    default:
      return false;
  }
}

void applyActionState(const String& action, const String& presetName) {
  ActionRelay relayToTrigger = relayForAction(action);
  if (action == "stop") {
    relayToTrigger = lastTriggeredRelay;
  }

  if (triggerRelay(relayToTrigger) && action != "stop") {
    lastTriggeredRelay = relayToTrigger;
  }

  stateManagerPtr->setRelayState(relayUpPtr->enabled(),
                                 relayDownPtr->enabled(),
                                 relayPreset1Ptr->enabled(),
                                 relayPreset2Ptr->enabled(),
                                 relayPreset3Ptr->enabled());
  stateManagerPtr->setPreset(presetName);
}

String contentTypeForPath(const String& path) {
  if (path.endsWith(".html")) {
    return "text/html";
  }
  if (path.endsWith(".js")) {
    return "application/javascript";
  }
  if (path.endsWith(".css")) {
    return "text/css";
  }
  if (path.endsWith(".json")) {
    return "application/json";
  }
  if (path.endsWith(".svg")) {
    return "image/svg+xml";
  }
  if (path.endsWith(".png")) {
    return "image/png";
  }
  if (path.endsWith(".ico")) {
    return "image/x-icon";
  }
  return "text/plain";
}

bool serveStaticFile(const String& path) {
  if (!LittleFS.exists(path)) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  webServer.streamFile(file, contentTypeForPath(path));
  file.close();
  return true;
}

void serveIndexPage() {
  if (serveStaticFile("/index.html")) {
    return;
  }

  webServer.send(404, "text/plain", "index.html not found");
}

void handleStatus() {
  if (stateManagerPtr == nullptr) {
    webServer.send(500, "application/json", "{\"error\":{\"message\":\"state not initialized\"}}");
    return;
  }

  DeviceSnapshot snapshot = stateManagerPtr->snapshot();
  webServer.send(200, "application/json", makeStatusJson(snapshot));
}

void handlePreset(uint8_t presetNumber) {
  if (presetManagerPtr == nullptr) {
    webServer.send(500, "application/json", "{\"error\":{\"message\":\"preset manager not initialized\"}}");
    return;
  }

  presetManagerPtr->executePreset(presetNumber);
  if (presetNumber == 1) {
    applyActionState("preset1", "Preset 1");
  } else if (presetNumber == 2) {
    applyActionState("preset2", "Preset 2");
  } else if (presetNumber == 3) {
    applyActionState("preset3", "Preset 3");
  } else {
    applyActionState("stop", "Stop");
  }

  webServer.send(200, "application/json",
                 "{\"preset\":" + String(presetNumber) + ",\"status\":\"ok\"}");
}

void handlePresetStore(uint8_t presetNumber) {
  if (relayPreset1Ptr == nullptr || relayPreset2Ptr == nullptr || relayPreset3Ptr == nullptr || stateManagerPtr == nullptr) {
    webServer.send(500, "application/json", "{\"error\":{\"message\":\"relay controller not initialized\"}}");
    return;
  }

  String action = "preset1";
  String presetName = "Store Preset 1";
  ActionRelay relayToTrigger = ActionRelay::Preset1;

  if (presetNumber == 2) {
    action = "preset2";
    presetName = "Store Preset 2";
    relayToTrigger = ActionRelay::Preset2;
  } else if (presetNumber == 3) {
    action = "preset3";
    presetName = "Store Preset 3";
    relayToTrigger = ActionRelay::Preset3;
  }

  switch (relayToTrigger) {
    case ActionRelay::Preset1:
      relayPreset1Ptr->trigger(kPresetStorePulseMs);
      break;
    case ActionRelay::Preset2:
      relayPreset2Ptr->trigger(kPresetStorePulseMs);
      break;
    case ActionRelay::Preset3:
      relayPreset3Ptr->trigger(kPresetStorePulseMs);
      break;
    default:
      break;
  }

  lastTriggeredRelay = relayToTrigger;

  stateManagerPtr->setRelayState(relayUpPtr->enabled(),
                                 relayDownPtr->enabled(),
                                 relayPreset1Ptr->enabled(),
                                 relayPreset2Ptr->enabled(),
                                 relayPreset3Ptr->enabled());
  stateManagerPtr->setPreset(presetName);

  webServer.send(200, "application/json",
                 "{\"action\":\"" + action + "\",\"mode\":\"store\",\"durationMs\":" + String(kPresetStorePulseMs) + ",\"status\":\"ok\"}");
}

void handleRelayToggle() {
  if (relayUpPtr == nullptr || relayDownPtr == nullptr || relayPreset1Ptr == nullptr ||
      relayPreset2Ptr == nullptr || relayPreset3Ptr == nullptr || stateManagerPtr == nullptr) {
    webServer.send(500, "application/json", "{\"error\":{\"message\":\"relay controller not initialized\"}}");
    return;
  }

  String action = actionFromRequestBody();

  if (action == "up") {
    applyActionState("up", "Up");
  } else if (action == "down") {
    applyActionState("down", "Down");
  } else if (action == "preset1") {
    applyActionState("preset1", "Preset 1");
  } else if (action == "preset2") {
    applyActionState("preset2", "Preset 2");
  } else if (action == "preset3") {
    applyActionState("preset3", "Preset 3");
  } else {
    applyActionState("stop", "Stop");
    action = "stop";
  }

  webServer.send(200, "application/json",
                 "{\"action\":\"" + action + "\",\"status\":\"ok\"}");
}

void handleRelayPressRelease(bool pressed) {
  if (relayUpPtr == nullptr || relayDownPtr == nullptr || relayPreset1Ptr == nullptr ||
      relayPreset2Ptr == nullptr || relayPreset3Ptr == nullptr || stateManagerPtr == nullptr) {
    webServer.send(500, "application/json", "{\"error\":{\"message\":\"relay controller not initialized\"}}");
    return;
  }

  const String action = actionFromRequestBody();
  const ActionRelay relay = relayForAction(action);
  if (!setRelayPressed(relay, pressed)) {
    webServer.send(400, "application/json",
                   "{\"error\":{\"message\":\"invalid action, expected up or down\"}}");
    return;
  }

  if (pressed) {
    lastTriggeredRelay = relay;
  }

  stateManagerPtr->setRelayState(relayUpPtr->enabled(),
                                 relayDownPtr->enabled(),
                                 relayPreset1Ptr->enabled(),
                                 relayPreset2Ptr->enabled(),
                                 relayPreset3Ptr->enabled());

  webServer.send(200, "application/json",
                 "{\"action\":\"" + action + "\",\"state\":\"" + String(pressed ? "pressed" : "released") + "\",\"status\":\"ok\"}");
}
}  // namespace

void configureWebServer(DeviceStateManager& stateManager,
                       RelayController& relayUp,
                       RelayController& relayDown,
                       RelayController& relayPreset1,
                       RelayController& relayPreset2,
                       RelayController& relayPreset3,
                       PresetManager& presetManager) {
  stateManagerPtr = &stateManager;
  relayUpPtr = &relayUp;
  relayDownPtr = &relayDown;
  relayPreset1Ptr = &relayPreset1;
  relayPreset2Ptr = &relayPreset2;
  relayPreset3Ptr = &relayPreset3;
  presetManagerPtr = &presetManager;

  webServer.on("/", HTTP_GET, serveIndexPage);
  webServer.on("/status", HTTP_GET, handleStatus);
  webServer.on("/preset/1", HTTP_POST, []() { handlePreset(1); });
  webServer.on("/preset/2", HTTP_POST, []() { handlePreset(2); });
  webServer.on("/preset/3", HTTP_POST, []() { handlePreset(3); });
  webServer.on("/preset/1/store", HTTP_POST, []() { handlePresetStore(1); });
  webServer.on("/preset/2/store", HTTP_POST, []() { handlePresetStore(2); });
  webServer.on("/preset/3/store", HTTP_POST, []() { handlePresetStore(3); });
  webServer.on("/relay/toggle", HTTP_POST, handleRelayToggle);
  webServer.on("/relay/press", HTTP_POST, []() { handleRelayPressRelease(true); });
  webServer.on("/relay/release", HTTP_POST, []() { handleRelayPressRelease(false); });
  webServer.on("/app.js", HTTP_GET, []() {
    if (!serveStaticFile("/app.js")) {
      webServer.send(404, "text/plain", "app.js not found");
    }
  });
  webServer.onNotFound([]() {
    String path = webServer.uri();
    if (path.length() > 0 && serveStaticFile(path)) {
      return;
    }

    serveIndexPage();
  });

  webServer.begin();
  Serial.println("Web server started on port 80");
}

void handleWebServer() {
  webServer.handleClient();
}
