#include <string.h>
#include "openthread/coap.h"
#include "openthread/udp.h"
#include "openthread/ip6.h"
#include "openthread/thread.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include "ot_br_web_api_auth.h"

#include "ot_br_web_api_internal.h"

#include "ot_br_web_api_coap_client.h"

static const char *TAG = "ot_br_command";
#define COMMAND_PORT 12345

static otUdpSocket s_command_socket;
static bool s_socket_ready = false;

void ot_br_command_init(void)
{
    otInstance *instance = esp_openthread_get_instance();

    otSockAddr sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    // Kein fester Port fuer den Sendenden Socket noetig - System vergibt einen freien Port

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otUdpOpen(instance, &s_command_socket, NULL, NULL);
    esp_openthread_lock_release();

    if (err == OT_ERROR_NONE) {
        s_socket_ready = true;
        ESP_LOGI(TAG, "Command-Socket bereit");
    } else {
        ESP_LOGE(TAG, "Konnte Command-Socket nicht oeffnen: %d", err);
    }
}

esp_err_t ot_br_command_send(const char *address_str, const char *command)
{
    if (!s_socket_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    otInstance *instance = esp_openthread_get_instance();
    otMessageInfo msgInfo;
    memset(&msgInfo, 0, sizeof(msgInfo));

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otIp6AddressFromString(address_str, &msgInfo.mPeerAddr);
    if (err != OT_ERROR_NONE) {
        esp_openthread_lock_release();
        return ESP_ERR_INVALID_ARG;
    }
    msgInfo.mPeerPort = COMMAND_PORT;

    otMessage *message = otUdpNewMessage(instance, NULL);
    esp_err_t result = ESP_FAIL;
    if (message) {
        otMessageAppend(message, command, strlen(command));
        err = otUdpSend(instance, &s_command_socket, message, &msgInfo);
        result = (err == OT_ERROR_NONE) ? ESP_OK : ESP_FAIL;
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Senden fehlgeschlagen: %d", err);
        }
    }
    esp_openthread_lock_release();

    return result;
}

// Berechnet die RLOC-IPv6-Adresse eines Nachbarn aus Mesh-Local-Prefix + RLOC16.
// Format: <mesh-local-prefix>:0000:00ff:fe00:<rloc16>
void ot_br_command_build_rloc_address(uint16_t rloc16, char *out, size_t out_len)
{
    otInstance *instance = esp_openthread_get_instance();

    esp_openthread_lock_acquire(portMAX_DELAY);
    const otMeshLocalPrefix *prefix = otThreadGetMeshLocalPrefix(instance);

    otIp6Address addr;
    memset(&addr, 0, sizeof(addr));
    memcpy(addr.mFields.m8, prefix->m8, 8);
    addr.mFields.m8[8] = 0x00;
    addr.mFields.m8[9] = 0x00;
    addr.mFields.m8[10] = 0x00;
    addr.mFields.m8[11] = 0xff;
    addr.mFields.m8[12] = 0xfe;
    addr.mFields.m8[13] = 0x00;
    addr.mFields.m8[14] = (rloc16 >> 8) & 0xff;
    addr.mFields.m8[15] = rloc16 & 0xff;

    otIp6AddressToString(&addr, out, out_len);
    esp_openthread_lock_release();
}

// POST /api/thread/send-command
// Body: {"address": "fd36:...", "command": "TOGGLE"}
esp_err_t ot_br_command_send_handler(httpd_req_t *req)
{
    if (!ot_br_web_api_check_auth(req)) return ESP_OK;

    char *body = read_body(req);
    if (!body) {
        send_json_error(req, "400 Bad Request", "missing body");
        return ESP_OK;
    }
    cJSON *json = cJSON_Parse(body);
    free(body);
    if (!json) {
        send_json_error(req, "400 Bad Request", "invalid JSON");
        return ESP_OK;
    }

    cJSON *addr_item = cJSON_GetObjectItem(json, "address");
    cJSON *cmd_item = cJSON_GetObjectItem(json, "command");

    if (!cJSON_IsString(addr_item) || !cJSON_IsString(cmd_item)) {
        cJSON_Delete(json);
        send_json_error(req, "400 Bad Request", "address and command required");
        return ESP_OK;
    }

    esp_err_t err = ot_br_command_send(addr_item->valuestring, cmd_item->valuestring);
    cJSON_Delete(json);

    if (err != ESP_OK) {
        send_json_error(req, "500 Internal Server Error", "failed to send command");
        return ESP_OK;
    }

    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "success", true);
    send_json(req, j);
    return ESP_OK;
}

