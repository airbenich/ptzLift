#include "preset.h"

PresetManager::PresetManager() : currentPreset_(1) {}

void PresetManager::begin() {
  currentPreset_ = 1;
}

uint8_t PresetManager::currentPreset() const {
  return currentPreset_;
}

const char* PresetManager::currentPresetName() const {
  return presetName(currentPreset_);
}

const char* PresetManager::presetName(uint8_t index) const {
  switch (index) {
    case 1:
      return "Default";
    case 2:
      return "Secondary";
    case 3:
      return "Safety Halt";
    default:
      return "Unknown";
  }
}

void PresetManager::executePreset(uint8_t index) {
  currentPreset_ = index;
  Serial.printf("Preset %u executed: %s\n", index, presetName(index));
}
