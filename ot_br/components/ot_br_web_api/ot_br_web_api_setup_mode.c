#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "openthread/commissioner.h"

static const char *TAG = "ot_br_setup_mode";
#define BUTTON_GPIO GPIO_NUM_61        // BOOT-button
#define LONG_PRESS_MS 3000
#define SETUP_WINDOW_MS (10 * 60 * 1000)  // 10 minutes timeframe

#define JOINER_PSKD "J01NME" // have to be the same as in auto_joiner.c and CommissionerPanel.tsx

static volatile bool s_setup_mode_active = false;
static int64_t s_setup_mode_started_at = 0;

static void commissioner_state_callback(otCommissionerState aState, void *aContext)
{
    switch (aState)
    {
        case OT_COMMISSIONER_STATE_ACTIVE:
            ESP_LOGI(TAG, "Commissioner ACTIVE");
            break;

        case OT_COMMISSIONER_STATE_PETITION:
            ESP_LOGI(TAG, "Commissioner PETITION");
            break;

        case OT_COMMISSIONER_STATE_DISABLED:
            ESP_LOGI(TAG, "Commissioner DISABLED");
            break;

        default:
            ESP_LOGI(TAG, "Commissioner state: %d", aState);
            break;
    }
}

static void auto_open_commissioning(void)
{
    otInstance *instance = esp_openthread_get_instance();
    esp_openthread_lock_acquire(portMAX_DELAY);

    otError err;

    if (otCommissionerGetState(instance) != OT_COMMISSIONER_STATE_DISABLED)
    {
        ESP_LOGW(TAG, "Commissioner already active - stopping first");

        err = otCommissionerStop(instance);

        ESP_LOGI(TAG, "otCommissionerStop: %s (%d)",
                 otThreadErrorToString(err), err);
    }

    err = otCommissionerStart(instance, commissioner_state_callback, NULL, NULL);
    ESP_LOGI(TAG, "otCommissionerStart: %s (%d)",
         otThreadErrorToString(err), err);

    if (err == OT_ERROR_NONE) {
        err = otCommissionerAddJoiner(instance, NULL, JOINER_PSKD, 600);
        ESP_LOGI(TAG, "otCommissionerAddJoiner: %s (%d)",
         otThreadErrorToString(err), err);

        if (err == OT_ERROR_NONE)
        {
            ESP_LOGI(TAG,
                     "Open commissioning enabled for 600 seconds");
        }
    }
    esp_openthread_lock_release();
}

static void button_task(void *arg)
{
    gpio_config_t cfg = {
        .pin_bit_mask = BIT64(BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);

    int64_t press_start = 0;
    bool was_pressed = false;

    while (1) {
        bool pressed = (gpio_get_level(BUTTON_GPIO) == 0);  // Active-Low

        if (pressed && !was_pressed) {
            press_start = esp_timer_get_time() / 1000;
        } else if (pressed && was_pressed) {
            int64_t held_ms = (esp_timer_get_time() / 1000) - press_start;
            if (held_ms >= LONG_PRESS_MS && !s_setup_mode_active) {
                s_setup_mode_active = true;
                s_setup_mode_started_at = esp_timer_get_time() / 1000;
                ESP_LOGW(TAG, "=== SETUP-MODUS ACTIVATED (10 minutes timeframe) ===");

                // allow auto commissioning after a reboot with the JOINER_PSKD
                auto_open_commissioning();
            }
        }
        was_pressed = pressed;

        // let the timeframe expire automatically
        if (s_setup_mode_active) {
            int64_t elapsed = (esp_timer_get_time() / 1000) - s_setup_mode_started_at;
            if (elapsed > SETUP_WINDOW_MS) {
                s_setup_mode_active = false;
                ESP_LOGI(TAG, "Setup-Modus is expired");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void ot_br_setup_mode_init(void)
{
    xTaskCreate(button_task, "setup_button", 2048, NULL, 5, NULL);
}

bool ot_br_setup_mode_is_active(void)
{
    return s_setup_mode_active;
}

void ot_br_setup_mode_deactivate(void)
{
    s_setup_mode_active = false;
    ESP_LOGI(TAG, "Setup-Modus manuell beendet");
}