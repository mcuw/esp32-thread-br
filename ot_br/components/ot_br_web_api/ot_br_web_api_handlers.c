#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "cJSON.h"
#include "openthread/thread.h"
#include "openthread/dataset.h"
#include "openthread/dataset_ftd.h"
#include "openthread/commissioner.h"
#include "openthread/joiner.h"
#include "esp_openthread_lock.h"

static const char *TAG = "ot_br_web_handlers";

extern bool ot_br_web_api_check_auth(httpd_req_t *req);

// -------- Hilfsfunktionen --------

static void send_json(httpd_req_t *req, cJSON *json)
{
    char *out = cJSON_PrintUnformatted(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    free(out);
    cJSON_Delete(json);
}

static void send_json_error(httpd_req_t *req, const char *status, const char *msg)
{
    httpd_resp_set_status(req, status);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "error", msg);
    send_json(req, j);
}

static char *read_body(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 2048) {
        return NULL;
    }
    char *buf = malloc(req->content_len + 1);
    if (!buf) return NULL;
    int received = httpd_req_recv(req, buf, req->content_len);
    if (received <= 0) {
        free(buf);
        return NULL;
    }
    buf[received] = '\0';
    return buf;
}

static void hex_encode(const uint8_t *in, size_t in_len, char *out)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < in_len; i++) {
        out[i * 2]     = hex[in[i] >> 4];
        out[i * 2 + 1] = hex[in[i] & 0x0F];
    }
    out[in_len * 2] = '\0';
}

// -------- GET /api/device/info --------

static esp_err_t device_info_handler(httpd_req_t *req)
{
    cJSON *j = cJSON_CreateObject();

    uint8_t eui64[8];
    esp_read_mac(eui64, ESP_MAC_IEEE802154);
    char eui64_str[17];
    hex_encode(eui64, 8, eui64_str);
    cJSON_AddStringToObject(j, "eui64", eui64_str);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    cJSON_AddStringToObject(j, "firmware_version", app_desc->version);
    cJSON_AddStringToObject(j, "idf_version", app_desc->idf_ver);
    cJSON_AddStringToObject(j, "project_name", app_desc->project_name);

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddStringToObject(j, "chip_model", CONFIG_IDF_TARGET);
    cJSON_AddNumberToObject(j, "chip_revision", chip_info.revision);
    cJSON_AddNumberToObject(j, "free_heap", esp_get_free_heap_size());

    send_json(req, j);
    return ESP_OK;
}

// -------- GET /api/thread/state --------

static esp_err_t thread_state_handler(httpd_req_t *req)
{
    otInstance *instance = otInstanceInitSingle();  // liefert bestehende Instanz zurück

    esp_openthread_lock_acquire(portMAX_DELAY);
    otDeviceRole role = otThreadGetDeviceRole(instance);
    uint16_t rloc16 = otThreadGetRloc16(instance);
    bool commissioned = otDatasetIsCommissioned(instance);
    esp_openthread_lock_release();

    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "role", otThreadDeviceRoleToString(role));
    cJSON_AddNumberToObject(j, "rloc16", rloc16);
    cJSON_AddBoolToObject(j, "commissioned", commissioned);

    send_json(req, j);
    return ESP_OK;
}

// -------- GET /api/thread/dataset (maskiert, ohne Auth) --------

static esp_err_t thread_dataset_masked_handler(httpd_req_t *req)
{
    otInstance *instance = otInstanceInitSingle();
    otOperationalDataset dataset;

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otDatasetGetActive(instance, &dataset);
    esp_openthread_lock_release();

    if (err != OT_ERROR_NONE) {
        send_json_error(req, "404 Not Found", "no active dataset");
        return ESP_OK;
    }

    cJSON *j = cJSON_CreateObject();
    if (dataset.mComponents.mIsNetworkNamePresent) {
        cJSON_AddStringToObject(j, "network_name", dataset.mNetworkName.m8);
    }
    if (dataset.mComponents.mIsChannelPresent) {
        cJSON_AddNumberToObject(j, "channel", dataset.mChannel);
    }
    if (dataset.mComponents.mIsPanIdPresent) {
        cJSON_AddNumberToObject(j, "panid", dataset.mPanId);
    }
    if (dataset.mComponents.mIsExtendedPanIdPresent) {
        char extpanid[17];
        hex_encode(dataset.mExtendedPanId.m8, 8, extpanid);
        cJSON_AddStringToObject(j, "extpanid", extpanid);
    }
    // Bewusst NICHT enthalten: mNetworkKey, mPskc

    send_json(req, j);
    return ESP_OK;
}

// -------- GET /api/thread/dataset/full (mit Network Key, Auth erforderlich) --------

