#include "state.h"

DeviceStateManager::DeviceStateManager()
    : deviceId_("M5StickC"),
      healthy_(true),
      relayUpEnabled_(false),
      relayDownEnabled_(false),
      relayPreset1Enabled_(false),
      relayPreset2Enabled_(false),
      relayPreset3Enabled_(false),
      preset_("Default") {
}

void DeviceStateManager::begin(const String& deviceId) {
  deviceId_ = deviceId;
}

DeviceSnapshot DeviceStateManager::snapshot() const {
  DeviceSnapshot snapshot;
  snapshot.deviceId = deviceId_;
  snapshot.healthy = healthy_;
  snapshot.relayUpEnabled = relayUpEnabled_;
  snapshot.relayDownEnabled = relayDownEnabled_;
  snapshot.relayPreset1Enabled = relayPreset1Enabled_;
  snapshot.relayPreset2Enabled = relayPreset2Enabled_;
  snapshot.relayPreset3Enabled = relayPreset3Enabled_;
  snapshot.preset = preset_;
  return snapshot;
}

void DeviceStateManager::setRelayState(bool relayUpEnabled,
                                       bool relayDownEnabled,
                                       bool relayPreset1Enabled,
                                       bool relayPreset2Enabled,
                                       bool relayPreset3Enabled) {
  relayUpEnabled_ = relayUpEnabled;
  relayDownEnabled_ = relayDownEnabled;
  relayPreset1Enabled_ = relayPreset1Enabled;
  relayPreset2Enabled_ = relayPreset2Enabled;
  relayPreset3Enabled_ = relayPreset3Enabled;
}

void DeviceStateManager::setPreset(const String& preset) {
  preset_ = preset;
}

void DeviceStateManager::setHealthy(bool healthy) {
  healthy_ = healthy;
}
