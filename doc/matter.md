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

- [ ] SRP-Server + Advertising Proxy explizit aktivieren/prüfen
      (`ot srp server state`, `_matter._tcp` im LAN per `dns-sd -B _matter._tcp`)
- [ ] NAT64 aktivieren (`CONFIG_OPENTHREAD_NAT64`), DNS64 Upstream
- [ ] Dataset als TLV-Hex für Controller abrufbar (`chip-tool pairing ble-thread … hex:<tlvs>`)
- [ ] Web-UI: SRP-Hosts/Services anzeigen (neuer Endpoint `/api/thread/srp`)
- [ ] Optional: OTBR-REST-kompatible Endpoints (`/node/dataset/active`, `/node/state`, …)
      → Integration in Home Assistant „OpenThread Border Router“
- [ ] Test: Matter-Gerät (z. B. esp-matter `light` auf ESP32-H2/C6) via Controller in dieses Thread-Netz bringen

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
