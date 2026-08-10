#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_littlefs.h"
#include "esp_log.h"

static const char *TAG = "ot_br_web_static";
#define WEB_BASE_PATH "/web"
#define MAX_URI_LEN 512
#define MAX_FILE_PATH_LEN (MAX_URI_LEN + sizeof(WEB_BASE_PATH))

esp_err_t ot_br_web_api_mount_littlefs(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = WEB_BASE_PATH,
        .partition_label = "littlefs",
        .format_if_mount_failed = false,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS mount fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }

    size_t total = 0, used = 0;
    esp_littlefs_info("littlefs", &total, &used);
    ESP_LOGI(TAG, "LittleFS gemountet: %d/%d KB belegt", used / 1024, total / 1024);
    return ESP_OK;
}

static const char *get_content_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";

    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0)  return "text/css";
    if (strcmp(ext, ".js") == 0 || strcmp(ext, ".mjs") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".svg") == 0)  return "image/svg+xml";
    if (strcmp(ext, ".png") == 0)  return "image/png";
    if (strcmp(ext, ".ico") == 0)  return "image/x-icon";
    if (strcmp(ext, ".woff2") == 0) return "font/woff2";
    if (strcmp(ext, ".webmanifest") == 0) return "application/manifest+json";
    return "text/plain";
}

// Astro/Vite hasht Asset-Dateinamen (z.B. /_astro/index.a1b2c3.js) -> unveraenderlich, lange cachebar.
// index.html & andere Top-Level-Dateien dagegen nicht hashen -> nie cachen, damit Updates sofort greifen.
static bool is_immutable_asset(const char *uri)
{
    return strncmp(uri, "/_astro/", 8) == 0;
}

static esp_err_t static_file_handler(httpd_req_t *req)
{
    char filepath[MAX_FILE_PATH_LEN];

    // Root und Verzeichnispfade -> index.html
    const char *uri = req->uri;
    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }

    // Query-String abschneiden (z.B. ?v=123), falls vorhanden
    char clean_uri[MAX_URI_LEN];
    strncpy(clean_uri, uri, sizeof(clean_uri) - 1);
    clean_uri[sizeof(clean_uri) - 1] = '\0';
    char *qmark = strchr(clean_uri, '?');
    if (qmark) *qmark = '\0';

    int written = snprintf(filepath, sizeof(filepath), "%s%s", WEB_BASE_PATH, clean_uri);
    if (written < 0 || (size_t)written >= sizeof(filepath)) {
        httpd_resp_set_status(req, "414 URI Too Long");
        httpd_resp_sendstr(req, "URI too long");
        return ESP_OK;
    }

    FILE *f = fopen(filepath, "r");
    if (!f) {
        // Kein SPA-Fallback noetig (Astro liefert vorgerenderte Multi-Page-Struktur),
        // daher hier bewusst ein klarer 404 statt stiller Umleitung auf index.html.
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "404 Not Found");
        return ESP_OK;
    }

    httpd_resp_set_type(req, get_content_type(clean_uri));
    if (is_immutable_asset(clean_uri)) {
        httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");
    } else {
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    }

    char chunk[1024];
    size_t read_bytes;
    esp_err_t res = ESP_OK;
    do {
        read_bytes = fread(chunk, 1, sizeof(chunk), f);
        if (read_bytes > 0) {
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                res = ESP_FAIL;
                break;
            }
        }
    } while (read_bytes > 0);

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);  // Chunked-Transfer abschliessen
    return res;
}

void ot_br_web_api_register_static_handler(httpd_handle_t server)
{
    httpd_uri_t catch_all = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &catch_all);
    ESP_LOGI(TAG, "Static-File-Handler registriert (Catch-All)");
}