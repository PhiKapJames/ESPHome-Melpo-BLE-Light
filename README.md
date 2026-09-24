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
Each light therefore supports an optional:

```yaml
refresh_interval: 15min
```

The component periodically resends the state ESPHome already believes the
light should have. The generic default is `never`; configure the interval
individually on each light.

Group lights support the same per-entity `refresh_interval`.

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
