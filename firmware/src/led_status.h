#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <Arduino.h>

enum class WifiLedState : uint8_t {
  Off = 0,
  Connecting,
  Connected,
  FallbackAp,
};

void ledStatusBegin();
void ledStatusSetWifiState(WifiLedState state);
void ledStatusSignalWebRequest();
void ledStatusUpdate();
bool ledStatusAvailable();
const char* ledStatusStateName();

#endif
