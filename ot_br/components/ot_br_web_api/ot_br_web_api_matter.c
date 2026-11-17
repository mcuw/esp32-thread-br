/*
 * Matter-ready Thread Border Router (Stufe 1, siehe doc/matter.md)
 *
 * - aktiviert SRP-Server (Auto-Enable) und NAT64-Prefix-Manager
 * - GET /api/thread/srp             SRP-Server-Status + registrierte Hosts/Services
 * - GET /api/thread/border-routing  Border-Routing-, OMR-, On-Link- und NAT64-Status
 * - GET /api/thread/dataset/tlvs    aktives Dataset als TLV-Hex (Auth), z. B. fuer
 *                                   chip-tool pairing ble-thread <id> hex:<tlvs> ...
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "openthread/border_routing.h"
#include "openthread/dataset.h"
#include "openthread/ip6.h"
#include "openthread/nat64.h"
#include "openthread/srp_server.h"

#include "ot_br_web_api_matter.h"
#include "ot_br_web_api_internal.h"

static const char *TAG = "ot_br_matter";

extern bool ot_br_web_api_check_auth(httpd_req_t *req);

#define MATTER_OPERATIONAL_SERVICE   "_matter._tcp"
#define MATTER_COMMISSIONABLE_SERVICE "_matterc._udp"
#define MATTER_COMMISSIONER_SERVICE  "_matterd._udp"

// -------- Init --------

void ot_br_matter_ready_init(otInstance *instance)
{
    // Must be called with the OpenThread lock held.
#if CONFIG_OT_BR_MATTER_SRP_AUTO_ENABLE
    // Server startet automatisch, sobald Border Routing laeuft, und stoppt, wenn es stoppt.
    otSrpServerSetAutoEnableMode(instance, true);
    ESP_LOGI(TAG, "SRP server auto-enable mode on (state: %d)", otSrpServerGetState(instance));
#endif
#if CONFIG_OT_BR_MATTER_NAT64
    // Prefix-Manager: publiziert einen NAT64-Prefix ins Thread-Netz, sobald einer verfuegbar ist
    // (z. B. vom Heimnetz via RA/DNS64 entdeckt).
    otNat64SetEnabled(instance, true);
    ESP_LOGI(TAG, "NAT64 enabled (prefix manager state: %s)",
             otNat64StateToString(otNat64GetPrefixManagerState(instance)));
#endif
#if !CONFIG_OT_BR_MATTER_SRP_AUTO_ENABLE && !CONFIG_OT_BR_MATTER_NAT64
    (void)instance;
#endif
}

// -------- Helpers --------

static const char *srp_state_to_string(otSrpServerState state)
{
    switch (state) {
    case OT_SRP_SERVER_STATE_DISABLED: return "disabled";
    case OT_SRP_SERVER_STATE_RUNNING:  return "running";
    case OT_SRP_SERVER_STATE_STOPPED:  return "stopped";
    default:                           return "unknown";
    }
}

static const char *srp_address_mode_to_string(otSrpServerAddressMode mode)
{
    switch (mode) {
    case OT_SRP_SERVER_ADDRESS_MODE_UNICAST:           return "unicast";
    case OT_SRP_SERVER_ADDRESS_MODE_ANYCAST:           return "anycast";
    case OT_SRP_SERVER_ADDRESS_MODE_UNICAST_FORCE_ADD: return "unicast-force-add";
    default:                                           return "unknown";
    }
}

static const char *br_state_to_string(otBorderRoutingState state)
{
    switch (state) {
    case OT_BORDER_ROUTING_STATE_UNINITIALIZED: return "uninitialized";
    case OT_BORDER_ROUTING_STATE_DISABLED:      return "disabled";
    case OT_BORDER_ROUTING_STATE_STOPPED:       return "stopped";
    case OT_BORDER_ROUTING_STATE_RUNNING:       return "running";
    default:                                    return "unknown";
    }
}

// true, wenn full_name (z. B. "_matter._tcp.default.service.arpa.") mit service beginnt
static bool service_name_is(const char *full_name, const char *service)
{
    size_t len = strlen(service);
    return full_name && strncmp(full_name, service, len) == 0 && full_name[len] == '.';
}

// Instanz-Label eines operativen Matter-Service: "<CompressedFabricId>-<NodeId>", je 16 Hex-Zeichen.
static void add_matter_operational_ids(cJSON *svc, const char *label)
{
    if (!label || strlen(label) != 33 || label[16] != '-') {
        return;
    }
    char fabric[17];
    char node[17];
    memcpy(fabric, label, 16);
    fabric[16] = '\0';
    memcpy(node, label + 17, 16);
    node[16] = '\0';
    cJSON_AddStringToObject(svc, "compressed_fabric_id", fabric);
    cJSON_AddStringToObject(svc, "node_id", node);
}

static void add_prefix(cJSON *obj, const char *key, otError err, const otIp6Prefix *prefix)
{
    if (err != OT_ERROR_NONE) {
        cJSON_AddNullToObject(obj, key);
        return;
    }
    char buf[OT_IP6_PREFIX_STRING_SIZE];
    otIp6PrefixToString(prefix, buf, sizeof(buf));
    cJSON_AddStringToObject(obj, key, buf);
}

// -------- GET /api/thread/srp --------

static esp_err_t srp_handler(httpd_req_t *req)
{
    otInstance *instance = esp_openthread_get_instance();
    cJSON *j = cJSON_CreateObject();
    cJSON *hosts = cJSON_CreateArray();
    int matter_nodes = 0;
    int matter_commissionable = 0;

    esp_openthread_lock_acquire(portMAX_DELAY);

    cJSON_AddStringToObject(j, "state", srp_state_to_string(otSrpServerGetState(instance)));
    cJSON_AddBoolToObject(j, "auto_enable", otSrpServerIsAutoEnableMode(instance));
    cJSON_AddStringToObject(j, "address_mode",
                            srp_address_mode_to_string(otSrpServerGetAddressMode(instance)));
    cJSON_AddNumberToObject(j, "port", otSrpServerGetPort(instance));
    const char *domain = otSrpServerGetDomain(instance);
    cJSON_AddStringToObject(j, "domain", domain ? domain : "");

    const otSrpServerHost *host = NULL;
    while ((host = otSrpServerGetNextHost(instance, host)) != NULL) {
        if (otSrpServerHostIsDeleted(host)) {
            continue;
        }
        cJSON *h = cJSON_CreateObject();
        cJSON_AddStringToObject(h, "name", otSrpServerHostGetFullName(host));

        otSrpServerLeaseInfo lease;
        otSrpServerHostGetLeaseInfo(host, &lease);
        cJSON_AddNumberToObject(h, "lease_remaining_s", lease.mRemainingLease / 1000);

        cJSON *addrs = cJSON_CreateArray();
        uint8_t num = 0;
        const otIp6Address *a = otSrpServerHostGetAddresses(host, &num);
        for (uint8_t i = 0; i < num; i++) {
            char buf[OT_IP6_ADDRESS_STRING_SIZE];
            otIp6AddressToString(&a[i], buf, sizeof(buf));
            cJSON_AddItemToArray(addrs, cJSON_CreateString(buf));
        }
        cJSON_AddItemToObject(h, "addresses", addrs);

        cJSON *services = cJSON_CreateArray();
        bool is_matter = false;
        const otSrpServerService *svc = NULL;
        while ((svc = otSrpServerHostGetNextService(host, svc)) != NULL) {
            if (otSrpServerServiceIsDeleted(svc)) {
                continue;
            }
            const char *service_name = otSrpServerServiceGetServiceName(svc);
            const char *label = otSrpServerServiceGetInstanceLabel(svc);

            cJSON *s = cJSON_CreateObject();
            cJSON_AddStringToObject(s, "instance", label ? label : "");
            cJSON_AddStringToObject(s, "service", service_name ? service_name : "");
            cJSON_AddNumberToObject(s, "port", otSrpServerServiceGetPort(svc));

            const char *matter_type = NULL;
            if (service_name_is(service_name, MATTER_OPERATIONAL_SERVICE)) {
                matter_type = "operational";
                add_matter_operational_ids(s, label);
                matter_nodes++;
            } else if (service_name_is(service_name, MATTER_COMMISSIONABLE_SERVICE)) {
                matter_type = "commissionable";
                matter_commissionable++;
            } else if (service_name_is(service_name, MATTER_COMMISSIONER_SERVICE)) {
                matter_type = "commissioner";
            }
            if (matter_type) {
                cJSON_AddStringToObject(s, "matter", matter_type);
                is_matter = true;
            }
            cJSON_AddItemToArray(services, s);
        }
        cJSON_AddItemToObject(h, "services", services);
        cJSON_AddBoolToObject(h, "matter", is_matter);
        cJSON_AddItemToArray(hosts, h);
    }

    esp_openthread_lock_release();

    cJSON_AddNumberToObject(j, "matter_operational", matter_nodes);
    cJSON_AddNumberToObject(j, "matter_commissionable", matter_commissionable);
    cJSON_AddItemToObject(j, "hosts", hosts);
    send_json(req, j);
    return ESP_OK;
}

// -------- GET /api/thread/border-routing --------

static esp_err_t border_routing_handler(httpd_req_t *req)
{
    otInstance *instance = esp_openthread_get_instance();
    cJSON *j = cJSON_CreateObject();
    otIp6Prefix prefix;
    otError err;

    esp_openthread_lock_acquire(portMAX_DELAY);

    cJSON_AddStringToObject(j, "state", br_state_to_string(otBorderRoutingGetState(instance)));

    err = otBorderRoutingGetOmrPrefix(instance, &prefix);
    add_prefix(j, "omr_prefix", err, &prefix);

    otRoutePreference pref;
    err = otBorderRoutingGetFavoredOmrPrefix(instance, &prefix, &pref);
    add_prefix(j, "favored_omr_prefix", err, &prefix);

    err = otBorderRoutingGetOnLinkPrefix(instance, &prefix);
    add_prefix(j, "on_link_prefix", err, &prefix);

    err = otBorderRoutingGetFavoredOnLinkPrefix(instance, &prefix);
    add_prefix(j, "favored_on_link_prefix", err, &prefix);

    cJSON *nat64 = cJSON_CreateObject();
    cJSON_AddStringToObject(nat64, "prefix_manager_state",
                            otNat64StateToString(otNat64GetPrefixManagerState(instance)));
    err = otBorderRoutingGetNat64Prefix(instance, &prefix);
    add_prefix(nat64, "local_prefix", err, &prefix);
    err = otBorderRoutingGetFavoredNat64Prefix(instance, &prefix, &pref);
    add_prefix(nat64, "favored_prefix", err, &prefix);
    cJSON_AddItemToObject(j, "nat64", nat64);

    esp_openthread_lock_release();

    send_json(req, j);
    return ESP_OK;
}

// -------- GET /api/thread/dataset/tlvs (Auth is required) --------

static esp_err_t dataset_tlvs_handler(httpd_req_t *req)
{
    if (!ot_br_web_api_check_auth(req)) return ESP_OK;

    otInstance *instance = esp_openthread_get_instance();
    otOperationalDatasetTlvs tlvs;

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError err = otDatasetGetActiveTlvs(instance, &tlvs);
    esp_openthread_lock_release();

    if (err != OT_ERROR_NONE) {
        send_json_error(req, "404 Not Found", "no active dataset");
        return ESP_OK;
    }

    static const char hex[] = "0123456789abcdef";
    char out[OT_OPERATIONAL_DATASET_MAX_LENGTH * 2 + 1];
    for (uint8_t i = 0; i < tlvs.mLength; i++) {
        out[i * 2]     = hex[tlvs.mTlvs[i] >> 4];
        out[i * 2 + 1] = hex[tlvs.mTlvs[i] & 0x0F];
    }
    out[tlvs.mLength * 2] = '\0';

    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "tlvs", out);
    send_json(req, j);
    return ESP_OK;
}

// -------- Registrierung --------

void ot_br_web_api_register_matter_handlers(httpd_handle_t server)
{
    const httpd_uri_t uris[] = {
        { "/api/thread/srp",            HTTP_GET, srp_handler,            NULL },
        { "/api/thread/border-routing", HTTP_GET, border_routing_handler, NULL },
        { "/api/thread/dataset/tlvs",   HTTP_GET, dataset_tlvs_handler,   NULL },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }
}
