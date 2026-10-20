Import("env")

from pathlib import Path


def _parse_env_file(path: Path):
    values = {}
    if not path.exists():
        return values

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[len("export "):].strip()
        if "=" not in line:
            continue

        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()

        if len(value) >= 2 and value[0] == value[-1] and value[0] in ('"', "'"):
            value = value[1:-1]

        values[key] = value

    return values


def _escape_define(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


project_root = Path(env.subst("$PROJECT_DIR")).resolve().parent
env_path = project_root / ".env"
vars_map = _parse_env_file(env_path)

ssid = vars_map.get("WIFI_SSID", "")
password = vars_map.get("WIFI_PASSWORD", "")
fallback_ap_ssid = vars_map.get("WIFI_AP_SSID", "ptzlift-local")
status_led_pin = vars_map.get("STATUS_LED_PIN", "-1")
status_led_active_high = vars_map.get("STATUS_LED_ACTIVE_HIGH", "1")

if ssid:
    env.Append(BUILD_FLAGS=[f'-DWIFI_SSID=\\"{_escape_define(ssid)}\\"'])
if password:
    env.Append(BUILD_FLAGS=[f'-DWIFI_PASSWORD=\\"{_escape_define(password)}\\"'])
env.Append(BUILD_FLAGS=[f'-DWIFI_AP_SSID=\\"{_escape_define(fallback_ap_ssid)}\\"'])
env.Append(BUILD_FLAGS=[f'-DSTATUS_LED_PIN={status_led_pin}'])
env.Append(BUILD_FLAGS=[f'-DSTATUS_LED_ACTIVE_HIGH={status_led_active_high}'])

if not ssid or not password:
    print("[load_env.py] WARNING: WIFI_SSID or WIFI_PASSWORD missing in .env; firmware will use fallback placeholder values.")

if not vars_map.get("WIFI_AP_SSID"):
    print("[load_env.py] INFO: WIFI_AP_SSID not set in .env; using default fallback AP SSID 'ptzlift-local' without a password.")

if not vars_map.get("STATUS_LED_PIN"):
    print("[load_env.py] INFO: STATUS_LED_PIN not set in .env; LED status output disabled by default on ESP32 DevKitC V4.")
