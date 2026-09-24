# ESPHome FastCon — MELPO Flood Bridge fork

This fork adds the changes needed for a MELPO RGB flood light to work reliably
with ESPHome over the BroadLink FastCon / brMesh BLE-advertisement protocol.

## Branch model

- **`dev`** — tracks `scross01/esphome-fastcon@dev` as closely as possible.
- **`main`** — the MELPO release branch used by the actual bridge.

The MELPO-specific changes are therefore combined on `main`, while `dev`
remains useful for native GitHub upstream comparisons and future merges.

## MELPO additions on main

- corrected FastCon manufacturer AD framing;
- MELPO-validated advertising type/flags;
- ESP-IDF GAP-event-driven advertising lifecycle;
- VERY_VERBOSE packet/GAP diagnostics;
- configurable periodic state reassertion for bulbs that do not report state;
- reusable ESPHome package for the ESP32-S3 MELPO Flood Bridge;
- ESPHome 2026.9 compile/package smoke tests.

See [docs/MELPO_PATCH.md](docs/MELPO_PATCH.md) for the rationale for each
retained change.

## ESPHome configuration split

The public package contains only reusable bridge/hardware/FastCon configuration.

Standard ESPHome device settings stay in your local YAML:

- `esphome:` name/friendly name
- `logger:`
- `api:` and its encryption key
- `ota:`
- `wifi:` and Wi-Fi secrets
- deployment-specific Wi-Fi RF tuning such as `output_power`

FastCon-specific values are passed to the package using ESPHome package
`vars`, which keeps the public package reusable without wrapping normal
ESPHome secrets in custom substitutions.

Example local configuration:

```yaml
esphome:
  name: melpo-flood-bridge
  friendly_name: Melpo Flood Bridge

packages:
  melpo_flood_bridge:
    url: https://github.com/PhiKapJames/ESPHome-Melpo-BLE-Light
    ref: main
    refresh: 1h
    files:
      - path: packages/melpo-flood-bridge.yaml
        vars:
          melpo_flood_key: !secret melpo_flood_key
          melpo_light_id: "1"
          melpo_light_name: "Melpo Flood"
          melpo_refresh_interval: "15min"

logger:
  level: VERY_VERBOSE
  initial_level: DEBUG

api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  # Add your site-specific output_power here.
```

The MELPO package sets a 15-minute refresh only because the local config passes
that value. The generic FastCon light component itself defaults
`refresh_interval` to `never`.

## Public package behavior

The package currently provides:

- ESP32-S3 / 4 MB / ESP-IDF hardware definition
- passive FastCon diagnostic scanning
- manufacturer ID `FFF0` diagnostic capture
- the patched FastCon component from this repository's `main`
- known-working MELPO advertisement duration of 1000 ms
- `supports_cwww: false`
- `color_interlock: true`
- zero default transition length
- `RESTORE_DEFAULT_OFF`

## Upstream

Primary upstream:

- https://github.com/scross01/esphome-fastcon

Original project:

- https://github.com/dennispg/esphome-fastcon

See [docs/REFERENCES.md](docs/REFERENCES.md) for the other protocol and
implementation references used during the MELPO investigation.

## License

MIT, following the upstream project. See [LICENSE](LICENSE).
