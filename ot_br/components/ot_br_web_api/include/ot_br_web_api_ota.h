#pragma once

#include "esp_err.h"

typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_IN_PROGRESS,
    OTA_STATE_SUCCESS,
    OTA_STATE_FAILED,
} ota_state_t;

esp_err_t ot_br_ota_start(const char *url);
ota_state_t ot_br_ota_get_state();
int ot_br_ota_get_progress_percent();
const char *ot_br_ota_get_error();

// Call after successful boot to disable rollback protection
void ot_br_ota_mark_valid();