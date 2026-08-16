#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
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

void ot_br_web_api_auth_init(void);

void ot_br_web_api_register_handlers(httpd_handle_t server);

// web: littlefs mounten to deliver the UI
esp_err_t ot_br_web_api_mount_littlefs(void);

// web: register static handler to deliver the UI (after all other handlers)
void ot_br_web_api_register_static_handler(httpd_handle_t server);

// CoAP Testing handler (POST /api/thread/coap-request)
esp_err_t ot_br_coap_generic_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif