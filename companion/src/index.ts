import http from 'node:http';

import { createDeviceClient, DeviceStatus } from './client.js';
import { toggleRelay, triggerPreset } from './actions.js';
import { createInitialState, CompanionState } from './state.js';

const client = createDeviceClient();
const state: CompanionState = createInitialState();

async function readBody(request: http.IncomingMessage): Promise<string> {
  const chunks: Buffer[] = [];
  for await (const chunk of request) {
    chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
  }
  return Buffer.concat(chunks).toString('utf8');
}

const server = http.createServer(async (request, response) => {
  const requestUrl = request.url ?? '/';
  const url = new URL(requestUrl, 'http://localhost');

  if (request.method === 'GET' && url.pathname === '/health') {
    response.writeHead(200, { 'Content-Type': 'application/json' });
    response.end(JSON.stringify({ status: 'ok', service: 'companion' }));
    return;
  }

  if (request.method === 'GET' && url.pathname === '/status') {
    try {
      const status: DeviceStatus = await client.getStatus();
      Object.assign(state, {
        deviceId: status.deviceId,
        relay1: status.relayState.relay1,
        relay2: status.relayState.relay2,
        preset: status.preset,
        healthy: status.healthy,
        connected: status.companionConnected,
        lastUpdated: Date.now(),
      });

      response.writeHead(200, { 'Content-Type': 'application/json' });
      response.end(JSON.stringify(state));
    } catch (error) {
      response.writeHead(502, { 'Content-Type': 'application/json' });
      response.end(JSON.stringify({ error: { code: 'DEVICE_UNAVAILABLE', message: error instanceof Error ? error.message : 'Unknown device error' } }));
    }
    return;
  }

  if (request.method === 'POST' && /^\/preset\/[1-3]$/.test(url.pathname)) {
    const preset = Number(url.pathname.split('/').at(-1));
    try {
      const result = await triggerPreset(client, preset);
      response.writeHead(200, { 'Content-Type': 'application/json' });
      response.end(JSON.stringify(result));
    } catch (error) {
      response.writeHead(502, { 'Content-Type': 'application/json' });
      response.end(JSON.stringify({ error: { code: 'PRESET_FAILED', message: error instanceof Error ? error.message : 'Preset execution failed' } }));
    }
    return;
  }

  if (request.method === 'POST' && url.pathname === '/relay/toggle') {
    try {
      const body = await readBody(request);
      const payload = JSON.parse(body || '{}') as { relayId?: 'relay1' | 'relay2'; enabled?: boolean };
      if (!payload.relayId || typeof payload.enabled !== 'boolean') {
        response.writeHead(400, { 'Content-Type': 'application/json' });
        response.end(JSON.stringify({ error: { code: 'INVALID_PAYLOAD', message: 'relayId and enabled are required' } }));
        return;
      }

      const result = await toggleRelay(client, payload.relayId, payload.enabled);
      state.relay1 = payload.relayId === 'relay1' ? payload.enabled : state.relay1;
      state.relay2 = payload.relayId === 'relay2' ? payload.enabled : state.relay2;
      state.lastUpdated = Date.now();

      response.writeHead(200, { 'Content-Type': 'application/json' });
      response.end(JSON.stringify(result));
    } catch (error) {
      response.writeHead(502, { 'Content-Type': 'application/json' });
      response.end(JSON.stringify({ error: { code: 'TOGGLE_FAILED', message: error instanceof Error ? error.message : 'Unknown relay toggle failure' } }));
    }
    return;
  }

  if (request.method === 'GET' && url.pathname === '/') {
    response.writeHead(200, { 'Content-Type': 'application/json' });
    response.end(JSON.stringify(state));
    return;
  }

  response.writeHead(404, { 'Content-Type': 'application/json' });
  response.end(JSON.stringify({ error: { code: 'NOT_FOUND', message: 'Endpoint not found' } }));
});

const port = Number(process.env.PORT ?? 3000);
server.listen(port, () => {
  console.log(`Companion service started on http://localhost:${port}`);
});