static esp_err_t thread_dataset_full_handler(httpd_req_t *req)
{
    if (!ot_br_web_api_check_auth(req)) return ESP_OK;

    otInstance *instance = otInstanceInitSingle();
    otOperationalDataset dataset;

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otDatasetGetActive(instance, &dataset);
    esp_openthread_lock_release();

    if (err != OT_ERROR_NONE) {
        send_json_error(req, "404 Not Found", "no active dataset");
        return ESP_OK;
    }

    cJSON *j = cJSON_CreateObject();
    if (dataset.mComponents.mIsNetworkNamePresent) {
        cJSON_AddStringToObject(j, "network_name", dataset.mNetworkName.m8);
    }
    if (dataset.mComponents.mIsChannelPresent) {
        cJSON_AddNumberToObject(j, "channel", dataset.mChannel);
    }
    if (dataset.mComponents.mIsPanIdPresent) {
        cJSON_AddNumberToObject(j, "panid", dataset.mPanId);
    }
    if (dataset.mComponents.mIsNetworkKeyPresent) {
        char key[33];
        hex_encode(dataset.mNetworkKey.m8, 16, key);
        cJSON_AddStringToObject(j, "network_key", key);
    }
    if (dataset.mComponents.mIsPskcPresent) {
        char pskc[33];
        hex_encode(dataset.mPskc.m8, 16, pskc);
        cJSON_AddStringToObject(j, "pskc", pskc);
    }

    send_json(req, j);
    return ESP_OK;
}

// -------- POST /api/thread/dataset/init (Auth erforderlich) --------
// Body: {"network_name": "MyThreadNet", "channel": 15}

static esp_err_t thread_dataset_init_handler(httpd_req_t *req)
{
    if (!ot_br_web_api_check_auth(req)) return ESP_OK;

    char *body = read_body(req);
    if (!body) {
        send_json_error(req, "400 Bad Request", "missing or too large body");
        return ESP_OK;
    }

    cJSON *json = cJSON_Parse(body);
    free(body);
    if (!json) {
        send_json_error(req, "400 Bad Request", "invalid JSON");
        return ESP_OK;
    }

    otInstance *instance = otInstanceInitSingle();
    otOperationalDataset dataset;
    memset(&dataset, 0, sizeof(dataset));

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otDatasetCreateNewNetwork(instance, &dataset);

    cJSON *name_item = cJSON_GetObjectItem(json, "network_name");
    if (cJSON_IsString(name_item)) {
        strncpy(dataset.mNetworkName.m8, name_item->valuestring, OT_NETWORK_NAME_MAX_SIZE);
        dataset.mComponents.mIsNetworkNamePresent = true;
    }

    cJSON *channel_item = cJSON_GetObjectItem(json, "channel");
    if (cJSON_IsNumber(channel_item)) {
        dataset.mChannel = (uint16_t)channel_item->valueint;
        dataset.mComponents.mIsChannelPresent = true;
    }

    if (err == OT_ERROR_NONE) {
        err = otDatasetSetActive(instance, &dataset);
    }
    esp_openthread_lock_release();
    cJSON_Delete(json);

    if (err != OT_ERROR_NONE) {
        send_json_error(req, "500 Internal Server Error", "failed to create/set dataset");
        return ESP_OK;
    }

    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "success", true);
    send_json(req, j);
    return ESP_OK;
}

// -------- POST /api/thread/start (Auth erforderlich) --------

static esp_err_t thread_start_handler(httpd_req_t *req)
{
    if (!ot_br_web_api_check_auth(req)) return ESP_OK;

    otInstance *instance = otInstanceInitSingle();

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err1 = otIp6SetEnabled(instance, true);
    otError err2 = otThreadSetEnabled(instance, true);
    esp_openthread_lock_release();

    if (err1 != OT_ERROR_NONE || err2 != OT_ERROR_NONE) {
        send_json_error(req, "500 Internal Server Error", "failed to start thread");
        return ESP_OK;
    }

    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "success", true);
    send_json(req, j);
    return ESP_OK;
}

// -------- POST /api/thread/stop (Auth erforderlich) --------

static esp_err_t thread_stop_handler(httpd_req_t *req)
{
    if (!ot_br_web_api_check_auth(req)) return ESP_OK;

    otInstance *instance = otInstanceInitSingle();

    esp_openthread_lock_acquire(portMAX_DELAY);
    otThreadSetEnabled(instance, false);
    otIp6SetEnabled(instance, false);
    esp_openthread_lock_release();

    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "success", true);
    send_json(req, j);
    return ESP_OK;
}

// -------- GET /api/thread/neighbors --------