esp_err_t ot_br_coap_light_handler(httpd_req_t *req)
{
    if (!ot_br_web_api_check_auth(req)) return ESP_OK;

    char *body = read_body(req);
    if (!body) {
        send_json_error(req, "400 Bad Request", "missing body");
        return ESP_OK;
    }
    cJSON *json = cJSON_Parse(body);
    free(body);
    if (!json) {
        send_json_error(req, "400 Bad Request", "invalid JSON");
        return ESP_OK;
    }

    cJSON *addr_item = cJSON_GetObjectItem(json, "address");
    cJSON *on_item = cJSON_GetObjectItem(json, "on");
    cJSON *r_item = cJSON_GetObjectItem(json, "r");
    cJSON *g_item = cJSON_GetObjectItem(json, "g");
    cJSON *b_item = cJSON_GetObjectItem(json, "b");

    if (!cJSON_IsString(addr_item)) {
        cJSON_Delete(json);
        send_json_error(req, "400 Bad Request", "address required");
        return ESP_OK;
    }

    bool on = cJSON_IsBool(on_item) ? cJSON_IsTrue(on_item) : true;
    uint8_t r = cJSON_IsNumber(r_item) ? (uint8_t)r_item->valueint : 255;
    uint8_t g = cJSON_IsNumber(g_item) ? (uint8_t)g_item->valueint : 255;
    uint8_t b = cJSON_IsNumber(b_item) ? (uint8_t)b_item->valueint : 255;
    char address[48];
    strncpy(address, addr_item->valuestring, sizeof(address) - 1);
    address[sizeof(address) - 1] = '\0';

    cJSON_Delete(json);

    char resolved_address[48];
    esp_err_t resolve_err = ot_br_resolve_omr_address(address, resolved_address, sizeof(resolved_address));

    const char *target = (resolve_err == ESP_OK) ? resolved_address : address;  // Fallback auf RLOC
    esp_err_t err = ot_br_coap_light_set(target, on, r, g, b);  

    if (err != ESP_OK) {
        const char *msg = (err == ESP_ERR_TIMEOUT) ? "Device does not respond (Timeout)" : "CoAP-Request failed";
        send_json_error(req, "504 Gateway Timeout", msg);
        return ESP_OK;
    }

    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "success", true);
    send_json(req, j);
    return ESP_OK;
}

//
// CoAP-Testing-Command
// 

static otCoapCode method_from_string(const char *m)
{
    if (strcmp(m, "GET") == 0) return OT_COAP_CODE_GET;
    if (strcmp(m, "PUT") == 0) return OT_COAP_CODE_PUT;
    if (strcmp(m, "POST") == 0) return OT_COAP_CODE_POST;
    if (strcmp(m, "DELETE") == 0) return OT_COAP_CODE_DELETE;
    return OT_COAP_CODE_GET;
}

// POST /api/thread/coap-request
// Body: {"address":"...", "method":"GET|PUT|POST|DELETE", "path":"light", "payload":"{...}"}
esp_err_t ot_br_coap_generic_handler(httpd_req_t *req)
{
    if (!ot_br_web_api_check_auth(req)) return ESP_OK;

    char *body = read_body(req);
    if (!body) {
        send_json_error(req, "400 Bad Request", "missing body");
        return ESP_OK;
    }
    cJSON *json = cJSON_Parse(body);
    free(body);
    if (!json) {
        send_json_error(req, "400 Bad Request", "invalid JSON");
        return ESP_OK;
    }

    cJSON *addr_item = cJSON_GetObjectItem(json, "address");
    cJSON *method_item = cJSON_GetObjectItem(json, "method");
    cJSON *path_item = cJSON_GetObjectItem(json, "path");
    cJSON *payload_item = cJSON_GetObjectItem(json, "payload");

    if (!cJSON_IsString(addr_item) || !cJSON_IsString(path_item)) {
        cJSON_Delete(json);
        send_json_error(req, "400 Bad Request", "address and path required");
        return ESP_OK;
    }

    char address[48];
    strncpy(address, addr_item->valuestring, sizeof(address) - 1);
    address[sizeof(address) - 1] = '\0';

    char path[64];
    strncpy(path, path_item->valuestring, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    otCoapCode method = cJSON_IsString(method_item)
        ? method_from_string(method_item->valuestring) : OT_COAP_CODE_GET;

    char payload[256] = {0};
    if (cJSON_IsString(payload_item)) {
        strncpy(payload, payload_item->valuestring, sizeof(payload) - 1);
    }
    cJSON_Delete(json);

    // Adresse ueber OMR aufloesen, mit RLOC-Fallback (wie bei /coap-light)
    char resolved_address[48];
    esp_err_t resolve_err = ot_br_resolve_omr_address(address, resolved_address, sizeof(resolved_address));
    const char *target = (resolve_err == ESP_OK) ? resolved_address : address;

    ot_br_coap_response_t response;
    esp_err_t err = ot_br_coap_generic_request(target, method, path, payload, &response);

    if (err != ESP_OK) {
        const char *msg = (err == ESP_ERR_TIMEOUT) ? "Geraet antwortet nicht (Timeout)" : "CoAP-Request fehlgeschlagen";
        send_json_error(req, "504 Gateway Timeout", msg);
        return ESP_OK;
    }

    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "success", true);
    cJSON_AddNumberToObject(j, "code", response.code);
    if (response.has_payload) {
        cJSON_AddStringToObject(j, "payload", response.payload);
    }
    send_json(req, j);
    return ESP_OK;
}