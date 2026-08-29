/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * OpenThread Command Line Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
*/

#include "ot_examples_br.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_border_router.h"
#include "protocol_examples_common.h"

#include "ot_br_web_api.h"
#include "ot_br_web_api_ota.h"

#define TAG "ot_examples_br"

#if CONFIG_OPENTHREAD_CLI_WIFI
#error "CONFIG_OPENTHREAD_CLI_WIFI conflicts with the border router auto-initialization feature"
#endif

static bool s_border_router_started = false;

static void ot_br_init(void *ctx)
{
    esp_err_t err = example_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "example_connect failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    esp_openthread_lock_acquire(portMAX_DELAY);
    esp_openthread_set_backbone_netif(get_example_netif());
    err = esp_openthread_border_router_init();
    if (err == ESP_OK) {
        otInstance *instance = esp_openthread_get_instance();
        if (otDatasetIsCommissioned(instance)) {
            otError err1 = otIp6SetEnabled(instance, true);
            otError err2 = otThreadSetEnabled(instance, true);
            if (err1 == OT_ERROR_NONE && err2 == OT_ERROR_NONE) {
                ESP_LOGI(TAG, "Auto-attach: Found registered dataset; Started Thread");
            } else {
                ESP_LOGW(TAG, "Failed to Auto-attach (IPv6: %d, thread: %d)", err1, err2);
            }
        } else {
            ESP_LOGI(TAG, "No dataset found - wait until manual Commissioning");
        }
    }
    esp_openthread_lock_release();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_openthread_border_router_init() failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    s_border_router_started = true;
    ESP_LOGI(TAG, "Border router init + Auto-Attach connected");
    ESP_ERROR_CHECK(ot_br_web_api_start(esp_openthread_get_instance()));

    ot_br_ota_mark_valid();

    vTaskDelete(NULL);
}

esp_err_t esp_openthread_border_router_start(void)
{
    return (xTaskCreate(ot_br_init, "ot_br_init", 6144, NULL, 4, NULL) == pdPASS) ? ESP_OK : ESP_FAIL;
}

void esp_openthread_border_router_stop(void)
{
    if (s_border_router_started) {
        esp_openthread_lock_acquire(portMAX_DELAY);
        ESP_ERROR_CHECK(esp_openthread_border_router_deinit());
        esp_openthread_lock_release();
        s_border_router_started =false;
    }
}
