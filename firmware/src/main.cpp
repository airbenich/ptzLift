#include <Arduino.h>
#include <LittleFS.h>

#include <WiFi.h>

#include "config.h"
#include "led_status.h"
#include "preset.h"
#include "relay.h"
#include "state.h"
#include "web_server.h"

#ifndef WIFI_SSID
#define WIFI_SSID "CHANGE_ME_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "CHANGE_ME_PASSWORD"
#endif

constexpr char kDefaultWifiSsid[] = WIFI_SSID;
constexpr char kDefaultWifiPassword[] = WIFI_PASSWORD;
constexpr char kSavedWifiConfigPath[] = "/wifi.cfg";

struct WifiConfig {
  String ssid;
  String password;
};

String trimConfigurationValue(const String& value) {
  String trimmed = value;
  trimmed.trim();
  return trimmed;
}

bool loadSavedWifiConfig(WifiConfig& config) {
  config.ssid = String(kDefaultWifiSsid);
  config.password = String(kDefaultWifiPassword);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed during WiFi config read");
    return false;
  }

  if (!LittleFS.exists(kSavedWifiConfigPath)) {
    Serial.println("No saved WiFi config found; using built-in defaults");
    return true;
  }

  File file = LittleFS.open(kSavedWifiConfigPath, "r");
  if (!file) {
    Serial.println("Could not open saved WiFi config file; using defaults");
    return true;
  }

  String contents = file.readString();
  file.close();

  const int newlinePos = contents.indexOf('\n');
  if (newlinePos > 0) {
    config.ssid = trimConfigurationValue(contents.substring(0, newlinePos));
    config.password = trimConfigurationValue(contents.substring(newlinePos + 1));
  }

  if (config.ssid.length() == 0) {
    config.ssid = String(kDefaultWifiSsid);
  }
  if (config.password.length() == 0) {
    config.password = String(kDefaultWifiPassword);
  }

  return true;
}

bool saveWifiConfig(const String& ssid, const String& password) {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed during WiFi config save");
    return false;
  }

  File file = LittleFS.open(kSavedWifiConfigPath, "w");
  if (!file) {
    Serial.println("Could not write WiFi config file");
    return false;
  }

  file.println(ssid);
  file.println(password);
  file.close();
  Serial.println("WiFi config saved to LittleFS");
  return true;
}

bool connectToConfiguredWifi(const WifiConfig& config) {
  ledStatusSetWifiState(WifiLedState::Connecting);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.ssid.c_str(), config.password.c_str());

  Serial.println("Connecting to WiFi: " + config.ssid);
  const unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000UL) {
    ledStatusUpdate();
    delay(50);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    ledStatusSetWifiState(WifiLedState::Connected);
    Serial.println();
    Serial.println("Connected to WiFi network");
    Serial.println("IP address: " + WiFi.localIP().toString());
    return true;
  }

  Serial.println();
  Serial.println("WiFi connection failed, falling back to AP mode");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kAccessPointSsid);
  ledStatusSetWifiState(WifiLedState::FallbackAp);
  return false;
}

RelayController relayUp;
RelayController relayDown;
RelayController relayPreset1;
RelayController relayPreset2;
RelayController relayPreset3;
PresetManager presetManager;
DeviceStateManager stateManager;

namespace {
enum class DisplayPage : uint8_t {
  Overview = 0,
  Network = 1,
  Relays = 2,
  Count = 3,
};

DisplayPage currentPage = DisplayPage::Overview;

void renderStatusDisplay(bool force = false) {
  (void)force;
  const DeviceSnapshot snapshot = stateManager.snapshot();
  Serial.printf("Device=%s | LastAction=%s | Up=%s | Down=%s | P1=%s | P2=%s | P3=%s\n",
                snapshot.deviceId.c_str(),
                snapshot.preset.c_str(),
                snapshot.relayUpEnabled ? "ON" : "OFF",
                snapshot.relayDownEnabled ? "ON" : "OFF",
                snapshot.relayPreset1Enabled ? "ON" : "OFF",
                snapshot.relayPreset2Enabled ? "ON" : "OFF",
                snapshot.relayPreset3Enabled ? "ON" : "OFF");
}

void handleDisplayNavigation() {
  // No display or physical buttons on the ESP32 DevKitC V4 target.
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 DevKitC V4 without display configured.");
  ledStatusBegin();
  ledStatusSetWifiState(WifiLedState::Connecting);
  if (ledStatusAvailable()) {
    Serial.println("Status LED enabled on GPIO" + String(kBuiltInLedPin));
  } else {
    Serial.println("Status LED unavailable on this board configuration (STATUS_LED_PIN < 0)");
  }

  WifiConfig wifiConfig;
  loadSavedWifiConfig(wifiConfig);
  connectToConfiguredWifi(wifiConfig);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }

  pinMode(kManualUpSwitchPin, INPUT_PULLUP);
  pinMode(kManualDownSwitchPin, INPUT_PULLUP);

  relayUp.begin(kRelayUpPin, "Relay Up");
  relayDown.begin(kRelayDownPin, "Relay Down");
  relayUp.setAutoOffTimeoutMs(kUpDownMaxOnMs);
  relayDown.setAutoOffTimeoutMs(kUpDownMaxOnMs);
  relayPreset1.begin(kRelayPreset1Pin, "Relay Preset 1");
  relayPreset2.begin(kRelayPreset2Pin, "Relay Preset 2");
  relayPreset3.begin(kRelayPreset3Pin, "Relay Preset 3");
  presetManager.begin();
  stateManager.begin(kDeviceId);
  stateManager.setRelayState(relayUp.enabled(),
                             relayDown.enabled(),
                             relayPreset1.enabled(),
                             relayPreset2.enabled(),
                             relayPreset3.enabled());
  stateManager.setPreset(presetManager.currentPresetName());
  stateManager.setHealthy(true);

  configureWebServer(stateManager,
                    relayUp,
                    relayDown,
                    relayPreset1,
                    relayPreset2,
                    relayPreset3,
                    presetManager);

  Serial.println("ESP32 DevKitC V4 relay controller ready");
  if (WiFi.status() == WL_CONNECTED) {
    ledStatusSetWifiState(WifiLedState::Connected);
    Serial.println("WiFi SSID: " + WiFi.SSID());
    Serial.println("WiFi IP: " + WiFi.localIP().toString());
  } else {
    ledStatusSetWifiState(WifiLedState::FallbackAp);
    Serial.println("AP SSID: " + String(kAccessPointSsid));
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
  }
  Serial.println("LED WiFi state: " + String(ledStatusStateName()));

  renderStatusDisplay(true);
}

void loop() {
  const bool manualUpPressed = (digitalRead(kManualUpSwitchPin) == LOW);
  const bool manualDownPressed = (digitalRead(kManualDownSwitchPin) == LOW);

  if (manualUpPressed && !manualDownPressed) {
    relayUp.setEnabled(true);
    relayDown.setEnabled(false);
  } else if (manualDownPressed && !manualUpPressed) {
    relayUp.setEnabled(false);
    relayDown.setEnabled(true);
  } else {
    relayUp.setEnabled(false);
    relayDown.setEnabled(false);
  }

  ledStatusUpdate();
  handleWebServer();
  relayUp.update();
  relayDown.update();
  relayPreset1.update();
  relayPreset2.update();
  relayPreset3.update();
  stateManager.setRelayState(relayUp.enabled(),
                             relayDown.enabled(),
                             relayPreset1.enabled(),
                             relayPreset2.enabled(),
                             relayPreset3.enabled());
  renderStatusDisplay();
  delay(60);
}
