# FastCon mesh-key discovery

This document describes the optional diagnostic listener used to recover a FastCon/brMesh mesh key from BLE advertisements.

QR-code recovery is intentionally outside the scope of the current implementation.

## Home Assistant diagnostics

When configured, the controller exposes two Diagnostic entities:

- **Mesh Key Listener**: one-shot switch, default OFF.
- **Detected Mesh Key**: text sensor containing the last validated key as eight hexadecimal characters.

Example:

```yaml
esp32_ble:

esp32_ble_tracker:
  scan_parameters:
    active: false
    interval: 320ms
    window: 160ms
    continuous: true

fastcon:
  id: fastcon_controller

  diagnostics:
    mesh_key_listener:
      name: "FastCon Mesh Key Listener"

    detected_mesh_key:
      name: "FastCon Detected Mesh Key"
```

The BLE tracker is only needed for receive-side diagnostics. Normal FastCon command transmission does not otherwise require it.

After a valid key is found, the text sensor publishes the key and the listener switch turns itself off.

## Recovery from normal control traffic

Normal single-light FastCon commands use a 16-byte encoded body. The command data is XORed with a repeating four-byte mesh key.

The well-understood single-light control packets used by this component are padded with zeros. For these packet shapes, the final four encoded body bytes contain one complete repetition of the active mesh key.

The listener validates the candidate before publishing it:

1. de-whiten the FastCon RF payload;
2. verify the fixed RF wrapper;
3. verify the RF CRC;
4. derive a candidate key from the padded command tail;
5. decrypt the packet with that candidate;
6. verify the FastCon body checksum;
7. verify command type and forward flag;
8. verify the safe-key byte;
9. accept only the known single-light `0x22` and `0x72` command forms;
10. verify the expected zero padding.

Practical workflow:

```text
Enable listener
  -> use the official iOS or Android app to change the light
  -> Detected Mesh Key is populated
  -> listener disables itself
```

This path observes over-the-air FastCon traffic and does not depend on Android ADB or logcat.

## Recovery from provisioning traffic

FastCon provisioning uses command type 2 and the known default pairing format. The decoded 12-byte provisioning payload is:

```text
device_id[6] light_id group_id new_mesh_key[4]
```

Because the provisioning packet uses the known default pairing key, a listener can decode the newly assigned mesh key without knowing it beforehand.

The implementation validates the RF wrapper, RF CRC, FastCon checksum, command type, direction, and safe-key byte before publishing the final four provisioning bytes.

A public reverse-engineering implementation documents the same payload layout and provisioning behavior:

- https://github.com/KDragon75/fastcon

The earlier brMesh MQTT work also notes that direct registration is possible without the official application:

- https://github.com/ArcadeMachinist/brMeshMQTT

## Factory-reset recovery workflow

The project currently listens during provisioning; it does not provision the light itself.

Recommended workflow:

```text
1. Enable FastCon Mesh Key Listener.
2. Factory-reset or place the light into its pairing window.
3. Register it with the official iOS or Android app.
4. Wait for Detected Mesh Key to populate.
5. The listener switches itself OFF.
6. Copy the hexadecimal key into the appropriate ESPHome secret.
```

The provisioning packet format is confirmed. A fresh iOS registration capture has not yet been independently validated by this repository, so iOS provisioning capture should be treated as expected but still worth verifying on hardware.

## Security note

The mesh key grants control of the FastCon mesh. The diagnostic text sensor intentionally exposes it because the feature exists for key recovery.

If that exposure is not desired, omit the `diagnostics:` configuration.

Recovered production mesh keys should not be committed to the repository.
