# ptzLift

Local-only ESP32 PTZ lift relay controller with a local web UI and REST API.

## Components

- `firmware/`: ESP32 firmware for the DevKitC V4 target and embedded web server.

## Build and flash the firmware

### Prerequisites

- Python 3
- PlatformIO Core (`pio`)
- A connected ESP32 DevKitC V4 via USB serial

### Install PlatformIO

```bash
python3 -m pip install platformio
```

### Configure WiFi credentials (not committed)

Create a `.env` file in the project root (next to this README):

```env
WIFI_SSID=your-wifi-ssid
WIFI_PASSWORD=your-wifi-password
WIFI_AP_SSID=ptzlift-local
```

You can start from `.env.example`.

The fallback access point is intentionally open and has no password. The AP SSID is configured in `.env` so it stays out of source control.

The firmware build injects these values at compile time via PlatformIO pre-build script, so credentials stay out of source files.

### Optional status LED output

The official ESP32 DevKitC V4 typically exposes a fixed 5V power LED (not GPIO-controlled). Because of this, status LED signaling is disabled by default:

```env
STATUS_LED_PIN=-1
STATUS_LED_ACTIVE_HIGH=1
```

If your board variant has a controllable onboard LED, set `STATUS_LED_PIN` to that GPIO (often `2` on some clones) and adjust polarity with `STATUS_LED_ACTIVE_HIGH`.

## Built-in LED status feedback

The firmware uses the built-in LED for connectivity and request activity:

- Connecting to WiFi: fast fading (breathing)
- Connected to WiFi: slow fading (breathing)
- Fallback AP mode: double blink
- Any web request (API or static file): short pulse overlay

The request pulse is layered on top of the WiFi pattern and returns automatically to the current WiFi state pattern.

If no controllable LED is configured (`STATUS_LED_PIN=-1`), the firmware reports `"ledState":"unavailable"` in `/status`.

### Build firmware

From the project root:

```bash
python3 -m platformio run -d firmware
```

### Flash firmware to the ESP32 DevKitC V4

Make sure the device is connected over USB and the serial port is detected, then run:

```bash
python3 -m platformio run -d firmware -t upload
```

If you also want to upload the LittleFS web data files for the embedded UI:

```bash
python3 -m platformio run -d firmware -t uploadfs
```

### Serial monitor

```bash
python3 -m platformio device monitor -d firmware
```

## Local operation

1. Flash the firmware to the ESP32 DevKitC V4 with PlatformIO.
2. Power the board and connect to its local Wi‑Fi access point.
3. Use the device web UI at `http://<device-ip>/` or the REST routes.

## Relay behavior

- Exclusivity rule: whenever one relay is pressed/activated, all other relays are switched OFF.
- This applies to Up/Down and also to Preset 1/2/3 actions.
- Up/Down are toggle relays with auto-off timeout.
- Preset relays support pulse, store, and explicit press/release endpoints.

## REST API

### Status and WiFi

- `GET /status`: complete device status, including relay states and `ledState`.
- `GET /wifi/status`: WiFi mode, current SSID, saved SSID, IP.
- `POST /wifi/save`: save SSID/password, reconnect, fallback to AP if connect fails.

### Relay control

- `POST /relay/toggle` body `{"action":"up|down|preset1|preset2|preset3|stop"}`
- `POST /relay/press` body `{"action":"up|down"}`
- `POST /relay/release` body `{"action":"up|down"}`

### Preset control

- `POST /preset/1`, `POST /preset/2`, `POST /preset/3`: trigger preset pulse.
- `POST /preset/1/store`, `POST /preset/2/store`, `POST /preset/3/store`: 6s store pulse.
- `POST /preset/press` body `{"action":"preset1|preset2|preset3"}`
- `POST /preset/release` body `{"action":"preset1|preset2|preset3"}`

### Static files

- `GET /`: web UI
- `GET /app.js`: client script

## Scope

This project intentionally stays local-only and does not include cloud hosting or a separate host app.
