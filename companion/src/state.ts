export interface CompanionState {
  deviceId: string;
  preset: string;
  relay1: boolean;
  relay2: boolean;
  healthy: boolean;
  connected: boolean;
  lastUpdated: number;
}

export const createInitialState = (): CompanionState => ({
  deviceId: 'M5StickC-01',
  preset: 'Default',
  relay1: false,
  relay2: false,
  healthy: true,
  connected: false,
  lastUpdated: Date.now(),
});
