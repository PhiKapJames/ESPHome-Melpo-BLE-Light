# ESPHome FastCon — MELPO Flood Bridge fork

This fork adds the changes needed for a MELPO RGB flood light to work reliably
with ESPHome over the BroadLink FastCon / brMesh BLE-advertisement protocol.

## Branches

- **`dev`** — keep aligned with upstream `scross01/esphome-fastcon@dev`.
- **`melpo`** — MELPO-specific patches and the reusable Flood Bridge package.

This keeps GitHub's native fork relationship useful: upstream changes can be
reviewed on `dev`, while the MELPO delta stays small and explicit.

## MELPO additions

The `melpo` branch adds:

- corrected FastCon manufacturer AD framing;
- MELPO-validated advertising type/flags;
- an ESP-IDF GAP-event-driven advertising state machine;
- VERY_VERBOSE packet/GAP diagnostics;
- configurable periodic state reassertion for bulbs that do not report state;
- a reusable ESPHome package for the ESP32-S3 MELPO Flood Bridge.

See [docs/MELPO_PATCH.md](docs/MELPO_PATCH.md) for the keep/drop rationale for
each change.

## Private local configuration

Credentials, mesh key, light/site identifiers you consider private, and
deployment RF settings such as `wifi.output_power` should stay in your local
ESPHome YAML.

Minimal local configuration:

```yaml
substitutions:
  melpo_wifi_ssid: !secret wifi_ssid
  melpo_wifi_password: !secret wifi_password
  melpo_api_encryption_key: !secret api_encryption_key
  melpo_flood_key: !secret melpo_flood_key
  melpo_light_id: "1"
  melpo_refresh_interval: "15min"

packages:
  melpo_flood_bridge:
    url: https://github.com/PhiKapJames/ESPHome-Melpo-BLE-Light
    files:
      - packages/melpo-flood-bridge.yaml
    ref: melpo
    refresh: 1h

wifi:
  # Set output_power here in your private/local file.
```

The refresh interval defaults to 15 minutes and can be overridden locally.
Use `melpo_refresh_interval: "never"` to disable periodic reassertion.

## MELPO package behavior

The public package currently targets:

- ESP32-S3
- 4 MB flash
- ESP-IDF
- passive FastCon diagnostic scanning
- FastCon manufacturer ID `FFF0`
- one MELPO light entity using upstream `supports_cwww: false` and
  `color_interlock: true`
- zero default transition length
- `RESTORE_DEFAULT_OFF`
- 1000 ms advertisement duration, retained as the known-working MELPO value
- 15-minute state refresh by default

## Upstream project

For the general FastCon component documentation and group-light support, see:

- https://github.com/scross01/esphome-fastcon

## References

See [docs/REFERENCES.md](docs/REFERENCES.md).

## License

MIT, following the upstream project. See [LICENSE](LICENSE).
