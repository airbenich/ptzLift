# FootPedalREST v2 for Windows

**FootPedalREST.exe** is a standalone Windows 10/11 x64 application. It detects
**F1, F2, F3** key-down (`press`) and key-up (`release`) transitions from a USB
foot pedal that behaves as a HID keyboard and calls **six independently
configurable HTTP endpoints**. It works while other applications have focus.

Each action has its **own URL, HTTP method, optional JSON body, and optional
HTTP headers**. The program does not automatically send `key` or `event` in
request bodies or URL parameters: pressing F1 and releasing F1 can invoke
completely different APIs, not just send different JSON to a shared endpoint.

Source is written in Go. The compiled .exe does not require Go or any external
runtime to be installed. Windows 10/11 x64 is the target platform.

## Quick start

1. Extract the ZIP into a normal writable folder, e.g. on your Desktop.
2. Double-click **RESTTestReceiver.exe** (a local test API).
3. Double-click **FootPedalREST.exe**.
4. Press and release F1, F2, and F3 on the foot pedal. The test receiver logs
   requests like `POST /f1/press` and `POST /f1/release` at different URLs.
5. Edit **settings.json** to point each action to the desired API. Restart
   FootPedalREST.exe after changing settings.
6. Close the app with **Ctrl+C** in its console.

You can use `start-minimized.cmd` to start the bridge with its console minimized.
The bridge must stay running. It does **not** block F1-F3 keyboard shortcuts in
the active application.

## settings.json: six individual actions

This is the **full default settings file**. Each event maps to its own HTTP
request. All URLs below are handled by the included local test receiver:

```json
{
  "device_filter": "",
  "bearer_token": "",
  "headers": {},
  "timeout_seconds": 5,
  "requests": {
    "F1": {
      "press":   {"url": "http://127.0.0.1:8080/f1/press",   "method": "POST"},
      "release": {"url": "http://127.0.0.1:8080/f1/release", "method": "POST"}
    },
    "F2": {
      "press":   {"url": "http://127.0.0.1:8080/f2/press",   "method": "POST"},
      "release": {"url": "http://127.0.0.1:8080/f2/release", "method": "POST"}
    },
    "F3": {
      "press":   {"url": "http://127.0.0.1:8080/f3/press",   "method": "POST"},
      "release": {"url": "http://127.0.0.1:8080/f3/release", "method": "POST"}
    }
  }
}
```

The six URL destinations and methods do **not** need to be similar. You can
use unrelated servers, endpoints, HTTP methods and payloads for each action.
All six `press` / `release` definitions are required, to avoid silent mistakes.
If you do not need an action, you may point it to a harmless endpoint such as
your own no-op API. The app sends one request for every configured transition.

### Example: per-action methods, headers and bodies

Replace an individual action with something like:

```json
"press": {
  "url": "https://example.org/api/lights/on",
  "method": "PUT",
  "headers": {
    "X-Device": "pedal-1",
    "X-API-Key": "press-specific-key"
  },
  "body": {"enabled": true, "level": 100}
}
```

And its corresponding release action could be:

```json
"release": {
  "url": "https://example.org/api/lights/off",
  "method": "GET"
}
```

In this example, **press** sends a PUT request with the specified JSON body,
while **release** sends a GET request to a different URL with **no body**.
No implicit event metadata is added to either request.

### Configuration fields

- **requests.F1.press** etc.: The individual URL and HTTP method for each of
  the six actions. `GET`, `POST`, `PUT`, `PATCH`, and `DELETE` are supported.
  If `method` is omitted on an action, it defaults to `POST`.
- **body** (per action): Optional JSON value, such as an object, array, number,
  string, or boolean. Sent unchanged as the HTTP body. When present, the
  `Content-Type` header defaults to `application/json`. There is **no body by
  default**. Omit `body` on GET actions.
- **headers** (per action): Optional extra headers. These override top-level
  `headers` or `Authorization` when names match.
- **headers** (top-level): Shared headers added to all six requests.
- **bearer_token**: Optional shared bearer token, sent as
  `Authorization: Bearer <token>` unless that action overrides Authorization.
- **device_filter**: Optional, case-insensitive substring of the Windows raw
  input HID path; empty means F1/F2/F3 from **any** attached keyboard.
- **timeout_seconds**: Timeout per HTTP request, between 1 and 120 seconds.

Every request also has an automatically generated `Idempotency-Key` HTTP
header containing its unique event ID. (A per-action header can override it.)
Redirects are not followed, and failed requests are logged but not retried.

For a GET URL with fixed query arguments, put them directly in `url`, e.g.
`https://example.org/pedal?channel=1&state=on`; the program will not append
any unexpected key/event parameters.

### Restrict the listener to your pedal

1. Leave `device_filter` empty at first.
2. Press one of the pedals and check the FootPedalREST console for a line like:
   `INPUT F1 press from \\?\HID#VID_1234&PID_ABCD&MI_00#...`
3. Copy `VID_1234&PID_ABCD` into `device_filter`, save the file and restart.

This avoids ordinary keyboard F1/F2/F3 triggering webhooks, as long as the HID
identifiers differ. The complete device path also works as a more specific
filter. This only supports pedals that generate keyboard HID events, not
joystick-only or proprietary HID reports.

## Upgrading from version 1

Version 1 had only one shared `endpoint` and `method`. **The configuration
format changed.** Back up your old `settings.json` and use the six-request
format above. When v2 detects the old `endpoint` field it stops with a clear
migration error rather than silently sending webhooks to wrong URLs. You may
reuse your old bearer token, headers, timeout and device filter.

## Delivery and security notes

- Key-down auto-repeat is suppressed while a pedal is held; one `press` and
  one `release` are queued for each normal down/up cycle.
- Events are processed in detection order by a single HTTP worker; up to
  1024 can wait in memory if the server is slow. When full, new events are
  dropped with a visible warning.
- One HTTP attempt is made per event. Non-2xx responses are logged but not
  retried, to avoid accidentally repeating API side effects. Use server-side
  idempotency/durable queues if you require guaranteed delivery.
- Closing with Ctrl+C allows up to 15 seconds for queued events to finish.
- Raw Input runs in the background only while the application is active. It
  cannot receive keys on the Windows secure/UAC desktop or when signed out.
- `settings.json` stores credentials in plaintext; protect it with Windows
  file permissions and use `https://` for endpoints outside your machine.
- Editing `settings.json` requires a restart; there is no live reload.

## Build from source

Install Go 1.23 or newer on Windows, then run `build-windows.cmd`.
Equivalent commands:

```bat
set GOOS=windows
set GOARCH=amd64
set CGO_ENABLED=0
go build -trimpath -ldflags "-s -w" -o FootPedalREST.exe .
go build -trimpath -ldflags "-s -w" -o RESTTestReceiver.exe ./cmd/testreceiver
```

Run `go test ./...` to execute the configuration and HTTP integration tests.
The packaged binaries have been cross-compiled to Windows x64. The six-route
HTTP behavior is tested locally, but physical pedal behavior must be verified
on Windows with your particular USB pedal.
