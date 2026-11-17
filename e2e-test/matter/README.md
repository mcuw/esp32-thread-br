# Bruno-Requests: Matter-ready Border Router (Stufe 1)

Collection in [Bruno](https://www.usebruno.com/) öffnen, unter *Collection Settings → Headers*
`X-Setup-Token` setzen (Token aus der Web-UI).

| Request | Erwartung |
|---|---|
| `srp-status` | `200`, `"state": "running"`, `"auto_enable": true`; nach Matter-Commissioning `matter_operational >= 1` und Host mit `"matter": true` |
| `border-routing` | `200`, `"state": "running"`, `favored_omr_prefix` gesetzt; `nat64.prefix_manager_state` = `Active` (NAT64 im Heimnetz) oder `Idle` |
| `dataset-tlvs` | `200`, `{"tlvs": "0e08…"}` – Wert für `chip-tool pairing ble-thread <node-id> hex:<tlvs> <pin> <discriminator>` |
| `dataset-tlvs-unauthorized` | `401`, `{"error": "invalid or missing X-Setup-Token header"}` |
