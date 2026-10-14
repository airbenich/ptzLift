# ptzPedestal

Local-only M5StickC preset controller with a companion module and local web UI.

## Components

- `firmware/`: ESP32 firmware for the M5StickC controller and embedded web server.
- `companion/`: Node.js companion service that interacts with the device over HTTP.
- `.azure/`: project plan and integration artifacts.

## Build and flash the firmware

### Prerequisites

- Python 3
- PlatformIO Core (`pio`)
- A connected M5StickC via USB serial

### Install PlatformIO

```bash
python3 -m pip install platformio
```

### Configure WiFi credentials (not committed)

Create a `.env` file in the project root (next to this README):

```env
WIFI_SSID=your-wifi-ssid
WIFI_PASSWORD=your-wifi-password
```

You can start from `.env.example`.

The firmware build injects these values at compile time via PlatformIO pre-build script, so credentials stay out of source files.

### Build firmware

From the project root:

```bash
python3 -m platformio run -d firmware
```

### Flash firmware to the M5StickC

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

1. Flash the firmware to the M5StickC with PlatformIO.
2. Power the board and connect to its local Wi‑Fi access point.
3. Run the companion service with `npm --prefix companion install && npm --prefix companion run dev`.
4. Use the device web UI at `http://m5stick-local/` or the companion routes.

## Scope

This project intentionally stays local-only and does not include cloud hosting or a separate host app.
