#include <Arduino.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "preset.h"
#include "relay.h"
#include "state.h"
#include "web_server.h"

constexpr uint8_t kRelayUpPin = 26;
constexpr uint8_t kRelayDownPin = 27;
constexpr uint8_t kRelayPreset1Pin = 25;
constexpr uint8_t kRelayPreset2Pin = 32;
constexpr uint8_t kRelayPreset3Pin = 33;
constexpr uint32_t kUpDownMaxOnMs = 25000; // max time that the pedestal is driving
constexpr char kDeviceId[] = "PTZ-Pedestal-Control";

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

constexpr int kHeaderY = 0;
constexpr int kHeaderH = 12;
constexpr int kContentTop = 14;
constexpr int kLineStep = 10;

String clipText(const String& text, int maxChars) {
  if (maxChars <= 0 || static_cast<int>(text.length()) <= maxChars) {
    return text;
  }
  if (maxChars <= 3) {
    return text.substring(0, maxChars);
  }
  return text.substring(0, maxChars - 3) + "...";
}

void drawPageHeader(const char* title, bool healthy) {
  const int w = M5.Lcd.width();

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.fillRect(0, kHeaderY, w, kHeaderH, TFT_DARKGREY);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.drawString(title, 2, 2);

  M5.Lcd.setTextColor(healthy ? TFT_GREEN : TFT_RED, TFT_DARKGREY);
  M5.Lcd.setTextDatum(TR_DATUM);
  M5.Lcd.drawString(healthy ? "OK" : "WARN", w - 2, 2);

  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Lcd.setTextDatum(TR_DATUM);
  M5.Lcd.drawString(String("P") + String(static_cast<int>(currentPage) + 1) + "/" + String(static_cast<int>(DisplayPage::Count)), w - 2, kContentTop);
}

void renderOverviewPage(const DeviceSnapshot& snapshot, const String& wifiState, const String& wifiSsid, const String& wifiIp) {
  drawPageHeader("PTZ Pedestal – Overview", snapshot.healthy);

  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextDatum(TL_DATUM);
  M5.Lcd.setCursor(2, kContentTop + 0 * kLineStep);
  M5.Lcd.printf("Last Action: %s", snapshot.preset.c_str());

  const String clippedSsid = clipText(wifiSsid, 12);
  M5.Lcd.setCursor(2, kContentTop + 2 * kLineStep);
  M5.Lcd.printf("SSID: %s", clippedSsid.c_str());

  M5.Lcd.setCursor(2, kContentTop + 3 * kLineStep);
  M5.Lcd.printf("IP: %s", wifiIp.c_str());
}

void renderNetworkPage(const DeviceSnapshot& snapshot, const String& wifiState, const String& wifiSsid, const String& wifiIp) {
  drawPageHeader("Network", snapshot.healthy);

  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setTextSize(1);
  const String clippedSsid = clipText(wifiSsid, 12);
  const String clippedDevice = clipText(snapshot.deviceId, 10);

  M5.Lcd.setCursor(2, kContentTop + 0 * kLineStep);
  M5.Lcd.printf("Status: %s", wifiState.c_str());

  M5.Lcd.setCursor(2, kContentTop + 1 * kLineStep);
  M5.Lcd.printf("SSID: %s", clippedSsid.c_str());

  M5.Lcd.setCursor(2, kContentTop + 2 * kLineStep);
  M5.Lcd.printf("IP: %s", wifiIp.c_str());

  M5.Lcd.setCursor(2, kContentTop + 4 * kLineStep);
  M5.Lcd.printf("Device: %s", clippedDevice.c_str());

  M5.Lcd.setCursor(2, kContentTop + 5 * kLineStep);
  M5.Lcd.printf("Companion: %s", snapshot.companionConnected ? "connected" : "offline");
}

void renderRelayPage(const DeviceSnapshot& snapshot) {
  drawPageHeader("Relays", snapshot.healthy);

  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(2, kContentTop + 0 * kLineStep);
  M5.Lcd.printf("Up: %s", snapshot.relayUpEnabled ? "ON" : "OFF");

  M5.Lcd.setCursor(2, kContentTop + 1 * kLineStep);
  M5.Lcd.printf("Down: %s", snapshot.relayDownEnabled ? "ON" : "OFF");

  M5.Lcd.setCursor(2, kContentTop + 2 * kLineStep);
  M5.Lcd.printf("P1: %s", snapshot.relayPreset1Enabled ? "ON" : "OFF");

  M5.Lcd.setCursor(2, kContentTop + 3 * kLineStep);
  M5.Lcd.printf("P2: %s", snapshot.relayPreset2Enabled ? "ON" : "OFF");

  M5.Lcd.setCursor(2, kContentTop + 4 * kLineStep);
  M5.Lcd.printf("P3: %s", snapshot.relayPreset3Enabled ? "ON" : "OFF");

  M5.Lcd.setCursor(2, kContentTop + 5 * kLineStep);
  M5.Lcd.printf("Last Action: %s", snapshot.preset.c_str());
}

