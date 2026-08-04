#include <string.h>
#include <stdio.h>
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ot_br_web_api_ota.h"

static const char *TAG = "ot_br_ota";

static volatile ota_state_t s_state = OTA_STATE_IDLE;
static volatile int s_progress = 0;
static char s_error[128] = {0};
static char s_url[256] = {0};

static void ota_task(void *pvParameter)
{
    s_state = OTA_STATE_IN_PROGRESS;
    s_progress = 0;
    s_error[0] = '\0';

    esp_http_client_config_t http_config = {
        .url = s_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK) {
        snprintf(s_error, sizeof(s_error), "begin failed: %s", esp_err_to_name(err));
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
        snprintf(s_error, sizeof(s_error), "download incomplete: %s", esp_err_to_name(err));
        esp_https_ota_abort(handle);
        s_state = OTA_STATE_FAILED;
        vTaskDelete(NULL);
        return;
    }

    err = esp_https_ota_finish(handle);
    if (err != ESP_OK) {
        snprintf(s_error, sizeof(s_error), "finish failed: %s", esp_err_to_name(err));
        s_state = OTA_STATE_FAILED;
        vTaskDelete(NULL);
        return;
    }

    s_progress = 100;
    s_state = OTA_STATE_SUCCESS;
    ESP_LOGI(TAG, "OTA erfolgreich, Neustart in 3 Sekunden");
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

void ot_br_ota_mark_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
            ESP_LOGI(TAG, "Firmware als funktionsfaehig markiert (Rollback deaktiviert)");
        }
    }
}