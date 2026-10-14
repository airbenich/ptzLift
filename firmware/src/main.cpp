#include <Arduino.h>
#include <LittleFS.h>

#include <WiFi.h>

#include "config.h"
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

constexpr char kWifiSsid[] = WIFI_SSID;
constexpr char kWifiPassword[] = WIFI_PASSWORD;

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
  Serial.println("Connecting to WiFi: " + String(kWifiSsid));

  WiFi.mode(WIFI_STA);
  WiFi.begin(kWifiSsid, kWifiPassword);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000UL) {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("Connected to WiFi network");
    Serial.println("IP address: " + WiFi.localIP().toString());
  } else {
    Serial.println();
    Serial.println("WiFi connection failed, falling back to AP mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("m5stick-local", "m5stickpass");
  }

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }

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
  stateManager.setCompanionConnected(true);

  configureWebServer(stateManager,
                    relayUp,
                    relayDown,
                    relayPreset1,
                    relayPreset2,
                    relayPreset3,
                    presetManager);

  Serial.println("ESP32 DevKitC V4 relay controller ready");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi SSID: " + String(kWifiSsid));
    Serial.println("WiFi IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("AP SSID: ptzpedestal-local");
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
  }

  renderStatusDisplay(true);
}

void loop() {
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