static esp_err_t thread_neighbors_handler(httpd_req_t *req)
{
    otInstance *instance = otInstanceInitSingle();
    cJSON *arr = cJSON_CreateArray();

    esp_openthread_lock_acquire(portMAX_DELAY);
    otNeighborInfoIterator iter = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    otNeighborInfo info;
    while (otThreadGetNextNeighborInfo(instance, &iter, &info) == OT_ERROR_NONE) {
        cJSON *n = cJSON_CreateObject();
        cJSON_AddNumberToObject(n, "rloc16", info.mRloc16);
        char ext_mac[17];
        hex_encode(info.mExtAddress.m8, 8, ext_mac);
        cJSON_AddStringToObject(n, "ext_mac", ext_mac);
        cJSON_AddNumberToObject(n, "avg_rssi", info.mAverageRssi);
        cJSON_AddNumberToObject(n, "last_rssi", info.mLastRssi);
        cJSON_AddNumberToObject(n, "age", info.mAge);
        cJSON_AddBoolToObject(n, "is_child", info.mIsChild);
        cJSON_AddBoolToObject(n, "rx_on_when_idle", info.mRxOnWhenIdle);
        cJSON_AddItemToArray(arr, n);
    }
    esp_openthread_lock_release();

    send_json(req, arr);
    return ESP_OK;
}

// -------- POST /api/thread/commissioner/start (Auth erforderlich) --------

static esp_err_t commissioner_start_handler(httpd_req_t *req)
{
    if (!ot_br_web_api_check_auth(req)) return ESP_OK;

    otInstance *instance = otInstanceInitSingle();

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otCommissionerStart(instance, NULL, NULL, NULL);
    esp_openthread_lock_release();

    if (err != OT_ERROR_NONE && err != OT_ERROR_ALREADY) {
        send_json_error(req, "500 Internal Server Error", "failed to start commissioner");
        return ESP_OK;
    }

    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "success", true);
    send_json(req, j);
    return ESP_OK;
}

// -------- POST /api/thread/commissioner/joiner (Auth erforderlich) --------
// Body: {"eui64": "*", "pskd": "J01NME"}

static esp_err_t commissioner_joiner_add_handler(httpd_req_t *req)
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

    cJSON *eui64_item = cJSON_GetObjectItem(json, "eui64");
    cJSON *pskd_item = cJSON_GetObjectItem(json, "pskd");

    if (!cJSON_IsString(pskd_item)) {
        cJSON_Delete(json);
        send_json_error(req, "400 Bad Request", "pskd required");
        return ESP_OK;
    }

    otInstance *instance = otInstanceInitSingle();
    otExtAddress ext_addr;
    bool use_any = !cJSON_IsString(eui64_item) || strcmp(eui64_item->valuestring, "*") == 0;

    if (!use_any) {
        // einfache Hex-String -> Byte-Array Konvertierung
        const char *s = eui64_item->valuestring;
        if (strlen(s) != 16) {
            cJSON_Delete(json);
            send_json_error(req, "400 Bad Request", "eui64 must be 16 hex chars or '*'");
            return ESP_OK;
        }
        for (int i = 0; i < 8; i++) {
            sscanf(s + i * 2, "%2hhx", &ext_addr.m8[i]);
        }
    }

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otCommissionerAddJoiner(instance, use_any ? NULL : &ext_addr,
                                           pskd_item->valuestring, 0);
    esp_openthread_lock_release();

    cJSON_Delete(json);

    if (err != OT_ERROR_NONE) {
        send_json_error(req, "500 Internal Server Error", "failed to add joiner");
        return ESP_OK;
    }

    cJSON *j = cJSON_CreateObject();
    cJSON_AddBoolToObject(j, "success", true);
    send_json(req, j);
    return ESP_OK;
}

// -------- Registrierung aller URIs --------

void ot_br_web_api_register_handlers(httpd_handle_t server)
{
    httpd_uri_t uris[] = {
        { "/api/device/info",                 HTTP_GET,  device_info_handler,             NULL },
        { "/api/thread/state",                HTTP_GET,  thread_state_handler,            NULL },
        { "/api/thread/dataset",              HTTP_GET,  thread_dataset_masked_handler,   NULL },
        { "/api/thread/dataset/full",         HTTP_GET,  thread_dataset_full_handler,     NULL },
        { "/api/thread/dataset/init",         HTTP_POST, thread_dataset_init_handler,     NULL },
        { "/api/thread/start",                HTTP_POST, thread_start_handler,            NULL },
        { "/api/thread/stop",                 HTTP_POST, thread_stop_handler,             NULL },
        { "/api/thread/neighbors",            HTTP_GET,  thread_neighbors_handler,        NULL },
        { "/api/thread/commissioner/start",   HTTP_POST, commissioner_start_handler,       NULL },
        { "/api/thread/commissioner/joiner",  HTTP_POST, commissioner_joiner_add_handler,  NULL },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }
}