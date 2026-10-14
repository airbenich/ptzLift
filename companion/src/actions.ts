import { M5StickClient, PresetResponse, RelayToggleResponse } from './client.js';

export async function triggerPreset(
  client: M5StickClient,
  preset: number,
): Promise<PresetResponse> {
  return client.triggerPreset(preset);
}

export async function toggleRelay(
  client: M5StickClient,
  relayId: 'relay1' | 'relay2',
  enabled: boolean,
): Promise<RelayToggleResponse> {
  return client.toggleRelay(relayId, enabled);
}
