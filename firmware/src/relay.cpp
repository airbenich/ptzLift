#include "relay.h"

RelayController::RelayController()
    : pin_(0),
      enabled_(false),
      pulsing_(false),
      pulseStartedAtMs_(0),
      pulseDurationMs_(0),
  autoOffTimeoutMs_(0),
  enabledSinceMs_(0),
      name_("relay") {}

void RelayController::begin(uint8_t pin, const char* name) {
  pin_ = pin;
  name_ = name;
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
  enabled_ = false;
  pulsing_ = false;
  pulseStartedAtMs_ = 0;
  pulseDurationMs_ = 0;
  enabledSinceMs_ = 0;
}

bool RelayController::enabled() const {
  return enabled_;
}

void RelayController::setEnabled(bool enabled) {
  pulsing_ = false;
  enabled_ = enabled;
  if (enabled_) {
    enabledSinceMs_ = millis();
  } else {
    enabledSinceMs_ = 0;
  }
  digitalWrite(pin_, enabled_ ? HIGH : LOW);
}

void RelayController::setAutoOffTimeoutMs(uint32_t timeoutMs) {
  autoOffTimeoutMs_ = timeoutMs;
}

void RelayController::trigger(uint16_t pulseMs) {
  pulseDurationMs_ = pulseMs;
  pulseStartedAtMs_ = millis();
  pulsing_ = true;
  enabled_ = true;
  enabledSinceMs_ = pulseStartedAtMs_;
  digitalWrite(pin_, HIGH);
}

void RelayController::update() {
  const uint32_t now = millis();

  if (!pulsing_) {
    if (enabled_ && autoOffTimeoutMs_ > 0 && enabledSinceMs_ > 0 && now - enabledSinceMs_ >= autoOffTimeoutMs_) {
      setEnabled(false);
    }
    return;
  }

  if (now - pulseStartedAtMs_ >= pulseDurationMs_) {
    pulsing_ = false;
    enabled_ = false;
    enabledSinceMs_ = 0;
    digitalWrite(pin_, LOW);
  }
}

void RelayController::toggle() {
  setEnabled(!enabled_);
}

const char* RelayController::name() const {
  return name_;
}
