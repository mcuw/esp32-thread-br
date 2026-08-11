#include <string.h>
#include <stdio.h>
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ot_br_web_api_ota.h"

static const char *TAG = "ot_br_ota";

static volatile ota_state_t s_state = OTA_STATE_IDLE;
static volatile int s_progress = 0;
static char s_error[160] = {0};
static char s_url[256] = {0};
static volatile int s_last_http_status = 0;

// Catch real HTTP-Statuscode from esp_https_ota, which is hidden by the internal handling of the esp_https_ota
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_HEADER) {
        s_last_http_status = esp_http_client_get_status_code(evt->client);
    }
    return ESP_OK;
}

// Map the HTTP-Statuscode to a human-readable message for the user
static const char *http_status_to_message(int status)
{
    switch (status) {
        case 200: return "OK";
        case 301:
        case 302: return "Weiterleitung nicht aufgeloest";
        case 401: return "Nicht autorisiert - privates Repo/Asset?";
        case 403: return "Zugriff verweigert (Rate-Limit oder privates Repo?)";
        case 404: return "Nicht gefunden - URL/Dateiname pruefen";
        case 500:
        case 502:
        case 503: return "Serverfehler beim Anbieter";
        default:  return "Unerwarteter Status";
    }
}

// Translate regular esp_err_t-Codes in verstaendliche Meldungen,
// ergaenzt um den zuletzt beobachteten HTTP-Status falls vorhanden.
static void format_error(char *out, size_t out_len, const char *phase, esp_err_t err)
{
    if (s_last_http_status > 0 && s_last_http_status != 200) {
        snprintf(out, out_len, "%s: HTTP %d (%s)",
                 phase, s_last_http_status, http_status_to_message(s_last_http_status));
        return;
    }

    const char *hint = "";
    switch (err) {
        case ESP_ERR_HTTP_CONNECT:
            hint = "Verbindung fehlgeschlagen - DNS, Netzwerk oder TLS-Zertifikat pruefen";
            break;
        case ESP_ERR_INVALID_ARG:
            hint = "Ungueltige URL oder Konfiguration";
            break;
        case ESP_ERR_OTA_VALIDATE_FAILED:
            hint = "Firmware-Image ungueltig (Signatur/Format)";
            break;
        case ESP_ERR_NO_MEM:
            hint = "Zu wenig Speicher fuer den Download";
            break;
        default:
            hint = esp_err_to_name(err);
            break;
    }
    snprintf(out, out_len, "%s: %s", phase, hint);
}


static void ota_task(void *pvParameter)
{
    s_state = OTA_STATE_IN_PROGRESS;
    s_progress = 0;
    s_error[0] = '\0';
    s_last_http_status = 0;

    esp_http_client_config_t http_config = {
        .url = s_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
        .buffer_size = 4096,       // Header read buffer size
        .buffer_size_tx = 2048,    // Send buffer (for long Auth-Header etc.)
        .event_handler = http_event_handler,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK) {
        format_error(s_error, sizeof(s_error), "Failed to begin OTA", err);
        ESP_LOGE(TAG, "%s", s_error);
        s_state = OTA_STATE_FAILED;
        vTaskDelete(NULL);
        return;
    }

    int total = esp_https_ota_get_image_size(handle);

    while (1) {
        err = esp_https_ota_perform(handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        int read = esp_https_ota_get_image_len_read(handle);
        if (total > 0) {
            s_progress = (read * 100) / total;
        }
    }

    if (err != ESP_OK || !esp_https_ota_is_complete_data_received(handle)) {
        format_error(s_error, sizeof(s_error), "Download incomplete", err);
        ESP_LOGE(TAG, "%s", s_error);
        esp_https_ota_abort(handle);
        s_state = OTA_STATE_FAILED;
        vTaskDelete(NULL);
        return;
    }

    err = esp_https_ota_finish(handle);
    if (err != ESP_OK) {
        format_error(s_error, sizeof(s_error), "Finish failed", err);
        ESP_LOGE(TAG, "%s", s_error);
        s_state = OTA_STATE_FAILED;
        vTaskDelete(NULL);
        return;
    }

    s_progress = 100;
    s_state = OTA_STATE_SUCCESS;
    ESP_LOGI(TAG, "OTA erfolgreich, Reboot in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}

esp_err_t ot_br_ota_start(const char *url)
{
    if (s_state == OTA_STATE_IN_PROGRESS) {
        return ESP_ERR_INVALID_STATE;
    }
    strncpy(s_url, url, sizeof(s_url) - 1);
    xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);
    return ESP_OK;
}

ota_state_t ot_br_ota_get_state(void) { return s_state; }
int ot_br_ota_get_progress_percent(void) { return s_progress; }
const char *ot_br_ota_get_error(void) { return s_error; }
int ot_br_ota_get_last_http_status(void) { return s_last_http_status; }

void ot_br_ota_mark_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
            ESP_LOGI(TAG, "Firmware marked as functional (Rollback disabled)");
        }
    }
}