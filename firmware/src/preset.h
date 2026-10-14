#ifndef PRESET_H
#define PRESET_H

#include <Arduino.h>

class PresetManager {
 public:
  PresetManager();
  void begin();
  uint8_t currentPreset() const;
  const char* currentPresetName() const;
  const char* presetName(uint8_t index) const;
  void executePreset(uint8_t index);

 private:
  uint8_t currentPreset_;
};

#endif
