#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

constexpr uint8_t kRelayUpPin = 32;
constexpr uint8_t kRelayDownPin = 33;
constexpr uint8_t kRelayPreset1Pin = 25;
constexpr uint8_t kRelayPreset2Pin = 26;
constexpr uint8_t kRelayPreset3Pin = 27;
constexpr uint32_t kUpDownMaxOnMs = 25000;
constexpr char kDeviceId[] = "PTZ-Pedestal-Control";
constexpr char kBoardName[] = "ESP32 DevKitC V4";

#endif
