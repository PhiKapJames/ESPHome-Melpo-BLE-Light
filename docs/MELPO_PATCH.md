# MELPO-specific changes

The `main` branch contains the MELPO release patch set. The fork's `dev`
branch remains aligned with `scross01/esphome-fastcon@dev` for upstream
tracking.

## Configuration architecture

This fork is used as an ESPHome **external component**. It does not provide or
require a remote device package.

The shared `fastcon:` controller owns BLE transport concerns:

- advertisement interval;
- advertisement duration;
- advertisement gap;
- command queue.

Each light/group owns entity-specific FastCon configuration:

- mesh key;
- light/group ID;
- refresh interval;
- light capabilities;
- restore/transition behavior.

This lets one controller service multiple FastCon meshes.

## 1. Correct manufacturer AD length

Upstream uses:

```cpp
cmd.data.size() + 2
```

The MELPO release uses:

```cpp
cmd.data.size() + 3
```

The BLE AD structure's length byte counts the AD type byte, the two-byte
manufacturer identifier, and the manufacturer payload. With a 24-byte FastCon
payload this produces the app-observed `0x1B` length.

**Status:** required; keep.

## 2. MELPO-validated advertisement type and flags

The known-working MELPO packet uses:

- `ADV_TYPE_IND`
- advertising flags byte `0x02`

Upstream currently uses `ADV_TYPE_NONCONN_IND` and the usual ESP BLE
general-discoverable + BR/EDR-not-supported flags.

These values were validated on the physical MELPO flood but were not isolated
independently from the other advertising fixes.

**Status:** keep on the MELPO release; candidates for a future controlled A/B
test.

## 3. GAP-event-driven advertising lifecycle

ESP-IDF BLE GAP configuration/start/stop operations are asynchronous. The MELPO
release waits for completion events before advancing:

```text
IDLE
  -> CONFIGURING
  -> STARTING
  -> ADVERTISING
  -> STOPPING
  -> GAP
  -> IDLE
```

The component registers with ESPHome's `esp32_ble` GAP callback mechanism and
handles:

- `ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT`
- `ESP_GAP_BLE_ADV_START_COMPLETE_EVT`
- `ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT`

This replaces the earlier fixed-delay experiment.

**Status:** required; keep.

## 4. Per-entity mesh keys

Upstream normal lights stored the mesh key on the shared controller. Group
lights already stored it on the entity.

This fork makes the behavior consistent: normal lights also require
`mesh_key` on each `platform: fastcon` entity. The controller is transport
only.

This supports multiple lights that belong to different FastCon meshes through a
single bridge.

## 5. Per-entity periodic state reassertion

FastCon/brMesh bulbs do not provide authoritative state feedback.

Both `platform: fastcon` and `platform: fastcon_group_light` support:

```yaml
refresh_interval: 15min
```

The default is `never`.

Every interval, the component resends the exact current ESPHome light state
without changing or republishing Home Assistant state.

A refresh is skipped if:

- a light transition is currently active, or
- the FastCon controller queue is busy.

## 6. Diagnostics

Detailed advertisement and GAP sequencing logs use `VERY_VERBOSE`.

**Status:** keep, but use VERY_VERBOSE only while diagnosing protocol/radio
behavior.

## Device-specific settings

Settings such as `adv_duration: 1000`, Wi-Fi output power, network
credentials, API encryption, logging level, light IDs, mesh keys, restore
modes, and refresh intervals belong in the user's ESPHome device YAML, not in
this repository's component defaults.
