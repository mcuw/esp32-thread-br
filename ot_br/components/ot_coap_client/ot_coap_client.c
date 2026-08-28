#include "ot_coap_client.h"

#include "esp_log.h"
#include "openthread/coap.h"

static const char *TAG = "ot_coap_client";
static bool s_coap_started = false;

esp_err_t ot_coap_client_ensure_started(otInstance *instance)
{
    if (s_coap_started) {
        return ESP_OK;
    }

    otError error = otCoapStart(instance, OT_DEFAULT_COAP_PORT);
    if (error != OT_ERROR_NONE && error != OT_ERROR_ALREADY) {
        ESP_LOGE(TAG, "otCoapStart failed: %s", otThreadErrorToString(error));
        return ESP_FAIL;
    }

    s_coap_started = true;
    ESP_LOGI(TAG, "CoAP client layer started on port %d", OT_DEFAULT_COAP_PORT);
    return ESP_OK;
}