void renderStatusDisplay(bool force = false) {
  static String lastDeviceId;
  static String lastWifiState;
  static String lastWifiIp;
  static String lastWifiSsid;
  static String lastPreset;
  static bool lastRelayUp = false;
  static bool lastRelayDown = false;
  static bool lastRelayPreset1 = false;
  static bool lastRelayPreset2 = false;
  static bool lastRelayPreset3 = false;
  static bool lastHealthy = false;
  static bool lastCompanionConnected = false;
  static DisplayPage lastPage = currentPage;
  static bool initialized = false;

  const DeviceSnapshot snapshot = stateManager.snapshot();
  String wifiState = WiFi.status() == WL_CONNECTED ? "WiFi" : "Offline";
  String wifiIp = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "N/A";
  String wifiSsid = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String(kWifiSsid);

  bool changed = force || !initialized || lastDeviceId != snapshot.deviceId ||
                 lastWifiState != wifiState || lastWifiIp != wifiIp ||
                 lastWifiSsid != wifiSsid || lastPreset != snapshot.preset ||
                 lastRelayUp != snapshot.relayUpEnabled ||
                 lastRelayDown != snapshot.relayDownEnabled ||
                 lastRelayPreset1 != snapshot.relayPreset1Enabled ||
                 lastRelayPreset2 != snapshot.relayPreset2Enabled ||
                 lastRelayPreset3 != snapshot.relayPreset3Enabled ||
                 lastHealthy != snapshot.healthy || lastCompanionConnected != snapshot.companionConnected ||
                 lastPage != currentPage;
  if (!changed) {
    return;
  }

  initialized = true;
  lastDeviceId = snapshot.deviceId;
  lastWifiState = wifiState;
  lastWifiIp = wifiIp;
  lastWifiSsid = wifiSsid;
  lastPreset = snapshot.preset;
  lastRelayUp = snapshot.relayUpEnabled;
  lastRelayDown = snapshot.relayDownEnabled;
  lastRelayPreset1 = snapshot.relayPreset1Enabled;
  lastRelayPreset2 = snapshot.relayPreset2Enabled;
  lastRelayPreset3 = snapshot.relayPreset3Enabled;
  lastHealthy = snapshot.healthy;
  lastCompanionConnected = snapshot.companionConnected;
  lastPage = currentPage;

  switch (currentPage) {
    case DisplayPage::Overview:
      renderOverviewPage(snapshot, wifiState, wifiSsid, wifiIp);
      break;
    case DisplayPage::Network:
      renderNetworkPage(snapshot, wifiState, wifiSsid, wifiIp);
      break;
    case DisplayPage::Relays:
      renderRelayPage(snapshot);
      break;
    default:
      renderOverviewPage(snapshot, wifiState, wifiSsid, wifiIp);
      break;
  }
}

void handleDisplayNavigation() {
  if (M5.BtnPWR.wasClicked()) {
    if (currentPage == DisplayPage::Overview) {
      currentPage = DisplayPage::Relays;
    } else {
      currentPage = static_cast<DisplayPage>(static_cast<int>(currentPage) - 1);
    }
    renderStatusDisplay(true);
  }

  if (M5.BtnB.wasPressed()) {
    if (currentPage == DisplayPage::Relays) {
      currentPage = DisplayPage::Overview;
    } else {
      currentPage = static_cast<DisplayPage>(static_cast<int>(currentPage) + 1);
    }
    renderStatusDisplay(true);
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(0, 0);
  // print connecting message to the display with ssid
  M5.Lcd.println("Connecting to WiFi: \n" + String(kWifiSsid));

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

  Serial.println("M5StickC preset controller ready");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi SSID: " + String(kWifiSsid));
    Serial.println("WiFi IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("AP SSID: m5stick-local");
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
  }

  renderStatusDisplay(true);
}

void loop() {
  handleWebServer();
  M5.update();
  relayUp.update();
  relayDown.update();
  relayPreset1.update();
  relayPreset2.update();
  relayPreset3.update();
  // Keep shared device state aligned with the real relay outputs.
  stateManager.setRelayState(relayUp.enabled(),
                             relayDown.enabled(),
                             relayPreset1.enabled(),
                             relayPreset2.enabled(),
                             relayPreset3.enabled());
  handleDisplayNavigation();
  renderStatusDisplay();
  delay(60);
}
