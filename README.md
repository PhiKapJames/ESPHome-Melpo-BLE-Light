# ESPHome FastCon — MELPO Flood Light fork

This repository is a fork of `scross01/esphome-fastcon` with MELPO-specific
BLE advertising fixes and a few configuration improvements for normal ESPHome
use.

## Branches

- **`dev`** — tracks `scross01/esphome-fastcon@dev`.
- **`main`** — the MELPO release branch.

## ESPHome integration

Use this repository as an **external component**, not as a remote ESPHome
package. Your ESPHome device YAML remains the source of truth for the device,
network, controller, and light entities.

```yaml
external_components:
  - source: github://PhiKapJames/ESPHome-Melpo-BLE-Light@main
    components:
      - fastcon

esp32_ble:

fastcon:
  id: fastcon_controller
  adv_duration: 1000

light:
  - platform: fastcon
    id: melpo_flood_1
    name: "Melpo Flood"
    controller_id: fastcon_controller
    mesh_key: !secret melpo_flood_key
    light_id: 1
    refresh_interval: 15min
    supports_cwww: false
    color_interlock: true
    restore_mode: RESTORE_DEFAULT_OFF
    default_transition_length: 0s
```

This follows the same pattern as normal ESPHome platforms: the component
provides functionality and each entity is configured in the device YAML.

## Multiple lights and mesh keys

`mesh_key` is defined **per light**, not on the shared controller. That means
one ESP32 bridge can control multiple FastCon meshes:

```yaml
fastcon:
  id: fastcon_controller
  adv_duration: 1000

light:
  - platform: fastcon
    name: "Front Flood"
    controller_id: fastcon_controller
    mesh_key: !secret front_flood_key
    light_id: 1
    refresh_interval: 15min

  - platform: fastcon
    name: "Rear Flood"
    controller_id: fastcon_controller
    mesh_key: !secret rear_flood_key
    light_id: 1
    refresh_interval: 30min
```

The two lights may use the same mesh key or completely different mesh keys.

## Per-light state reassertion

FastCon/brMesh lights do not report authoritative state back to ESPHome.
Each light supports an optional static/default refresh interval:

```yaml
refresh_interval: 15min
```

The generic default is `never`.

A light can also expose persistent Home Assistant configuration entities:

```yaml
refresh_interval: 15min
refresh_control:
  enabled:
    name: "Melpo Flood Periodic State Refresh"
  interval:
    name: "Melpo Flood Refresh Interval"
    min_value: 1
    max_value: 1440
    step: 1
    mode: box
```

This creates a configuration-category switch and number entity in Home
Assistant. The switch enables/disables periodic reassertion, and the number
sets the interval in minutes. Changes are persisted on the ESP32 and survive
reboots. The YAML `refresh_interval` is the initial/default value.

The same per-entity controls are available on `fastcon_group_light`.

## Group lights

The group-light platform continues to keep its mesh key with the entity:

```yaml
external_components:
  - source: github://PhiKapJames/ESPHome-Melpo-BLE-Light@main
    components:
      - fastcon
      - fastcon_group_light

fastcon:
  id: fastcon_controller
  adv_duration: 1000

light:
  - platform: fastcon_group_light
    id: living_room_group
    name: "Living Room"
    controller_id: fastcon_controller
    mesh_key: !secret living_room_mesh_key
    start_light_id: 12
    mask: 0x3F
    refresh_interval: 15min
    refresh_control:
      enabled:
        name: "Living Room Periodic State Refresh"
      interval:
        name: "Living Room Refresh Interval"
        min_value: 1
        max_value: 1440
        step: 1
        mode: box
    default_transition_length: 0s
    restore_mode: ALWAYS_OFF
```

## MELPO-specific fixes

The release branch retains the fixes that were validated against the physical
MELPO flood:

- corrected BLE manufacturer AD length;
- `ADV_TYPE_IND` and MELPO-observed advertisement flags;
- GAP-event-driven asynchronous advertising lifecycle;
- VERY_VERBOSE packet/GAP diagnostics;
- known-working MELPO advertisement duration can be selected in YAML
  (`adv_duration: 1000`) without changing the generic component default.

See [docs/MELPO_PATCH.md](docs/MELPO_PATCH.md) for details.

## Example device

See [examples/melpo_flood.yaml](examples/melpo_flood.yaml).

## Upstream and references

See [docs/REFERENCES.md](docs/REFERENCES.md).

## License

MIT, following the upstream project. See [LICENSE](LICENSE).


## Mesh-key diagnostics

The controller can optionally expose a one-shot mesh-key listener in Home
Assistant's **Diagnostic** section.

```yaml
esp32_ble:

esp32_ble_tracker:
  scan_parameters:
    active: false
    continuous: false

fastcon:
  id: fastcon_controller
  adv_duration: 1000

  diagnostics:
    mesh_key_listener:
      name: "FastCon Mesh Key Listener"

    detected_mesh_key:
      name: "FastCon Detected Mesh Key"
```

Usage:

1. Turn on **FastCon Mesh Key Listener**.
2. To recover an existing key, use the official app to send a normal
   single-light command.
3. To recover a newly assigned key after factory reset, enable the listener
   before registering the light with the official app.
4. A validated key is published as eight hexadecimal characters in
   **FastCon Detected Mesh Key**.
5. The listener automatically switches itself back off after a key is found.

The listener recognizes both validated normal control traffic and validated
FastCon provisioning traffic. It is based on over-the-air BLE advertisements,
not Android logcat.

With `continuous: false`, the ESP32 BLE tracker stays idle during normal
operation. Turning **FastCon Mesh Key Listener** on starts a one-shot scan.
Finding a key, manually turning the listener off, or reaching the scan-duration
timeout ends the FastCon diagnostic session. FastCon only stops the tracker when
it was the component that started that scan; an already-running scan owned by
another component is left alone.

ESPHome already defaults the scan interval to 320 ms, so no interval/window
override is required for this diagnostic use case.

See [docs/PROVISIONING.md](docs/PROVISIONING.md) for protocol details,
validation rules, and current limitations.
