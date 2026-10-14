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

if ssid:
    env.Append(BUILD_FLAGS=[f'-DWIFI_SSID=\\"{_escape_define(ssid)}\\"'])
if password:
    env.Append(BUILD_FLAGS=[f'-DWIFI_PASSWORD=\\"{_escape_define(password)}\\"'])

if not ssid or not password:
    print("[load_env.py] WARNING: WIFI_SSID or WIFI_PASSWORD missing in .env; firmware will use fallback placeholder values.")
