# Thread Border Router with ESP32

## Bullet points

- Supports single ESP32-S31 microcontroller setup

- Supports stable and fast 1 GB/s Ethernet network connection instead of rely on Wi-Fi

## Features

### Backend

- Thread Border Router with Ethernet-Backhaul, Auto-Attach after Reboot

- REST-API with Setup-Token-Auth (Device-Info, Thread-State, Dataset, Neighbors, Commissioner, OTA)

- OTA with GitHub Releases, including Rollback-Protection

- 16-MB-Partitionslayout incl. prepared nvs_keys for future encryption

- CI: firmware build

### Frontend

- Fast Astro SSR with Qwik interactions

- CI: static file generation

- dev-proxy for local development

## Prerequisites

- ESP-IDF master-branch installed under `~/.espressif/preview`
- Target: `esp32s31`

## Get Started

Flash Thread border router firmware:

```sh
. ~/.espressif/preview/export.sh
cd ot_br
idf.py --preview set-target esp32s31
idf.py build flash monitor
```

## Web change

1. Edit frontend

2. Build frontend

```sh
cd web && pnpm build
```

3. Build Firmware incl. UI-Partition and flash

```sh
cd ../ot_br && idf.py build flash monitor
````

4. Browser: (Hard-) Reload

## Supported Hardware Platforms

### Single SoC Module

- ESP32-S31 single SoC (Function-CoreBoard-1, Korvo-1)

### SoC with Co-Processor

(- ESP32-P4 with Radio Co-Processor (RCP) e.g. ESP32-C6, ESP32-H2, ESP32-C5 - not tested)

(- ESP Thread Border Router - ESP32-S3 with ESP32-H2 - not tested)
