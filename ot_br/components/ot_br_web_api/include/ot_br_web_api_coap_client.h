#pragma once

#include "esp_err.h"
#include "openthread/coap.h"

#ifdef __cplusplus
extern "C" {
#endif

void ot_br_coap_client_init(void);

esp_err_t ot_br_coap_light_set(const char *address_str, bool on, uint8_t r, uint8_t g, uint8_t b);

esp_err_t ot_br_resolve_omr_address(const char *rloc_address_str, char *out, size_t out_len);

// CoAP-Test-Client (for CoAP Testing)
typedef struct {
    otCoapCode code;
    char payload[256];
    bool has_payload;
} ot_br_coap_response_t;
esp_err_t ot_br_resolve_omr_address(const char *rloc_address_str, char *out, size_t out_len);

esp_err_t ot_br_coap_generic_request(const char *address_str, otCoapCode method,
                                      const char *uri_path, const char *payload,
                                      ot_br_coap_response_t *out_response);

#ifdef __cplusplus
}
#endif