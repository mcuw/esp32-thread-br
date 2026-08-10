#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "ot_br_setup_mode";
#define BUTTON_GPIO GPIO_NUM_61        // BOOT-Taste
#define LONG_PRESS_MS 3000
#define SETUP_WINDOW_MS (10 * 60 * 1000)  // 10 Minuten Zeitfenster

static volatile bool s_setup_mode_active = false;
static int64_t s_setup_mode_started_at = 0;

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
        // uncomment for debug button state
        //
        // static int64_t last_log = 0;
        // int64_t now = esp_timer_get_time() / 1000;
        // if (now - last_log > 1000) {
        //     ESP_LOGI(TAG, "GPIO0 Pegel: %d", gpio_get_level(BUTTON_GPIO));
        //     last_log = now;
        // }

        bool pressed = (gpio_get_level(BUTTON_GPIO) == 0);  // Active-Low

        if (pressed && !was_pressed) {
            press_start = esp_timer_get_time() / 1000;
        } else if (pressed && was_pressed) {
            int64_t held_ms = (esp_timer_get_time() / 1000) - press_start;
            if (held_ms >= LONG_PRESS_MS && !s_setup_mode_active) {
                s_setup_mode_active = true;
                s_setup_mode_started_at = esp_timer_get_time() / 1000;
                ESP_LOGW(TAG, "=== SETUP-MODUS AKTIVIERT (10 Minuten Zeitfenster) ===");
            }
        }
        was_pressed = pressed;

        // Zeitfenster automatisch ablaufen lassen
        if (s_setup_mode_active) {
            int64_t elapsed = (esp_timer_get_time() / 1000) - s_setup_mode_started_at;
            if (elapsed > SETUP_WINDOW_MS) {
                s_setup_mode_active = false;
                ESP_LOGI(TAG, "Setup-Modus abgelaufen");
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