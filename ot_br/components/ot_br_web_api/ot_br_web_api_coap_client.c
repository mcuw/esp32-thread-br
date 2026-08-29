#include <string.h>
#include "openthread/coap.h"
#include "openthread/ip6.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "openthread/netdiag.h"
#include "freertos/semphr.h"

#include "ot_coap_client.h"

static const char *TAG = "ot_br_coap_client";
static SemaphoreHandle_t s_response_sem = NULL;
static otCoapCode s_last_response_code = 0;

// for Diagnostic-Get-request
static SemaphoreHandle_t s_diag_sem = NULL;
static char s_resolved_omr[48] = {0};

// CoAP-Testing
typedef struct {
    otCoapCode code;
    char payload[256];
    bool has_payload;
} ot_br_coap_response_t;
static ot_br_coap_response_t s_generic_response;


static void coap_response_handler(void *ctx, otMessage *message,
                                   const otMessageInfo *info, otError result)
{
    if (result == OT_ERROR_NONE) {
        s_last_response_code = otCoapMessageGetCode(message);
    } else {
        s_last_response_code = 0;
        ESP_LOGW(TAG, "CoAP-Request fehlgeschlagen: %d", result);
    }
    xSemaphoreGive(s_response_sem);
}

void ot_br_coap_client_init(void)
{
    s_response_sem = xSemaphoreCreateBinary();

    otInstance *instance = esp_openthread_get_instance();

    esp_openthread_lock_acquire(portMAX_DELAY);
    esp_err_t err = ot_coap_client_ensure_started(instance);
    esp_openthread_lock_release();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start CoAP-Client");
    }
}

