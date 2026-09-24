# Repository guidance

## Branches

- `main`: working/release branch used by ESPHome devices.
- `dev`: clean tracking branch for `scross01/esphome-fastcon@dev`.

Project-specific changes belong on `main`; upstream comparisons are made against `dev`.

## ESPHome architecture

This repository is consumed through `external_components:`.

The shared `fastcon:` component is the BLE transport/controller. Individual `fastcon` and `fastcon_group_light` entities own their mesh key, target IDs, refresh settings, restore behavior, and light capabilities. One controller can service entities using different mesh keys.

Standard device settings such as Wi-Fi, API encryption, OTA, logger configuration, and secrets remain in the user's ESPHome YAML.

## Validated MELPO behavior

Read `docs/MELPO_PATCH.md` before modifying packet framing or BLE lifecycle behavior.

The MELPO release currently depends on:

- corrected manufacturer AD length;
- `ADV_TYPE_IND`;
- BLE flags byte `0x02`;
- ESP-IDF GAP completion events for configure/start/stop sequencing;
- VERY_VERBOSE protocol diagnostics;
- deployment-selectable advertising duration, with 1000 ms known to work on the tested MELPO flood.

## State refresh

FastCon lights do not report authoritative state.

Normal and group lights support per-entity `refresh_interval`. Optional `refresh_control` entities expose persistent Home Assistant controls for enabling refresh and changing its interval.

## Mesh-key diagnostics

Read `docs/PROVISIONING.md` before modifying mesh-key capture.

The optional controller diagnostics provide:

- a one-shot Diagnostic switch that enables capture;
- a Diagnostic text sensor that publishes a validated 8-character hexadecimal mesh key.

The listener supports recovery from validated normal single-light control packets and validated provisioning packets.

When diagnostics are configured, the private ESPHome YAML should normally set
`esp32_ble_tracker.scan_parameters.active: false` and `continuous: false`.
The FastCon diagnostic switch starts a one-shot scan and only stops the tracker
when FastCon itself started that scan.

QR-code recovery is not currently part of the project.

## Validation

The GitHub Actions baseline is ESPHome 2026.9.0.

`tests/melpo-component.yaml` should continue covering:

- two normal FastCon lights with different mesh keys;
- per-light refresh controls;
- a group light and group refresh controls;
- controller mesh-key diagnostics.

## Documentation

Relevant references:

- `README.md`
- `docs/MELPO_PATCH.md`
- `docs/GROUP_PROTOCOL.md`
- `docs/PROVISIONING.md`
- `docs/REFERENCES.md`

## Repository hygiene

The checked-in `.vscode/extensions.json` and `.vscode/settings.json` currently match upstream and contain shared Ruff/PlatformIO/editor configuration.

Machine-generated VS Code files, ESPHome/PlatformIO build directories, `secrets.yaml`, virtual environments, caches, and generated artifacts are ignored and should remain outside version control.
