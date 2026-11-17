#pragma once

#include "esp_http_server.h"
#include "openthread/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Aktiviert die fuer Matter-over-Thread benoetigten Border-Router-Dienste
 * (SRP-Server Auto-Enable, NAT64). Muss mit gehaltenem OpenThread-Lock
 * nach esp_openthread_border_router_init() aufgerufen werden.
 */
void ot_br_matter_ready_init(otInstance *instance);

/** Registriert /api/thread/srp, /api/thread/border-routing, /api/thread/dataset/tlvs */
void ot_br_web_api_register_matter_handlers(httpd_handle_t server);

#ifdef __cplusplus
}
#endif
