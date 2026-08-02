# Thread Border Router with ESP32-S31

## Bullet points

- Single microcontroller setup
- Stable and fast 1 GB/s Ethernet networking instead of Wi-Fi

## Prerequisites

- ESP-IDF master-Branch installed under `~/.espressif/preview`
- Target: `esp32s31`

## Get Started

```sh
. ~/.espressif/preview/export.sh
cd ot_br
idf.py --preview set-target esp32s31
idf.py build flash monitor
```