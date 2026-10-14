export interface RelayState {
  relay1: boolean;
  relay2: boolean;
}

export interface DeviceStatus {
  deviceId: string;
  relayState: RelayState;
  preset: string;
  healthy: boolean;
  companionConnected: boolean;
}

export interface PresetResponse {
  preset: number;
  status: 'ok';
}

export interface RelayToggleResponse {
  relayId: 'relay1' | 'relay2';
  enabled: boolean;
  status: 'ok';
}

export class M5StickClient {
  constructor(private readonly baseUrl: string) {}

  async getStatus(): Promise<DeviceStatus> {
    const response = await fetch(`${this.baseUrl}/status`);
    if (!response.ok) {
      throw new Error(`Device status request failed: ${response.status}`);
    }
    return (await response.json()) as DeviceStatus;
  }

  async triggerPreset(preset: number): Promise<PresetResponse> {
    const response = await fetch(`${this.baseUrl}/preset/${preset}`, { method: 'POST' });
    if (!response.ok) {
      throw new Error(`Preset ${preset} request failed: ${response.status}`);
    }
    return (await response.json()) as PresetResponse;
  }

  async toggleRelay(relayId: 'relay1' | 'relay2', enabled: boolean): Promise<RelayToggleResponse> {
    const response = await fetch(`${this.baseUrl}/relay/toggle`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ relayId, enabled }),
    });

    if (!response.ok) {
      throw new Error(`Relay toggle request failed: ${response.status}`);
    }

    return (await response.json()) as RelayToggleResponse;
  }
}

export const createDeviceClient = (baseUrl = process.env.DEVICE_HOST ?? 'http://192.168.1.50') =>
  new M5StickClient(baseUrl);
