# Thread Border Router with ESP32

## Bullet points

- Supports single ESP32-S31 microcontroller setup
- Supports stable and fast 1 GB/s Ethernet network connection instead of rely on Wi-Fi

## Prerequisites

- ESP-IDF master-branch installed under `~/.espressif/preview`
- Target: `esp32s31`

## Get Started

```sh
. ~/.espressif/preview/export.sh
cd ot_br
idf.py --preview set-target esp32s31
idf.py build flash monitor
```

## Supported Hardware Platforms

### Single SoC Module

- ESP32-S31 single SoC (Function-CoreBoard-1, Korvo-1)

### SoC with Co-Processor

(- ESP32-P4 with Radio Co-Processor (RCP) e.g. ESP32-C6, ESP32-H2, ESP32-C5 - not tested)

(- ESP Thread Border Router - ESP32-S3 with ESP32-H2 - not tested)
