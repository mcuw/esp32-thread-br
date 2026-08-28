#pragma once

#include "esp_err.h"
#include "openthread/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ensure the OpenThread CoAP layer's UDP socket is open.
 *
 * otCoapSendRequest() requires otCoapStart() to have been called at
 * least once -- even for pure client usage that never registers a
 * resource -- otherwise it fails with OT_ERROR_INVALID_STATE. This
 * wrapper makes that call exactly once per boot and is safe to call
 * from every place that is about to send a CoAP request.
 *
 * Must be called with the OpenThread lock already held by the caller
 * (esp_openthread_lock_acquire()), same as any other otXxx() call.
 *
 * @param instance  A valid OpenThread instance (e.g. from
 *                  esp_openthread_get_instance()).
 * @return ESP_OK if the CoAP layer is started (either just now or
 *         already before), ESP_FAIL if otCoapStart() itself failed.
 */
esp_err_t ot_coap_client_ensure_started(otInstance *instance);

#ifdef __cplusplus
}
#endif
