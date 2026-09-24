# MELPO-specific changes

The `main` branch contains the MELPO release patch set. The fork's `dev`
branch remains aligned with `scross01/esphome-fastcon@dev` for upstream
tracking.

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

## 4. Diagnostics

Detailed raw advertisement and GAP sequencing logs use `VERY_VERBOSE`.
Normal startup configuration remains at `CONFIG`; normal light operations
remain at `DEBUG`.

**Status:** keep, but do not normally run the device at VERY_VERBOSE unless
diagnosing protocol/radio behavior.

## 5. Periodic state reassertion

FastCon/brMesh bulbs do not provide authoritative state feedback to this
component. The FastCon light platform adds a configurable
`refresh_interval`.

The generic component defaults to:

```yaml
refresh_interval: never
```

The MELPO device configuration passes a 15-minute interval through package
`vars`.

Every interval, the component resends the exact current ESPHome light state
without changing or republishing the Home Assistant state.

A refresh is skipped if:

- a light transition is currently active, or
- the FastCon command queue is already busy.

**Status:** MELPO reliability feature; keep.

## Configuration choices that are not component patches

The MELPO package also uses:

- `adv_duration: 1000` — known-working hardware value; upstream default remains unchanged.
- `supports_cwww: false` — already supported upstream.
- `color_interlock: true` — already supported upstream.
- `default_transition_length: 0s` — avoids transitional command floods.
- `restore_mode: RESTORE_DEFAULT_OFF`.
- passive BLE diagnostic scanning.

Standard ESPHome device configuration and site-specific values are intentionally
not part of the public package. This includes Wi-Fi credentials, API encryption,
OTA setup, logger settings, and `wifi.output_power`.
