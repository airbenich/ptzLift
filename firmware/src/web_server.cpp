#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "led_status.h"
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

String escapeJsonString(const String& value) {
  String escaped = value;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  escaped.replace("\n", "\\n");
  escaped.replace("\r", "\\r");
  return escaped;
}

String jsonStringField(const String& body, const String& fieldName) {
  const String marker = "\"" + fieldName + "\":\"";
  const int fieldStart = body.indexOf(marker);
  if (fieldStart < 0) {
    return "";
  }

  const int valueStart = fieldStart + marker.length();
  const int valueEnd = body.indexOf("\"", valueStart);
  if (valueEnd <= valueStart) {
    return "";
  }

  return body.substring(valueStart, valueEnd);
}

String makeWifiStatusJson() {
  const uint8_t mode = WiFi.getMode();
  String ssid = WiFi.SSID();
  String ipAddress = "";
  if (WiFi.status() == WL_CONNECTED) {
    ipAddress = WiFi.localIP().toString();
  } else if (mode == WIFI_AP) {
    ipAddress = WiFi.softAPIP().toString();
    ssid = WiFi.softAPSSID();
  }

  String savedSsid = "";
  if (LittleFS.exists("/wifi.cfg")) {
    File cfg = LittleFS.open("/wifi.cfg", "r");
    if (cfg) {
      String contents = cfg.readString();
      cfg.close();
      const int newlinePos = contents.indexOf('\n');
      if (newlinePos > 0) {
        savedSsid = contents.substring(0, newlinePos);
      }
    }
  }

  String json = "{";
  json += "\"mode\":\"" + String(mode == WIFI_AP ? "AP" : (mode == WIFI_STA ? "STA" : "OFF")) + "\",";
  json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ssid\":\"" + escapeJsonString(ssid) + "\",";
  json += "\"savedSsid\":\"" + escapeJsonString(savedSsid) + "\",";
  json += "\"ip\":\"" + escapeJsonString(ipAddress) + "\"";
  json += "}";
  return json;
}

