#include <string.h>
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