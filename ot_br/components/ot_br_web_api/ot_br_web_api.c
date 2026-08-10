#include "esp_http_server.h"
#include "esp_log.h"
#include "ot_br_web_api.h"

static const char *TAG = "ot_br_web_api";


esp_err_t ot_br_web_api_start(otInstance *instance)
{
    ot_br_web_api_auth_init();

    esp_err_t err = ot_br_web_api_mount_littlefs();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LittleFS nicht verfuegbar - UI wird nicht ausgeliefert, API bleibt aktiv");
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 20;

    httpd_handle_t server = NULL;
    esp_err_t start_err = httpd_start(&server, &config);
    if (start_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(start_err));
        return start_err;
    }

    ot_br_web_api_register_handlers(server);   // /api/* -- register FIRST

    if (err == ESP_OK) {
        ot_br_web_api_register_static_handler(server);  // "/*" -- AT LEAST, after all other handlers
    }

    ESP_LOGI(TAG, "Web API + UI gestartet auf Port %d", config.server_port);
    return ESP_OK;
}