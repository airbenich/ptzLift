#ifndef RELAY_H
#define RELAY_H

#include <Arduino.h>

class RelayController {
 public:
  RelayController();
  void begin(uint8_t pin, const char* name);
  bool enabled() const;
  void setEnabled(bool enabled);
  void setAutoOffTimeoutMs(uint32_t timeoutMs);
  void trigger(uint16_t pulseMs = 150);
  void update();
  void toggle();
  const char* name() const;

 private:
  uint8_t pin_;
  bool enabled_;
  bool pulsing_;
  uint32_t pulseStartedAtMs_;
  uint16_t pulseDurationMs_;
  uint32_t autoOffTimeoutMs_;
  uint32_t enabledSinceMs_;
  const char* name_;
};

#endif