String makeStatusJson(const DeviceSnapshot& snapshot) {
  String json = "{";
  json += "\"deviceId\":\"" + snapshot.deviceId + "\",";
  json += "\"boardName\":\"" + String(kBoardName) + "\",";
  json += "\"pinout\":{";
  json += "\"up\":" + String(kRelayUpPin) + ",";
  json += "\"down\":" + String(kRelayDownPin) + ",";
  json += "\"preset1\":" + String(kRelayPreset1Pin) + ",";
  json += "\"preset2\":" + String(kRelayPreset2Pin) + ",";
  json += "\"preset3\":" + String(kRelayPreset3Pin);
  json += "},";
  json += "\"relayState\":{";
  json += "\"up\":" + String(snapshot.relayUpEnabled ? 1 : 0) + ",";
  json += "\"down\":" + String(snapshot.relayDownEnabled ? 1 : 0) + ",";
  json += "\"preset1\":" + String(snapshot.relayPreset1Enabled ? 1 : 0) + ",";
  json += "\"preset2\":" + String(snapshot.relayPreset2Enabled ? 1 : 0) + ",";
  json += "\"preset3\":" + String(snapshot.relayPreset3Enabled ? 1 : 0);
  json += "},";
  json += "\"preset\":\"" + snapshot.preset + "\",";
  json += "\"healthy\":" + String(snapshot.healthy ? "true" : "false") + ",";
  json += "\"companionConnected\":" + String(snapshot.companionConnected ? "true" : "false") + ",";
  json += "\"ledState\":\"" + String(ledStatusStateName()) + "\",";
  json += "\"wifi\":" + makeWifiStatusJson();
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

bool setPresetPressed(ActionRelay relay, bool pressed) {
  switch (relay) {
    case ActionRelay::Preset1:
      relayPreset1Ptr->setEnabled(pressed);
      return true;
    case ActionRelay::Preset2:
      relayPreset2Ptr->setEnabled(pressed);
      return true;
    case ActionRelay::Preset3:
      relayPreset3Ptr->setEnabled(pressed);
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
  ledStatusSignalWebRequest();
  if (stateManagerPtr == nullptr) {
    webServer.send(500, "application/json", "{\"error\":{\"message\":\"state not initialized\"}}");
    return;
  }

  DeviceSnapshot snapshot = stateManagerPtr->snapshot();
  webServer.send(200, "application/json", makeStatusJson(snapshot));
}

void handleWifiStatus() {
  ledStatusSignalWebRequest();
  webServer.send(200, "application/json", makeWifiStatusJson());
}

bool persistWiFiConfig(const String& ssid, const String& password) {
  if (ssid.length() == 0 || password.length() == 0) {
    return false;
  }

  if (!LittleFS.begin(true)) {
    return false;
  }

  File file = LittleFS.open("/wifi.cfg", "w");
  if (!file) {
    return false;
  }

  file.println(ssid);
  file.println(password);
  file.close();
  return true;
}

void handleWifiSave() {
  ledStatusSignalWebRequest();
  const String body = webServer.arg("plain");
  const String ssid = jsonStringField(body, "ssid");
  const String password = jsonStringField(body, "password");

  if (ssid.length() == 0 || password.length() == 0) {
    webServer.send(400, "application/json", "{\"error\":{\"message\":\"ssid and password are required\"}}");
    return;
  }

  if (!persistWiFiConfig(ssid, password)) {
    webServer.send(500, "application/json", "{\"error\":{\"message\":\"failed to store WiFi credentials\"}}");
    return;
  }

  ledStatusSetWifiState(WifiLedState::Connecting);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000UL) {
    ledStatusUpdate();
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    ledStatusSetWifiState(WifiLedState::Connected);
    webServer.send(200, "application/json",
                   "{\"status\":\"saved_and_connected\",\"ssid\":\"" + escapeJsonString(ssid) + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}");
    return;
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(kAccessPointSsid);
  ledStatusSetWifiState(WifiLedState::FallbackAp);
  webServer.send(200, "application/json",
                 "{\"status\":\"saved_but_ap_fallback\",\"ssid\":\"" + escapeJsonString(ssid) + "\",\"ap\":\"" + escapeJsonString(kAccessPointSsid) + "\"}");
}

void handlePreset(uint8_t presetNumber) {
  ledStatusSignalWebRequest();
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
  ledStatusSignalWebRequest();
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
  ledStatusSignalWebRequest();
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
  ledStatusSignalWebRequest();
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

void handlePresetPressRelease(bool pressed) {
  ledStatusSignalWebRequest();
  if (relayPreset1Ptr == nullptr || relayPreset2Ptr == nullptr || relayPreset3Ptr == nullptr || stateManagerPtr == nullptr) {
    webServer.send(500, "application/json", "{\"error\":{\"message\":\"relay controller not initialized\"}}");
    return;
  }

  const String action = actionFromRequestBody();
  const ActionRelay relay = relayForAction(action);
  if (!setPresetPressed(relay, pressed)) {
    webServer.send(400, "application/json",
                   "{\"error\":{\"message\":\"invalid action, expected preset1, preset2 or preset3\"}}");
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

  webServer.on("/", HTTP_GET, []() {
    ledStatusSignalWebRequest();
    serveIndexPage();
  });
  webServer.on("/status", HTTP_GET, handleStatus);
  webServer.on("/wifi/status", HTTP_GET, handleWifiStatus);
  webServer.on("/wifi/save", HTTP_POST, handleWifiSave);
  webServer.on("/preset/1", HTTP_POST, []() { handlePreset(1); });
  webServer.on("/preset/2", HTTP_POST, []() { handlePreset(2); });
  webServer.on("/preset/3", HTTP_POST, []() { handlePreset(3); });
  webServer.on("/preset/1/store", HTTP_POST, []() { handlePresetStore(1); });
  webServer.on("/preset/2/store", HTTP_POST, []() { handlePresetStore(2); });
  webServer.on("/preset/3/store", HTTP_POST, []() { handlePresetStore(3); });
  webServer.on("/relay/toggle", HTTP_POST, handleRelayToggle);
  webServer.on("/relay/press", HTTP_POST, []() { handleRelayPressRelease(true); });
  webServer.on("/relay/release", HTTP_POST, []() { handleRelayPressRelease(false); });
  webServer.on("/preset/press", HTTP_POST, []() { handlePresetPressRelease(true); });
  webServer.on("/preset/release", HTTP_POST, []() { handlePresetPressRelease(false); });
  webServer.on("/app.js", HTTP_GET, []() {
    ledStatusSignalWebRequest();
    if (!serveStaticFile("/app.js")) {
      webServer.send(404, "text/plain", "app.js not found");
    }
  });
  webServer.onNotFound([]() {
    ledStatusSignalWebRequest();
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
