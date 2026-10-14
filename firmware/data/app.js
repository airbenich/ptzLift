const PRESET_MIN = 1;
const PRESET_MAX = 3;

function setTextIfPresent(id, value) {
  const element = document.getElementById(id);
  if (!element) {
    return;
  }
  element.textContent = value;
}

async function fetchStatus() {
  try {
    const response = await fetch('/status');
    if (!response.ok) {
      return;
    }

    const payload = await response.json();
    const pinout = payload.pinout || {};

    setTextIfPresent('deviceId', payload.deviceId || 'unknown');
    setTextIfPresent('pinUp', pinout.up !== undefined ? `GPIO${pinout.up}` : '--');
    setTextIfPresent('pinDown', pinout.down !== undefined ? `GPIO${pinout.down}` : '--');
    setTextIfPresent('pinPreset1', pinout.preset1 !== undefined ? `GPIO${pinout.preset1}` : '--');
    setTextIfPresent('pinPreset2', pinout.preset2 !== undefined ? `GPIO${pinout.preset2}` : '--');
    setTextIfPresent('pinPreset3', pinout.preset3 !== undefined ? `GPIO${pinout.preset3}` : '--');
    setTextIfPresent('boardName', payload.boardName || 'ESP32');
    setTextIfPresent('relayUp', payload.relayState && payload.relayState.up ? 'ON' : 'OFF');
    setTextIfPresent('relayDown', payload.relayState && payload.relayState.down ? 'ON' : 'OFF');
    setTextIfPresent('relayPreset1', payload.relayState && payload.relayState.preset1 ? 'ON' : 'OFF');
    setTextIfPresent('relayPreset2', payload.relayState && payload.relayState.preset2 ? 'ON' : 'OFF');
    setTextIfPresent('relayPreset3', payload.relayState && payload.relayState.preset3 ? 'ON' : 'OFF');
    setTextIfPresent('preset', payload.preset || 'Unknown');
  } catch (error) {
    console.error('Failed to fetch status:', error);
  }
}

async function triggerRelayAction(action) {
  const validActions = ['up', 'down', 'stop', 'preset1', 'preset2', 'preset3'];
  if (!validActions.includes(action)) {
    return;
  }

  await fetch('/relay/toggle', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action })
  });
  fetchStatus();
}

async function setRelayPressState(action, pressed) {
  const validActions = ['up', 'down'];
  if (!validActions.includes(action)) {
    return;
  }

  const endpoint = pressed ? '/relay/press' : '/relay/release';
  await fetch(endpoint, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action })
  });
  fetchStatus();
}

async function triggerPreset(id) {
  const actionMap = {
    1: 'preset1',
    2: 'preset2',
    3: 'preset3'
  };

  const action = actionMap[id];
  if (!action) {
    return;
  }

  await triggerRelayAction(action);
}

async function triggerPresetStore(id) {
  if (![1, 2, 3].includes(id)) {
    return;
  }

  await fetch(`/preset/${id}/store`, {
    method: 'POST'
  });
  fetchStatus();
}

function changePreset(direction) {
  const action = direction < 0 ? 'up' : 'down';
  return triggerRelayAction(action);
}

fetchStatus();
setInterval(fetchStatus, 5000);
