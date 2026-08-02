#include "esp_http_server.h"
#include "esp_log.h"
#include "ot_br_web_api.h"

static const char *TAG = "ot_br_web_api";

extern void ot_br_web_api_auth_init(void);
extern void ot_br_web_api_register_handlers(httpd_handle_t server);

esp_err_t ot_br_web_api_start(otInstance *instance)
{
    ot_br_web_api_auth_init();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 16;

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    ot_br_web_api_register_handlers(server);
    ESP_LOGI(TAG, "Web API gestartet auf Port %d", config.server_port);
    return ESP_OK;
}