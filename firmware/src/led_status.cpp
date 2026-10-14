#include "led_status.h"

#include "config.h"

namespace {
WifiLedState wifiState = WifiLedState::Off;
unsigned long requestOverlayUntilMs = 0;
bool outputState = false;
bool ledAvailable = false;

constexpr unsigned long kRequestPulseMs = 90;

void writeLed(bool on) {
  if (!ledAvailable) {
    return;
  }
  const bool physicalLevel = kBuiltInLedActiveHigh ? on : !on;
  digitalWrite(kBuiltInLedPin, physicalLevel ? HIGH : LOW);
}

bool connectingPattern(unsigned long nowMs) {
  return (nowMs % 1000UL) < 500UL;
}

bool connectedPattern(unsigned long nowMs) {
  return (nowMs % 2000UL) < 80UL;
}

bool fallbackPattern(unsigned long nowMs) {
  const unsigned long phase = nowMs % 1400UL;
  return phase < 120UL || (phase >= 240UL && phase < 360UL);
}

bool desiredBaseState(unsigned long nowMs) {
  switch (wifiState) {
    case WifiLedState::Connecting:
      return connectingPattern(nowMs);
    case WifiLedState::Connected:
      return connectedPattern(nowMs);
    case WifiLedState::FallbackAp:
      return fallbackPattern(nowMs);
    case WifiLedState::Off:
    default:
      return false;
  }
}
}  // namespace

void ledStatusBegin() {
  ledAvailable = kBuiltInLedPin >= 0;
  if (ledAvailable) {
    pinMode(kBuiltInLedPin, OUTPUT);
  }
  writeLed(false);
  outputState = false;
  requestOverlayUntilMs = 0;
}

void ledStatusSetWifiState(WifiLedState state) {
  wifiState = state;
}

void ledStatusSignalWebRequest() {
  if (!ledAvailable) {
    return;
  }
  requestOverlayUntilMs = millis() + kRequestPulseMs;
}

void ledStatusUpdate() {
  if (!ledAvailable) {
    return;
  }

  const unsigned long nowMs = millis();
  bool targetState = desiredBaseState(nowMs);

  if (nowMs < requestOverlayUntilMs) {
    targetState = true;
  }

  if (targetState != outputState) {
    outputState = targetState;
    writeLed(outputState);
  }
}

bool ledStatusAvailable() {
  return ledAvailable;
}

const char* ledStatusStateName() {
  if (!ledAvailable) {
    return "unavailable";
  }

  switch (wifiState) {
    case WifiLedState::Connecting:
      return "connecting";
    case WifiLedState::Connected:
      return "connected";
    case WifiLedState::FallbackAp:
      return "fallback_ap";
    case WifiLedState::Off:
    default:
      return "off";
  }
}
