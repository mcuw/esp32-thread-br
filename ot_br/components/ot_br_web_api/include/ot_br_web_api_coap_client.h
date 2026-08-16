#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

void ot_br_coap_client_init(void);

esp_err_t ot_br_coap_light_set(const char *address_str, bool on, uint8_t r, uint8_t g, uint8_t b);

// static void diagnostic_callback(otError err, otMessage *message,
//                                  const otMessageInfo *info, void *ctx);

esp_err_t ot_br_resolve_omr_address(const char *rloc_address_str, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif