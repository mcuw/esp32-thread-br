# Matter-Unterstützung – Roadmap

Ziel: Der ESP32-S31 Thread Border Router (TBR) soll Matter-over-Thread-Geräte
unterstützen.

## Ausgangslage

Matter-over-Thread-Geräte brauchen einen Thread Border Router, der:

| Anforderung | Status im Projekt |
|---|---|
| IPv6-Routing Thread ⇄ Ethernet (Border Routing, OMR-Präfix, RA) | vorhanden (`CONFIG_OPENTHREAD_BORDER_ROUTER`) |
| SRP-Server (Geräte registrieren `_matter._tcp` / `_matterc._udp`) | im ESP-IDF-BR enthalten, zu verifizieren |
| Advertising Proxy (SRP → mDNS auf Ethernet) | im ESP-IDF-BR enthalten, zu verifizieren |
| mDNS auf dem Backbone | vorhanden (`espressif/mdns`, Multi-Instance) |
| Thread-Dataset für den Matter-Controller bereitstellen | teilweise (`/api/thread/dataset/full`) |
| NAT64 / DNS64 (Internetzugang für Geräte, z. B. OTA) | offen |

Der TBR muss also selbst **kein** Matter sprechen, damit Matter-Geräte im Thread-Netz
funktionieren. Die Inbetriebnahme (BLE + Dataset-Übergabe) erledigt ein
Matter-Controller (Apple Home, Google Home, Home Assistant, `chip-tool`).

## Ausbaustufen

### Stufe 1 – „Matter-ready“ TBR (ohne Matter-Stack)

Umgesetzt (Kconfig-Menü „OT BR Matter support“, beide Optionen standardmäßig an):

- [x] SRP-Server im Auto-Enable-Modus (`CONFIG_OT_BR_MATTER_SRP_AUTO_ENABLE`):
      läuft, sobald Border Routing läuft
- [x] NAT64-Prefix-Manager aktiviert (`CONFIG_OT_BR_MATTER_NAT64`): publiziert einen
      NAT64-Prefix ins Thread-Netz, sobald einer verfügbar ist
- [x] `GET /api/thread/srp` – SRP-Status, registrierte Hosts/Services; Matter-Services
      (`_matter._tcp`, `_matterc._udp`, `_matterd._udp`) werden markiert, bei
      operativen Services werden Compressed-Fabric-ID und Node-ID ausgelesen
- [x] `GET /api/thread/border-routing` – Border-Routing-Status, OMR-/On-Link-Prefix, NAT64
- [x] `GET /api/thread/dataset/tlvs` (Auth: `X-Setup-Token`) – aktives Dataset als TLV-Hex
      für Matter-Controller, z. B. `chip-tool pairing ble-thread <node-id> hex:<tlvs> <pin> <discriminator>`
- [x] Web-UI: Panel „Matter“ mit Status, Matter-Geräteliste und Dataset-Export
- [ ] Auf Hardware testen:
      - `GET /api/thread/srp` → `"state": "running"`
      - Matter-Gerät (z. B. esp-matter `light` auf ESP32-H2/C6) per `chip-tool` mit dem
        exportierten Dataset in Betrieb nehmen
      - im LAN: `dns-sd -B _matter._tcp` bzw. `avahi-browse -r _matter._tcp` muss das Gerät zeigen
        (Advertising Proxy der ESP-Thread-Lib)
      - NAT64: `ping 8.8.8.8` vom Thread-Gerät; ohne NAT64 im Heimnetz bleibt der
        Prefix-Manager ggf. `Idle`
- [ ] Optional: OTBR-REST-kompatible Endpoints (`/node/dataset/active`, `/node/state`, …)
      → Integration in Home Assistant „OpenThread Border Router“

### Stufe 2 – TBR als Matter-Gerät (esp-matter)

- [ ] `espressif/esp_matter` als Abhängigkeit (Partitionen: ota_0/ota_1 mit ~6,7 MB reichen)
- [ ] Endpoint mit *Thread Border Router Management Cluster* (Matter 1.4) +
      *Thread Network Directory Cluster* → Controller kann Dataset/Credentials teilen
- [ ] Commissioning über BLE (ESP32-S31 hat BLE) oder On-Network über Ethernet
- [ ] Matter-OTA vs. bestehende GitHub-OTA abgleichen

### Stufe 3 – Matter-Controller auf dem TBR (optional)

- [ ] esp-matter Controller: Geräte direkt über die Web-UI in Betrieb nehmen und steuern
- [ ] Ablösung/Ergänzung der CoAP-Light-Steuerung durch Matter On/Off/Level

## Risiken / offene Punkte

- Speicher: Matter-Stack + OpenThread-BR + HTTP-Server + mDNS → PSRAM ist aktiviert, RAM-Budget prüfen
- ESP-IDF `master`/Preview + esp-matter-Versionskompatibilität für `esp32s31`
- Matter-Zertifizierung (DAC/PAI) nur für Test-Zertifikate im Hobby-Umfeld
