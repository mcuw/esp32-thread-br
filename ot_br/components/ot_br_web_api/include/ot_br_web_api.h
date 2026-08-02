#pragma once

#include "esp_err.h"
#include "openthread/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

// Startet den HTTP-Server mit allen REST-Endpunkten.
// Muss aufgerufen werden, NACHDEM die OpenThread-Instanz bereits läuft
// (esp_openthread_start()).
esp_err_t ot_br_web_api_start(otInstance *instance);

// Gibt den aktuellen Setup-Token zurück (z.B. für Debug-Zwecke).
const char *ot_br_web_api_get_setup_token(void);

#ifdef __cplusplus
}
#endif