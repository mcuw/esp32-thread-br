#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "mbedtls/base64.h"

static const char *TAG = "ot_br_web_auth";
#define NVS_NAMESPACE "web_setup"
#define NVS_KEY_TOKEN "setup_token"
#define TOKEN_RAW_BYTES 18   // -> 24 Base64-Zeichen, gut lesbar zum Abtippen

static char s_setup_token[32] = {0};

static void generate_and_store_token()
{
    uint8_t raw[TOKEN_RAW_BYTES];
    esp_fill_random(raw, sizeof(raw));

    size_t out_len = 0;
    mbedtls_base64_encode((unsigned char *)s_setup_token, sizeof(s_setup_token),
                           &out_len, raw, sizeof(raw));
    s_setup_token[out_len] = '\0';

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, NVS_KEY_TOKEN, s_setup_token);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void ot_br_web_api_auth_init()
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        size_t len = sizeof(s_setup_token);
        esp_err_t err = nvs_get_str(nvs, NVS_KEY_TOKEN, s_setup_token, &len);
        nvs_close(nvs);
        if (err == ESP_OK && strlen(s_setup_token) > 0) {
            ESP_LOGI(TAG, "Bestehenden Setup-Token aus NVS geladen");
            ESP_LOGW(TAG, "=== SETUP-TOKEN: %s ===", s_setup_token);
            return;
        }
    }

    generate_and_store_token();
    ESP_LOGW(TAG, "=== NEUER SETUP-TOKEN GENERIERT: %s ===", s_setup_token);
    ESP_LOGW(TAG, "=== Notiere ihn dir - er wird für kritische API-Aufrufe benoetigt ===");
}

const char *ot_br_web_api_get_setup_token()
{
    return s_setup_token;
}

// true = Zugriff erlaubt, false = 401 wurde bereits gesendet
bool ot_br_web_api_check_auth(httpd_req_t *req)
{
    char header[48] = {0};
    esp_err_t err = httpd_req_get_hdr_value_str(req, "X-Setup-Token", header, sizeof(header));

    if (err == ESP_OK && strcmp(header, s_setup_token) == 0) {
        return true;
    }

    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"error\":\"invalid or missing X-Setup-Token header\"}");
    return false;
}