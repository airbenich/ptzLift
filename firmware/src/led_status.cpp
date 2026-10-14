#include "led_status.h"

#include "config.h"

namespace {
WifiLedState wifiState = WifiLedState::Off;
unsigned long requestOverlayUntilMs = 0;
bool ledAvailable = false;
uint16_t lastWrittenBrightness = 256;

constexpr unsigned long kRequestPulseMs = 90;
constexpr uint8_t kLedMaxBrightness = 255;
constexpr uint16_t kLedPwmFrequencyHz = 5000;
constexpr uint8_t kLedPwmResolutionBits = 8;

void writeLedBrightness(uint8_t brightness) {
  if (!ledAvailable) {
    return;
  }

  if (brightness == lastWrittenBrightness) {
    return;
  }

  const uint8_t physicalBrightness = kBuiltInLedActiveHigh
                                         ? brightness
                                         : static_cast<uint8_t>(kLedMaxBrightness - brightness);
  ledcWrite(kBuiltInLedPin, physicalBrightness);
  lastWrittenBrightness = brightness;
}

uint8_t breathingBrightness(unsigned long nowMs,
                           unsigned long periodMs,
                           uint8_t minBrightness,
                           uint8_t maxBrightness) {
  if (periodMs < 2 || maxBrightness <= minBrightness) {
    return maxBrightness;
  }

  const unsigned long halfPeriod = periodMs / 2;
  const unsigned long phase = nowMs % periodMs;
  const unsigned long ramp = phase < halfPeriod ? phase : (periodMs - phase);
  const uint16_t span = static_cast<uint16_t>(maxBrightness - minBrightness);
  return static_cast<uint8_t>(minBrightness + (span * ramp) / halfPeriod);
}

uint8_t connectingPattern(unsigned long nowMs) {
  return breathingBrightness(nowMs, 1400UL, 0, 90);
}

uint8_t connectedPattern(unsigned long nowMs) {
  return breathingBrightness(nowMs, 2800UL, 4, 55);
}

uint8_t fallbackPattern(unsigned long nowMs) {
  const unsigned long phase = nowMs % 1400UL;
  return (phase < 120UL || (phase >= 240UL && phase < 360UL)) ? kLedMaxBrightness : 0;
}

uint8_t desiredBaseBrightness(unsigned long nowMs) {
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
    ledAvailable = ledcAttach(kBuiltInLedPin, kLedPwmFrequencyHz, kLedPwmResolutionBits);
  }
  writeLedBrightness(0);
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
  uint8_t targetBrightness = desiredBaseBrightness(nowMs);

  if (nowMs < requestOverlayUntilMs) {
    targetBrightness = kLedMaxBrightness;
  }

  writeLedBrightness(targetBrightness);
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
