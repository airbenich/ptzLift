#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

constexpr uint8_t kRelayUpPin = 32;
constexpr uint8_t kRelayDownPin = 33;
constexpr uint8_t kRelayPreset1Pin = 25;
constexpr uint8_t kRelayPreset2Pin = 26;
constexpr uint8_t kRelayPreset3Pin = 27;
constexpr uint8_t kManualUpSwitchPin = 21;
constexpr uint8_t kManualDownSwitchPin = 22;

#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN -1
#endif

#ifndef STATUS_LED_ACTIVE_HIGH
#define STATUS_LED_ACTIVE_HIGH 1
#endif

constexpr int8_t kBuiltInLedPin = STATUS_LED_PIN;
constexpr bool kBuiltInLedActiveHigh = STATUS_LED_ACTIVE_HIGH != 0;
constexpr uint32_t kUpDownMaxOnMs = 25000;
constexpr char kDeviceId[] = "PTZ-Lift-Control";
constexpr char kBoardName[] = "ESP32 DevKitC V4";

#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID "ptzlift-local"
#endif

constexpr char kAccessPointSsid[] = WIFI_AP_SSID;

#endif
