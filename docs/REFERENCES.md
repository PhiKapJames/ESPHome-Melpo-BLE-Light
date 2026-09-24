# Upstream and references

## Fork lineage

This repository is a GitHub fork of:

- https://github.com/scross01/esphome-fastcon

which in turn is a fork of:

- https://github.com/dennispg/esphome-fastcon

Branch policy:

- `dev` tracks `scross01/esphome-fastcon@dev`.
- `main` contains the MELPO release changes.

When upstream changes, sync/review `dev` first, then merge/rebase those
changes into `main` and resolve only the MELPO-specific delta.

## Other protocol/implementation references

The MELPO investigation also referred to:

- https://github.com/ArcadeMachinist/brMeshMQTT
- https://github.com/KDragon75/fastcon
- https://mooody.me/posts/2023-04/reverse-the-fastcon-ble-protocol/
- https://community.home-assistant.io/t/brmesh-app-bluetooth-lights/473486/102

The FastCon component lineage is MIT licensed. References without a published
license are linked for attribution and behavioral comparison rather than copied
into this repository.