esp_err_t ot_br_coap_light_set(const char *address_str, bool on, uint8_t r, uint8_t g, uint8_t b)
{
    otInstance *instance = esp_openthread_get_instance();

    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "on", on);
    cJSON_AddNumberToObject(j, "r", r);
    cJSON_AddNumberToObject(j, "g", g);
    cJSON_AddNumberToObject(j, "b", b);
    char *payload = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);

    esp_openthread_lock_acquire(portMAX_DELAY);

    otMessage *message = otCoapNewMessage(instance, NULL);
    if (!message) {
        esp_openthread_lock_release();
        free(payload);
        return ESP_ERR_NO_MEM;
    }

    otCoapMessageInit(message, OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_PUT);
    otCoapMessageGenerateToken(message, OT_COAP_DEFAULT_TOKEN_LENGTH);
    otCoapMessageAppendUriPathOptions(message, "light");
    otCoapMessageAppendContentFormatOption(message, OT_COAP_OPTION_CONTENT_FORMAT_JSON);
    otCoapMessageSetPayloadMarker(message);
    otMessageAppend(message, payload, strlen(payload));
    free(payload);

    otMessageInfo msgInfo;
    memset(&msgInfo, 0, sizeof(msgInfo));
    otError addr_err = otIp6AddressFromString(address_str, &msgInfo.mPeerAddr);
    if (addr_err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "Ungueltige Zieladresse: '%s'", address_str);
        otMessageFree(message);
        esp_openthread_lock_release();
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Sende CoAP-Request an: %s", address_str);
    msgInfo.mPeerPort = OT_DEFAULT_COAP_PORT;  // 5683

    otError err = otCoapSendRequest(instance, message, &msgInfo,
                                     coap_response_handler, NULL);
    esp_openthread_lock_release();

    if (err != OT_ERROR_NONE) {
        ESP_LOGW(TAG, "CoAP-Request senden fehlgeschlagen: %d", err);
        return ESP_FAIL;
    }

    // Auf Antwort warten (max. 3 Sekunden, CoAP kuemmert sich intern um Retransmission)
    if (xSemaphoreTake(s_response_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return (s_last_response_code == OT_COAP_CODE_CHANGED) ? ESP_OK : ESP_FAIL;
}

static void diagnostic_callback(otError err, otMessage *message,
                                 const otMessageInfo *info, void *ctx)
{
    s_resolved_omr[0] = '\0';

    if (err == OT_ERROR_NONE && message) {
        otNetworkDiagTlv tlv;
        otNetworkDiagIterator iterator = OT_NETWORK_DIAGNOSTIC_ITERATOR_INIT;

        while (otThreadGetNextDiagnosticTlv(message, &iterator, &tlv) == OT_ERROR_NONE) {
            if (tlv.mType == OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST) {
                for (uint8_t i = 0; i < tlv.mData.mIp6AddrList.mCount; i++) {
                    otIp6Address *addr = &tlv.mData.mIp6AddrList.mList[i];
                    // OMR-Praefix des eigenen BR holen und Adresse damit vergleichen
                    otInstance *instance = esp_openthread_get_instance();
                    otIp6Prefix omr;
                    otError omr_err = otBorderRoutingGetOmrPrefix(instance, &omr);
                    if (omr_err == OT_ERROR_NONE &&
                        memcmp(addr->mFields.m8, omr.mPrefix.mFields.m8, omr.mLength / 8) == 0) {
                        otIp6AddressToString(addr, s_resolved_omr, sizeof(s_resolved_omr));
                        break;
                    }
                }
            }
        }
    }

    xSemaphoreGive(s_diag_sem);
}

esp_err_t ot_br_resolve_omr_address(const char *rloc_address_str, char *out, size_t out_len)
{
    if (!s_diag_sem) {
        s_diag_sem = xSemaphoreCreateBinary();
    }
    s_resolved_omr[0] = '\0';

    otInstance *instance = esp_openthread_get_instance();
    otIp6Address dest;

    esp_openthread_lock_acquire(portMAX_DELAY);
    otIp6AddressFromString(rloc_address_str, &dest);

    uint8_t tlv_types[] = { OT_NETWORK_DIAGNOSTIC_TLV_IP6_ADDR_LIST };
    otError err = otThreadSendDiagnosticGet(instance, &dest, tlv_types, 1,
                                             diagnostic_callback, NULL);
    esp_openthread_lock_release();

    if (err != OT_ERROR_NONE) {
        return ESP_FAIL;
    }

    if (xSemaphoreTake(s_diag_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (s_resolved_omr[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }

    strncpy(out, s_resolved_omr, out_len - 1);
    out[out_len - 1] = '\0';
    return ESP_OK;
}

//
// CoAP-Testing
//
static void generic_response_handler(void *ctx, otMessage *message,
                                      const otMessageInfo *info, otError result)
{
    if (result == OT_ERROR_NONE && message) {
        s_generic_response.code = otCoapMessageGetCode(message);
        uint16_t offset = otMessageGetOffset(message);
        uint16_t length = otMessageGetLength(message) - offset;
        if (length > 0 && length < sizeof(s_generic_response.payload)) {
            otMessageRead(message, offset, s_generic_response.payload, length);
            s_generic_response.payload[length] = '\0';
            s_generic_response.has_payload = true;
        } else {
            s_generic_response.has_payload = false;
        }
    } else {
        s_generic_response.code = 0;
        s_generic_response.has_payload = false;
    }
    xSemaphoreGive(s_response_sem);
}

esp_err_t ot_br_coap_generic_request(const char *address_str, otCoapCode method,
                                      const char *uri_path, const char *payload,
                                      ot_br_coap_response_t *out_response)
{
    otInstance *instance = esp_openthread_get_instance();
    xSemaphoreTake(s_response_sem, 0);

    esp_openthread_lock_acquire(portMAX_DELAY);

    otMessage *message = otCoapNewMessage(instance, NULL);
    if (!message) {
        esp_openthread_lock_release();
        return ESP_ERR_NO_MEM;
    }

    otCoapType type = OT_COAP_TYPE_CONFIRMABLE;
    otCoapMessageInit(message, type, method);
    otCoapMessageGenerateToken(message, OT_COAP_DEFAULT_TOKEN_LENGTH);
    otCoapMessageAppendUriPathOptions(message, uri_path);

    if (payload && strlen(payload) > 0 &&
        (method == OT_COAP_CODE_PUT || method == OT_COAP_CODE_POST)) {
        otCoapMessageAppendContentFormatOption(message, OT_COAP_OPTION_CONTENT_FORMAT_JSON);
        otCoapMessageSetPayloadMarker(message);
        otMessageAppend(message, payload, strlen(payload));
    }

    otMessageInfo msgInfo;
    memset(&msgInfo, 0, sizeof(msgInfo));
    otError addr_err = otIp6AddressFromString(address_str, &msgInfo.mPeerAddr);
    if (addr_err != OT_ERROR_NONE) {
        otMessageFree(message);
        esp_openthread_lock_release();
        return ESP_ERR_INVALID_ARG;
    }
    msgInfo.mPeerPort = OT_DEFAULT_COAP_PORT;

    otCoapTxParameters tx_params = {
        .mAckTimeout = 2000,
        .mAckRandomFactorNumerator = 3,
        .mAckRandomFactorDenominator = 2,
        .mMaxRetransmit = 2,
    };

    otError err = otCoapSendRequestWithParameters(instance, message, &msgInfo,
                                                    generic_response_handler, NULL, &tx_params);
    esp_openthread_lock_release();

    if (err != OT_ERROR_NONE) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(s_response_sem, pdMS_TO_TICKS(8000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    *out_response = s_generic_response;
    return ESP_OK;
}