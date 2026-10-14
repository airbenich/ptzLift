#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

struct DeviceSnapshot {
  String deviceId;
  bool healthy;
  bool relayUpEnabled;
  bool relayDownEnabled;
  bool relayPreset1Enabled;
  bool relayPreset2Enabled;
  bool relayPreset3Enabled;
  String preset;
  bool companionConnected;
};

class DeviceStateManager {
 public:
  DeviceStateManager();
  void begin(const String& deviceId);
  DeviceSnapshot snapshot() const;
  void setRelayState(bool relayUpEnabled,
                     bool relayDownEnabled,
                     bool relayPreset1Enabled,
                     bool relayPreset2Enabled,
                     bool relayPreset3Enabled);
  void setPreset(const String& preset);
  void setCompanionConnected(bool connected);
  void setHealthy(bool healthy);

 private:
  String deviceId_;
  bool healthy_;
  bool relayUpEnabled_;
  bool relayDownEnabled_;
  bool relayPreset1Enabled_;
  bool relayPreset2Enabled_;
  bool relayPreset3Enabled_;
  String preset_;
  bool companionConnected_;
};

#endif
