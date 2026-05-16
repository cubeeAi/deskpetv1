#include "services/app_services.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/time.h>
#include <vector>

#include "display/display_assets.h"
#include "display/oled_display.h"
#include "display/text_render.h"

extern "C" {
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"
}

struct FactoryMqttConfig {
    std::string host = kMqttBrokerHost;
    int port = kMqttBrokerPort;
    std::string device_id = kMqttDeviceId;
    std::string secret = kMqttSecret;
    bool loaded_from_factory_nvs = false;
};

struct TimezoneAlias {
    const char *iana;
    const char *posix;
};

struct TelemetrySnapshot {
    int rssi = 0;
    int brightness = 0;
    bool wifi_connected = false;
    bool config_portal_active = false;
    std::string current_page;
    std::string current_mood;
    std::string city;
    std::string weather_theme;
    std::string latitude;
    std::string longitude;
    std::string timezone;
    std::string weather_main;
    std::string weather_desc;
    float temperature = 0.0F;
    float feels_like = 0.0F;
    int humidity = 0;
    bool weather_ready = false;
    std::string weather_forecast_json;
};

FactoryMqttConfig g_factory_mqtt = {};
TelemetrySnapshot g_last_telemetry_snapshot = {};
bool g_has_last_telemetry_snapshot = false;

constexpr TimezoneAlias kAliases[] = {
    {"Asia/Shanghai", "CST-8"},
    {"Asia/Chongqing", "CST-8"},
    {"Asia/Harbin", "CST-8"},
    {"Asia/Urumqi", "CST-6"},
    {"PRC", "CST-8"},
    {"Asia/Tokyo", "JST-9"},
    {"Asia/Seoul", "KST-9"},
    {"Asia/Singapore", "SGT-8"},
    {"Asia/Hong_Kong", "HKT-8"},
    {"Asia/Taipei", "CST-8"},
    {"Asia/Kolkata", "IST-5:30"},
    {"Asia/Calcutta", "IST-5:30"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0/2"},
    {"America/New_York", "EST5EDT,M3.2.0/2,M11.1.0/2"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0/2,M11.1.0/2"},
    {"UTC", "UTC0"},
    {"Etc/UTC", "UTC0"},
};

bool mqtt_config_ready(const DeviceConfig &config)
{
    (void)config;
    return !g_factory_mqtt.host.empty() &&
           !g_factory_mqtt.device_id.empty() &&
           !g_factory_mqtt.secret.empty() &&
           g_factory_mqtt.secret != "REPLACE_WITH_DEVICE_SECRET";
}

std::string read_nvs_string(nvs_handle_t handle, const char *key)
{
    size_t required = 0;
    esp_err_t err = nvs_get_str(handle, key, nullptr, &required);
    if (err != ESP_OK || required == 0) {
        return {};
    }

    std::string value(required, '\0');
    err = nvs_get_str(handle, key, value.data(), &required);
    if (err != ESP_OK) {
        return {};
    }

    if (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return value;
}

uint32_t read_nvs_u32(nvs_handle_t handle, const char *key, uint32_t fallback)
{
    uint32_t value = fallback;
    const esp_err_t err = nvs_get_u32(handle, key, &value);
    if (err != ESP_OK) {
        return fallback;
    }
    return value;
}

void load_factory_mqtt_config()
{
    g_factory_mqtt = {};

    nvs_handle_t handle = 0;
    const esp_err_t err = nvs_open_from_partition(kFactoryNvsPartition, kFactoryConfigNamespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(
            kTag,
            "factory mqtt config not found in partition '%s' namespace '%s': %s; using firmware defaults",
            kFactoryNvsPartition,
            kFactoryConfigNamespace,
            esp_err_to_name(err));
        return;
    }

    const std::string host = read_nvs_string(handle, "mqtt_host");
    const std::string device_id = read_nvs_string(handle, "mqtt_device_id");
    const std::string secret = read_nvs_string(handle, "mqtt_secret");
    const uint32_t port = read_nvs_u32(handle, "mqtt_port", static_cast<uint32_t>(kMqttBrokerPort));
    nvs_close(handle);

    if (!host.empty()) {
        g_factory_mqtt.host = host;
    }
    if (!device_id.empty()) {
        g_factory_mqtt.device_id = device_id;
    }
    if (!secret.empty()) {
        g_factory_mqtt.secret = secret;
    }
    g_factory_mqtt.port = static_cast<int>(port);
    g_factory_mqtt.loaded_from_factory_nvs = true;

    ESP_LOGI(
        kTag,
        "factory mqtt config loaded: host='%s' port=%d device_id='%s' secret=%s source=%s",
        g_factory_mqtt.host.c_str(),
        g_factory_mqtt.port,
        g_factory_mqtt.device_id.c_str(),
        g_factory_mqtt.secret.empty() ? "missing" : "present",
        g_factory_mqtt.loaded_from_factory_nvs ? "factory_nvs" : "firmware_default");
}

void load_config()
{
    nvs_handle_t handle = 0;
    const esp_err_t err = nvs_open(kConfigNamespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "no saved config in nvs namespace '%s': %s", kConfigNamespace, esp_err_to_name(err));
        g_config = {};
        load_factory_mqtt_config();
        return;
    }

    g_config.ssid = read_nvs_string(handle, "ssid");
    g_config.pass = read_nvs_string(handle, "pass");
    g_config.city = read_nvs_string(handle, "city");
    g_config.latitude = read_nvs_string(handle, "lat");
    g_config.longitude = read_nvs_string(handle, "lon");
    g_config.tz = read_nvs_string(handle, "tz");
    g_config.weather_theme = read_nvs_string(handle, "weather_theme");
    nvs_close(handle);

    int weather_theme = WEATHER_THEME_CLASSIC;
    if (!parse_weather_theme(g_config.weather_theme, &weather_theme)) {
        g_config.weather_theme = weather_theme_to_string(WEATHER_THEME_CLASSIC);
    }

    load_factory_mqtt_config();

    ESP_LOGI(
        kTag,
        "config loaded: ssid='%s', city='%s', lat='%s', lon='%s', tz='%s', theme='%s', mqtt_host='%s', mqtt_port=%d, device_id='%s', secret=%s source=%s",
        g_config.ssid.c_str(),
        g_config.city.c_str(),
        g_config.latitude.c_str(),
        g_config.longitude.c_str(),
        g_config.tz.c_str(),
        g_config.weather_theme.c_str(),
        g_factory_mqtt.host.c_str(),
        g_factory_mqtt.port,
        g_factory_mqtt.device_id.c_str(),
        g_factory_mqtt.secret == "REPLACE_WITH_DEVICE_SECRET" ? "placeholder" : "present",
        g_factory_mqtt.loaded_from_factory_nvs ? "factory_nvs" : "firmware_default");
}

esp_err_t save_config(const DeviceConfig &config)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kConfigNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, "ssid", config.ssid.c_str());
    if (err == ESP_OK) err = nvs_set_str(handle, "pass", config.pass.c_str());
    if (err == ESP_OK) err = nvs_set_str(handle, "city", config.city.c_str());
    if (err == ESP_OK) err = nvs_set_str(handle, "lat", config.latitude.c_str());
    if (err == ESP_OK) err = nvs_set_str(handle, "lon", config.longitude.c_str());
    if (err == ESP_OK) err = nvs_set_str(handle, "tz", config.tz.c_str());
    if (err == ESP_OK) err = nvs_set_str(handle, "weather_theme", config.weather_theme.c_str());
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err == ESP_OK) {
        ESP_LOGI(kTag, "config saved successfully for ssid='%s'", config.ssid.c_str());
    } else {
        ESP_LOGE(kTag, "config save failed: %s", esp_err_to_name(err));
    }
    return err;
}

void reset_forecast()
{
    for (auto &entry : g_weather.forecast) {
        entry = {};
    }
}

bool weather_location_configured()
{
    return !g_config.latitude.empty() && !g_config.longitude.empty() && !g_config.tz.empty();
}

void clear_pending_weather_request()
{
    g_mqtt.weather_req_pending = false;
    g_mqtt.weather_req_id.clear();
    g_mqtt.weather_req_sent_at = 0;
}

void schedule_restart()
{
    ESP_LOGI(kTag, "restart scheduled in %lu ms", kRestartDelayMs);
    g_restart_requested.store(true);
    g_restart_requested_at.store(millis());
}

void maybe_restart()
{
    if (!g_restart_requested.load()) {
        return;
    }
    if (millis() - g_restart_requested_at.load() < kRestartDelayMs) {
        return;
    }
    esp_restart();
}

void init_nvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(kTag, "nvs requires erase, reinitializing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    esp_err_t factory_err = nvs_flash_init_partition(kFactoryNvsPartition);
    if (factory_err == ESP_ERR_NVS_NO_FREE_PAGES || factory_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(kTag, "factory nvs requires erase, reinitializing");
        ESP_ERROR_CHECK(nvs_flash_erase_partition(kFactoryNvsPartition));
        factory_err = nvs_flash_init_partition(kFactoryNvsPartition);
    }
    if (factory_err != ESP_OK) {
        ESP_LOGW(kTag, "factory nvs init skipped: partition='%s' err=%s", kFactoryNvsPartition, esp_err_to_name(factory_err));
    }
    ESP_LOGI(kTag, "nvs initialized");
}

void init_touch_gpio()
{
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << kTouchPin;
    config.mode = GPIO_MODE_INPUT;
    config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_LOGI(kTag, "touch gpio initialized on GPIO%d", static_cast<int>(kTouchPin));
}

void init_wifi_stack()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    g_sta_netif = esp_netif_create_default_wifi_sta();
    g_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    g_wifi_event_group = xEventGroupCreate();

    auto wifi_handler = [](void *, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        if (event_base == WIFI_EVENT) {
            if (event_id == WIFI_EVENT_STA_START) {
                const esp_err_t err = esp_wifi_connect();
                if (err != ESP_OK) {
                    ESP_LOGW(kTag, "wifi connect on start failed: %s", esp_err_to_name(err));
                }
            } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
                const auto *event = static_cast<const wifi_event_sta_disconnected_t *>(event_data);
                if (event != nullptr) {
                    g_mqtt.last_wifi_disconnect_reason = static_cast<int>(event->reason);
                    g_mqtt.wifi_retry_count += 1;
                    ESP_LOGW(
                        kTag,
                        "wifi disconnected: reason=%d, ssid='%.*s'",
                        static_cast<int>(event->reason),
                        static_cast<int>(event->ssid_len),
                        reinterpret_cast<const char *>(event->ssid));
                } else {
                    ESP_LOGW(kTag, "wifi disconnected: no event data");
                }
                g_wifi_connected.store(false);
                if (g_wifi_event_group != nullptr) {
                    xEventGroupClearBits(g_wifi_event_group, kWifiConnectedBit);
                    xEventGroupSetBits(g_wifi_event_group, kWifiDisconnectedBit);
                }
                if (!g_ui.in_config_mode) {
                    const esp_err_t err = esp_wifi_connect();
                    if (err != ESP_OK) {
                        ESP_LOGW(kTag, "wifi reconnect failed: %s", esp_err_to_name(err));
                    }
                }
            }
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            const auto *event = static_cast<const ip_event_got_ip_t *>(event_data);
            if (event != nullptr) {
                ESP_LOGI(kTag, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
            }
            g_mqtt.wifi_retry_count = 0;
            g_wifi_connected.store(true);
            if (g_wifi_event_group != nullptr) {
                xEventGroupSetBits(g_wifi_event_group, kWifiConnectedBit);
                xEventGroupClearBits(g_wifi_event_group, kWifiDisconnectedBit);
            }
        }
    };

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, nullptr, &g_wifi_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, nullptr, &g_ip_got_ip));
    ESP_LOGI(kTag, "wifi stack initialized");
}

void stop_wifi_safely()
{
    const esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_ERROR_CHECK(err);
    }
}

void reset_mqtt_topics()
{
    g_mqtt.topic_cmd.clear();
    g_mqtt.topic_resp.clear();
    g_mqtt.topic_ack.clear();
    g_mqtt.topic_telemetry.clear();
    g_mqtt.topic_event.clear();
    g_mqtt.topic_req.clear();
}

void build_mqtt_topics()
{
    reset_mqtt_topics();
    if (g_factory_mqtt.device_id.empty()) {
        return;
    }

    const std::string base = "pet/" + g_factory_mqtt.device_id;
    g_mqtt.topic_cmd = base + "/cmd";
    g_mqtt.topic_resp = base + "/resp";
    g_mqtt.topic_ack = base + "/cmd/ack";
    g_mqtt.topic_telemetry = base + "/telemetry";
    g_mqtt.topic_event = base + "/event";
    g_mqtt.topic_req = base + "/req";
}

bool mqtt_req_id_seen(const std::string &req_id)
{
    for (const auto &cached : g_mqtt.recent_req_ids) {
        if (!cached.empty() && cached == req_id) {
            return true;
        }
    }
    return false;
}

void remember_mqtt_req_id(const std::string &req_id)
{
    if (req_id.empty()) {
        return;
    }
    g_mqtt.recent_req_ids[g_mqtt.recent_req_index] = req_id;
    g_mqtt.recent_req_index = (g_mqtt.recent_req_index + 1) % g_mqtt.recent_req_ids.size();
}

bool mqtt_publish_json(const std::string &topic, cJSON *root, int qos)
{
    if (!g_mqtt_connected.load() || g_mqtt.client == nullptr || root == nullptr || topic.empty()) {
        ESP_LOGW(
            kTag,
            "mqtt publish skipped: connected=%s client=%s topic='%s'",
            g_mqtt_connected.load() ? "true" : "false",
            g_mqtt.client != nullptr ? "ready" : "null",
            topic.c_str());
        return false;
    }

    char *json = cJSON_PrintUnformatted(root);
    if (json == nullptr) {
        ESP_LOGE(kTag, "mqtt publish failed: cJSON_PrintUnformatted returned null");
        return false;
    }

    ESP_LOGI(kTag, "mqtt publish topic=%s qos=%d payload=%s", topic.c_str(), qos, json);
    const int msg_id = esp_mqtt_client_publish(g_mqtt.client, topic.c_str(), json, 0, qos, 0);
    ESP_LOGI(kTag, "mqtt publish result: topic=%s msg_id=%d", topic.c_str(), msg_id);
    cJSON_free(json);
    return msg_id >= 0;
}

int current_rssi()
{
    wifi_ap_record_t ap_info = {};
    if (g_wifi_connected.load() && esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}

void publish_cmd_ack(const std::string &req_id, bool ok, const char *code, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    if (root == nullptr) {
        return;
    }

    cJSON_AddNumberToObject(root, "schemaVersion", 1);
    cJSON_AddStringToObject(root, "reqId", req_id.c_str());
    cJSON_AddBoolToObject(root, "ok", ok);
    cJSON_AddStringToObject(root, "code", code);
    cJSON_AddStringToObject(root, "message", message);
    cJSON_AddNumberToObject(root, "ts", static_cast<double>(std::time(nullptr)));
    mqtt_publish_json(g_mqtt.topic_ack, root, 1);
    cJSON_Delete(root);
}

uint64_t current_timestamp_ms()
{
    timeval now = {};
    if (gettimeofday(&now, nullptr) == 0) {
        const uint64_t epoch_ms =
            static_cast<uint64_t>(now.tv_sec) * 1000ULL +
            static_cast<uint64_t>(now.tv_usec / 1000ULL);
        if (epoch_ms >= 1700000000000ULL) {
            return epoch_ms;
        }
    }

    const uint64_t fallback_ms = esp_timer_get_time() / 1000ULL;
    ESP_LOGW(kTag, "timestamp fallback to uptime ms=%llu because wall clock is not ready", fallback_ms);
    return fallback_ms;
}

void publish_event_object(const char *event_id, const char *event_type, cJSON *params)
{
    cJSON *root = cJSON_CreateObject();
    if (root == nullptr) {
        if (params != nullptr) {
            cJSON_Delete(params);
        }
        return;
    }

    cJSON_AddStringToObject(root, "eventId", event_id);
    cJSON_AddStringToObject(root, "eventType", event_type);
    cJSON_AddNumberToObject(root, "timestamp", static_cast<double>(current_timestamp_ms()));
    if (params != nullptr) {
        cJSON_AddItemToObject(root, "params", params);
    }
    ESP_LOGI(kTag, "event snapshot: eventId=%s eventType=%s", event_id, event_type);
    mqtt_publish_json(g_mqtt.topic_event, root, 1);
    cJSON_Delete(root);
}

void publish_touch_event(const char *press_type, unsigned long duration_ms, int page_before, int page_after)
{
    ESP_LOGI(
        kTag,
        "touch event: pressType=%s durationMs=%lu pageBefore=%s pageAfter=%s",
        press_type,
        duration_ms,
        page_to_string(page_before),
        page_to_string(page_after));
    cJSON *params = cJSON_CreateObject();
    if (params == nullptr) {
        return;
    }
    cJSON_AddStringToObject(params, "pressType", press_type);
    cJSON_AddNumberToObject(params, "durationMs", static_cast<double>(duration_ms));
    cJSON_AddStringToObject(params, "pageBefore", page_to_string(page_before));
    cJSON_AddStringToObject(params, "pageAfter", page_to_string(page_after));
    publish_event_object("touch", "info", params);
}

void publish_config_portal_entered_event(const char *reason)
{
    cJSON *params = cJSON_CreateObject();
    if (params == nullptr) {
        return;
    }
    cJSON_AddStringToObject(params, "reason", reason);
    publish_event_object("configPortalEntered", "info", params);
}

void publish_wifi_connect_failed_event(const std::string &ssid, int reason, int retry_count)
{
    cJSON *params = cJSON_CreateObject();
    if (params == nullptr) {
        return;
    }
    cJSON_AddStringToObject(params, "ssid", ssid.c_str());
    cJSON_AddNumberToObject(params, "reason", reason);
    cJSON_AddNumberToObject(params, "retryCount", retry_count);
    publish_event_object("wifiConnectFailed", "alert", params);
}

void publish_weather_refresh_failed_event(const char *stage, int status_code)
{
    cJSON *params = cJSON_CreateObject();
    if (params == nullptr) {
        return;
    }
    cJSON_AddStringToObject(params, "stage", stage);
    cJSON_AddNumberToObject(params, "statusCode", status_code);
    publish_event_object("weatherRefreshFailed", "alert", params);
}

cJSON *build_weather_forecast_telemetry_object()
{
    cJSON *root = cJSON_CreateObject();
    cJSON *days = cJSON_CreateArray();
    if (root == nullptr || days == nullptr) {
        if (days != nullptr) {
            cJSON_Delete(days);
        }
        if (root != nullptr) {
            cJSON_Delete(root);
        }
        return nullptr;
    }

    cJSON_AddItemToObject(root, "days", days);
    for (const auto &forecast : g_weather.forecast) {
        if (!forecast.valid) {
            continue;
        }

        cJSON *entry = cJSON_CreateObject();
        if (entry == nullptr) {
            continue;
        }
        cJSON_AddStringToObject(entry, "date", forecast.date.c_str());
        cJSON_AddStringToObject(entry, "dayName", forecast.day_name.c_str());
        cJSON_AddStringToObject(entry, "weatherMain", forecast.weather_main.c_str());
        cJSON_AddStringToObject(entry, "weatherDesc", forecast.weather_desc.c_str());
        cJSON_AddNumberToObject(entry, "tempMin", forecast.temp_min);
        cJSON_AddNumberToObject(entry, "tempMax", forecast.temp_max);
        cJSON_AddNumberToObject(entry, "humidity", forecast.humidity);
        cJSON_AddItemToArray(days, entry);
    }
    return root;
}

std::string build_weather_forecast_telemetry_json()
{
    cJSON *weather_forecast = build_weather_forecast_telemetry_object();
    if (weather_forecast == nullptr) {
        return {};
    }

    char *json = cJSON_PrintUnformatted(weather_forecast);
    std::string result;
    if (json != nullptr) {
        result = json;
        cJSON_free(json);
    }
    cJSON_Delete(weather_forecast);
    return result;
}

TelemetrySnapshot capture_telemetry_snapshot()
{
    TelemetrySnapshot snapshot = {};
    snapshot.rssi = current_rssi();
    snapshot.brightness = g_ui.brightness_percent;
    snapshot.wifi_connected = g_wifi_connected.load();
    snapshot.config_portal_active = g_ui.in_config_mode;
    snapshot.current_page = page_to_string(g_ui.current_page);
    snapshot.current_mood = mood_to_string(g_eyes.current_mood);
    snapshot.city = g_config.city;
    snapshot.weather_theme = g_config.weather_theme;
    snapshot.latitude = g_config.latitude;
    snapshot.longitude = g_config.longitude;
    snapshot.timezone = g_config.tz;
    snapshot.weather_main = g_weather.weather_main;
    snapshot.weather_desc = g_weather.weather_desc;
    snapshot.temperature = g_weather.temperature;
    snapshot.feels_like = g_weather.feels_like;
    snapshot.humidity = g_weather.humidity;
    snapshot.weather_ready = g_weather.has_current;
    snapshot.weather_forecast_json = build_weather_forecast_telemetry_json();
    return snapshot;
}

bool telemetry_float_changed(float before, float after)
{
    return std::fabs(before - after) > 0.01F;
}

void add_optional_number_from_string(cJSON *root, const char *name, const std::string &value, bool include_empty)
{
    if (!value.empty()) {
        cJSON_AddNumberToObject(root, name, std::strtod(value.c_str(), nullptr));
    } else if (include_empty) {
        cJSON_AddNullToObject(root, name);
    }
}

void add_weather_forecast_json(cJSON *root, const std::string &weather_forecast_json)
{
    if (weather_forecast_json.empty()) {
        return;
    }

    cJSON *weather_forecast = cJSON_ParseWithLength(weather_forecast_json.c_str(), weather_forecast_json.size());
    if (weather_forecast != nullptr) {
        cJSON_AddItemToObject(root, "weatherForecast", weather_forecast);
    }
}

void add_telemetry_metadata(cJSON *root)
{
    cJSON_AddNumberToObject(root, "schemaVersion", 1);
    cJSON_AddNumberToObject(root, "ts", static_cast<double>(std::time(nullptr)));
}

int add_full_telemetry_properties(cJSON *root, const TelemetrySnapshot &snapshot)
{
    cJSON_AddNumberToObject(root, "rssi", snapshot.rssi);
    cJSON_AddNumberToObject(root, "brightness", snapshot.brightness);
    cJSON_AddStringToObject(root, "version", kFirmwareVersion);
    cJSON_AddBoolToObject(root, "wifiConnected", snapshot.wifi_connected);
    cJSON_AddBoolToObject(root, "configPortalActive", snapshot.config_portal_active);
    cJSON_AddStringToObject(root, "currentPage", snapshot.current_page.c_str());
    cJSON_AddStringToObject(root, "currentMood", snapshot.current_mood.c_str());
    cJSON_AddStringToObject(root, "city", snapshot.city.c_str());
    cJSON_AddStringToObject(root, "weatherTheme", snapshot.weather_theme.c_str());
    add_optional_number_from_string(root, "latitude", snapshot.latitude, false);
    add_optional_number_from_string(root, "longitude", snapshot.longitude, false);
    cJSON_AddStringToObject(root, "timezone", snapshot.timezone.c_str());
    cJSON_AddStringToObject(root, "weatherMain", snapshot.weather_main.c_str());
    cJSON_AddStringToObject(root, "weatherDesc", snapshot.weather_desc.c_str());
    cJSON_AddNumberToObject(root, "temperature", snapshot.temperature);
    cJSON_AddNumberToObject(root, "feelsLike", snapshot.feels_like);
    cJSON_AddNumberToObject(root, "humidity", snapshot.humidity);
    cJSON_AddBoolToObject(root, "weatherReady", snapshot.weather_ready);
    add_weather_forecast_json(root, snapshot.weather_forecast_json);
    return 20;
}

int add_changed_telemetry_properties(cJSON *root, const TelemetrySnapshot &current, const TelemetrySnapshot &previous)
{
    int changed_count = 0;

    if (current.rssi != previous.rssi) {
        cJSON_AddNumberToObject(root, "rssi", current.rssi);
        ++changed_count;
    }
    if (current.brightness != previous.brightness) {
        cJSON_AddNumberToObject(root, "brightness", current.brightness);
        ++changed_count;
    }
    if (current.wifi_connected != previous.wifi_connected) {
        cJSON_AddBoolToObject(root, "wifiConnected", current.wifi_connected);
        ++changed_count;
    }
    if (current.config_portal_active != previous.config_portal_active) {
        cJSON_AddBoolToObject(root, "configPortalActive", current.config_portal_active);
        ++changed_count;
    }
    if (current.current_page != previous.current_page) {
        cJSON_AddStringToObject(root, "currentPage", current.current_page.c_str());
        ++changed_count;
    }
    if (current.current_mood != previous.current_mood) {
        cJSON_AddStringToObject(root, "currentMood", current.current_mood.c_str());
        ++changed_count;
    }
    if (current.city != previous.city) {
        cJSON_AddStringToObject(root, "city", current.city.c_str());
        ++changed_count;
    }
    if (current.weather_theme != previous.weather_theme) {
        cJSON_AddStringToObject(root, "weatherTheme", current.weather_theme.c_str());
        ++changed_count;
    }
    if (current.latitude != previous.latitude) {
        add_optional_number_from_string(root, "latitude", current.latitude, true);
        ++changed_count;
    }
    if (current.longitude != previous.longitude) {
        add_optional_number_from_string(root, "longitude", current.longitude, true);
        ++changed_count;
    }
    if (current.timezone != previous.timezone) {
        cJSON_AddStringToObject(root, "timezone", current.timezone.c_str());
        ++changed_count;
    }
    if (current.weather_main != previous.weather_main) {
        cJSON_AddStringToObject(root, "weatherMain", current.weather_main.c_str());
        ++changed_count;
    }
    if (current.weather_desc != previous.weather_desc) {
        cJSON_AddStringToObject(root, "weatherDesc", current.weather_desc.c_str());
        ++changed_count;
    }
    if (telemetry_float_changed(previous.temperature, current.temperature)) {
        cJSON_AddNumberToObject(root, "temperature", current.temperature);
        ++changed_count;
    }
    if (telemetry_float_changed(previous.feels_like, current.feels_like)) {
        cJSON_AddNumberToObject(root, "feelsLike", current.feels_like);
        ++changed_count;
    }
    if (current.humidity != previous.humidity) {
        cJSON_AddNumberToObject(root, "humidity", current.humidity);
        ++changed_count;
    }
    if (current.weather_ready != previous.weather_ready) {
        cJSON_AddBoolToObject(root, "weatherReady", current.weather_ready);
        ++changed_count;
    }
    if (current.weather_forecast_json != previous.weather_forecast_json) {
        add_weather_forecast_json(root, current.weather_forecast_json);
        ++changed_count;
    }

    return changed_count;
}

void log_telemetry_snapshot(const TelemetrySnapshot &snapshot, const char *mode, int property_count)
{
    ESP_LOGI(
        kTag,
        "telemetry %s: changedProperties=%d wifi=%s mqtt=%s page=%s mood=%s brightness=%d city='%s' theme='%s' lat='%s' lon='%s' temp=%.1f humidity=%d ready=%s",
        mode,
        property_count,
        snapshot.wifi_connected ? "true" : "false",
        g_mqtt_connected.load() ? "true" : "false",
        snapshot.current_page.c_str(),
        snapshot.current_mood.c_str(),
        snapshot.brightness,
        snapshot.city.c_str(),
        snapshot.weather_theme.c_str(),
        snapshot.latitude.c_str(),
        snapshot.longitude.c_str(),
        static_cast<double>(snapshot.temperature),
        snapshot.humidity,
        snapshot.weather_ready ? "true" : "false");
}

void publish_telemetry_snapshot(bool full)
{
    const TelemetrySnapshot current = capture_telemetry_snapshot();
    cJSON *root = cJSON_CreateObject();
    if (root == nullptr) {
        return;
    }

    add_telemetry_metadata(root);
    const bool force_full = full || !g_has_last_telemetry_snapshot;
    const int property_count = force_full
        ? add_full_telemetry_properties(root, current)
        : add_changed_telemetry_properties(root, current, g_last_telemetry_snapshot);

    if (property_count <= 0) {
        cJSON_Delete(root);
        ESP_LOGI(kTag, "telemetry delta skipped: no property changes");
        return;
    }

    log_telemetry_snapshot(current, force_full ? "full" : "delta", property_count);
    if (mqtt_publish_json(g_mqtt.topic_telemetry, root, 0)) {
        g_last_telemetry_snapshot = current;
        g_has_last_telemetry_snapshot = true;
    }
    cJSON_Delete(root);
}

void publish_telemetry()
{
    publish_telemetry_snapshot(true);
}

void publish_telemetry_if_connected()
{
    if (g_mqtt_connected.load()) {
        publish_telemetry_snapshot(false);
    }
}

void stop_mqtt()
{
    if (g_mqtt.client == nullptr) {
        return;
    }

    ESP_LOGI(kTag, "stopping mqtt client");
    g_mqtt_connected.store(false);
    g_mqtt.connected = false;
    clear_pending_weather_request();
    esp_mqtt_client_stop(g_mqtt.client);
    esp_mqtt_client_destroy(g_mqtt.client);
    g_mqtt.client = nullptr;
}

std::string json_string(cJSON *obj, const char *name);
double json_number(cJSON *obj, const char *name, double fallback);
void apply_timezone();
bool sync_time();
void update_weather();
void handle_mqtt_response(const std::string &payload);

bool resolve_weather_coordinates(double *lat, double *lon)
{
    if (lat == nullptr || lon == nullptr) {
        return false;
    }

    if (!parse_double_string(g_config.latitude, lat) || !parse_double_string(g_config.longitude, lon)) {
        g_weather.status_line = "SET LAT/LON";
        ESP_LOGW(kTag, "weather skipped: latitude/longitude is missing or invalid");
        publish_weather_refresh_failed_event("parse", -30);
        return false;
    }

    if (*lat < -90.0 || *lat > 90.0 || *lon < -180.0 || *lon > 180.0) {
        g_weather.status_line = "BAD LAT/LON";
        ESP_LOGW(kTag, "weather skipped: latitude/longitude out of range lat=%.6f lon=%.6f", *lat, *lon);
        publish_weather_refresh_failed_event("parse", -31);
        return false;
    }

    return true;
}

void handle_mqtt_command(const std::string &payload)
{
    cJSON *root = cJSON_ParseWithLength(payload.c_str(), payload.size());
    if (root == nullptr) {
        publish_cmd_ack("", false, "BAD_PAYLOAD", "invalid json");
        return;
    }

    const int schema_version = static_cast<int>(json_number(root, "schemaVersion", -1));
    const std::string type = json_string(root, "type");
    const std::string req_id = json_string(root, "reqId");
    cJSON *payload_obj = cJSON_GetObjectItemCaseSensitive(root, "payload");

    if (schema_version != 1 || type.empty() || req_id.empty()) {
        publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "missing required fields");
        cJSON_Delete(root);
        return;
    }

    if (mqtt_req_id_seen(req_id)) {
        publish_cmd_ack(req_id, false, "DUPLICATE", "duplicate reqId");
        cJSON_Delete(root);
        return;
    }
    remember_mqtt_req_id(req_id);

    if (type == "setBrightness") {
        if (payload_obj == nullptr) {
            publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "missing payload");
        } else {
            const int brightness = static_cast<int>(json_number(payload_obj, "brightness", -1));
            if (brightness < 0 || brightness > 100) {
                publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "brightness out of range");
            } else {
                apply_display_brightness(brightness);
                publish_telemetry_if_connected();
                publish_cmd_ack(req_id, true, "DONE", "brightness updated");
            }
        }
    } else if (type == "setEmotion") {
        if (payload_obj == nullptr) {
            publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "missing payload");
        } else {
            const std::string emotion = json_string(payload_obj, "emotion");
            int mood = MOOD_NORMAL;
            if (!emotion_to_mood(emotion, &mood)) {
                publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "invalid emotion");
            } else {
                g_eyes.current_mood = mood;
                g_eyes.last_saccade = 0;
                publish_telemetry_if_connected();
                publish_cmd_ack(req_id, true, "DONE", "emotion updated");
            }
        }
    } else if (type == "setPage") {
        if (payload_obj == nullptr) {
            publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "missing payload");
        } else {
            const std::string page_value = json_string(payload_obj, "page");
            int page = PAGE_EMO;
            if (!parse_page(page_value, &page)) {
                publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "invalid page");
            } else {
                g_ui.current_page = page;
                g_ui.last_page_switch = millis();
                publish_telemetry_if_connected();
                publish_cmd_ack(req_id, true, "DONE", "page updated");
            }
        }
    } else if (type == "setWeatherConfig") {
        if (payload_obj == nullptr) {
            publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "missing payload");
        } else {
            DeviceConfig next = g_config;

            cJSON *city_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "city");
            cJSON *latitude_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "latitude");
            cJSON *longitude_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "longitude");
            cJSON *tz_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "timezone");

            bool has_update = false;

            if (cJSON_IsString(city_item) && city_item->valuestring != nullptr) {
                next.city = city_item->valuestring;
                has_update = true;
            }
            if (cJSON_IsString(latitude_item) && latitude_item->valuestring != nullptr) {
                next.latitude = latitude_item->valuestring;
                has_update = true;
            }
            if (cJSON_IsNumber(latitude_item)) {
                next.latitude = std::to_string(latitude_item->valuedouble);
                has_update = true;
            }
            if (cJSON_IsString(longitude_item) && longitude_item->valuestring != nullptr) {
                next.longitude = longitude_item->valuestring;
                has_update = true;
            }
            if (cJSON_IsNumber(longitude_item)) {
                next.longitude = std::to_string(longitude_item->valuedouble);
                has_update = true;
            }
            if (cJSON_IsString(tz_item) && tz_item->valuestring != nullptr) {
                next.tz = tz_item->valuestring;
                has_update = true;
            }

            if (!has_update) {
                publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "no supported weather config fields");
            } else if (next.latitude.empty() || next.longitude.empty() || next.tz.empty()) {
                publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "latitude, longitude and timezone must not be empty");
            } else {
                const esp_err_t save_err = save_config(next);
                if (save_err != ESP_OK) {
                    publish_cmd_ack(req_id, false, "INTERNAL_ERROR", "save config failed");
                } else {
                    g_config = next;
                    apply_timezone();
                    clear_pending_weather_request();
                    if (g_wifi_connected.load()) {
                        sync_time();
                        update_weather();
                    }
                    publish_telemetry_if_connected();
                    publish_cmd_ack(req_id, true, "DONE", "weather config updated");
                }
            }
        }
    } else if (type == "setWeatherTheme") {
        if (payload_obj == nullptr) {
            publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "missing payload");
        } else {
            const std::string theme = json_string(payload_obj, "theme");
            int parsed_theme = WEATHER_THEME_CLASSIC;
            if (!parse_weather_theme(theme, &parsed_theme)) {
                publish_cmd_ack(req_id, false, "BAD_PAYLOAD", "invalid weather theme");
            } else {
                DeviceConfig next = g_config;
                next.weather_theme = weather_theme_to_string(parsed_theme);
                const esp_err_t save_err = save_config(next);
                if (save_err != ESP_OK) {
                    publish_cmd_ack(req_id, false, "INTERNAL_ERROR", "save config failed");
                } else {
                    g_config = next;
                    if (g_ui.weather_theme_picker_active) {
                        g_ui.weather_theme_preview = parsed_theme;
                        g_ui.weather_theme_original = parsed_theme;
                    }
                    publish_telemetry_if_connected();
                    publish_cmd_ack(req_id, true, "DONE", "weather theme updated");
                }
            }
        }
    } else if (type == "reboot") {
        schedule_restart();
        publish_cmd_ack(req_id, true, "DONE", "reboot scheduled");
    } else {
        publish_cmd_ack(req_id, false, "UNSUPPORTED_TYPE", "unsupported command for deskbuddy-v1");
    }

    cJSON_Delete(root);
}

void mqtt_event_handler(void *, esp_event_base_t, int32_t event_id, void *event_data)
{
    auto *event = static_cast<esp_mqtt_event_handle_t>(event_data);
    if (event == nullptr) {
        return;
    }

    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
    case MQTT_EVENT_CONNECTED:
        g_mqtt.connected = true;
        g_mqtt_connected.store(true);
        ESP_LOGI(kTag, "mqtt connected");
        esp_mqtt_client_subscribe(g_mqtt.client, g_mqtt.topic_cmd.c_str(), 1);
        esp_mqtt_client_subscribe(g_mqtt.client, g_mqtt.topic_resp.c_str(), 1);
        publish_telemetry();
        if (g_wifi_connected.load() && weather_location_configured()) {
            update_weather();
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        g_mqtt.connected = false;
        g_mqtt_connected.store(false);
        clear_pending_weather_request();
        ESP_LOGW(kTag, "mqtt disconnected");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(kTag, "mqtt published ack: msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA: {
        const std::string topic(event->topic, event->topic_len);
        const std::string payload(event->data, event->data_len);
        ESP_LOGI(kTag, "mqtt data on topic=%s", topic.c_str());
        if (topic == g_mqtt.topic_cmd) {
            handle_mqtt_command(payload);
        } else if (topic == g_mqtt.topic_resp) {
            handle_mqtt_response(payload);
        }
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGW(kTag, "mqtt error event received: error_handle=%p", event->error_handle);
        break;
    default:
        break;
    }
}

void start_mqtt()
{
    if (!mqtt_config_ready(g_config)) {
        ESP_LOGW(kTag, "mqtt disabled: host/deviceId/secret is missing or still placeholder");
        return;
    }
    if (g_mqtt.client != nullptr) {
        ESP_LOGI(kTag, "mqtt client already started");
        return;
    }

    build_mqtt_topics();
    g_mqtt.broker_uri = "mqtt://" + g_factory_mqtt.host + ":" + std::to_string(g_factory_mqtt.port);

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = g_mqtt.broker_uri.c_str();
    mqtt_cfg.credentials.client_id = g_factory_mqtt.device_id.c_str();
    mqtt_cfg.credentials.username = g_factory_mqtt.device_id.c_str();
    mqtt_cfg.credentials.authentication.password = g_factory_mqtt.secret.c_str();
    mqtt_cfg.network.disable_auto_reconnect = false;

    g_mqtt.client = esp_mqtt_client_init(&mqtt_cfg);
    if (g_mqtt.client == nullptr) {
        ESP_LOGE(kTag, "failed to init mqtt client");
        return;
    }

    esp_mqtt_client_register_event(g_mqtt.client, MQTT_EVENT_ANY, mqtt_event_handler, nullptr);
    ESP_ERROR_CHECK(esp_mqtt_client_start(g_mqtt.client));
    ESP_LOGI(
        kTag,
        "mqtt starting: broker=%s deviceId=%s source=%s",
        g_mqtt.broker_uri.c_str(),
        g_factory_mqtt.device_id.c_str(),
        g_factory_mqtt.loaded_from_factory_nvs ? "factory_nvs" : "firmware_default");
}

void log_visible_wifi_networks()
{
    ESP_LOGI(kTag, "starting wifi scan for diagnostics");

    const esp_err_t disconnect_err = esp_wifi_disconnect();
    if (disconnect_err != ESP_OK && disconnect_err != ESP_ERR_WIFI_NOT_CONNECT) {
        ESP_LOGW(kTag, "wifi disconnect before scan failed: %s", esp_err_to_name(disconnect_err));
    }

    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = true;

    const esp_err_t scan_err = esp_wifi_scan_start(&scan_config, true);
    if (scan_err != ESP_OK) {
        ESP_LOGW(kTag, "wifi scan start failed: %s", esp_err_to_name(scan_err));
        return;
    }

    uint16_t ap_count = 0;
    if (esp_wifi_scan_get_ap_num(&ap_count) != ESP_OK) {
        ESP_LOGW(kTag, "wifi scan get ap count failed");
        return;
    }

    if (ap_count == 0) {
        ESP_LOGW(kTag, "wifi scan found no access points");
        return;
    }

    std::vector<wifi_ap_record_t> records(ap_count);
    uint16_t fetched = ap_count;
    const esp_err_t records_err = esp_wifi_scan_get_ap_records(&fetched, records.data());
    if (records_err != ESP_OK) {
        ESP_LOGW(kTag, "wifi scan get records failed: %s", esp_err_to_name(records_err));
        return;
    }

    ESP_LOGI(kTag, "wifi scan found %u access points", static_cast<unsigned>(fetched));
    for (uint16_t i = 0; i < fetched; ++i) {
        const auto &record = records[i];
        size_t ssid_len = 0;
        while (ssid_len < sizeof(record.ssid) && record.ssid[ssid_len] != 0) {
            ++ssid_len;
        }

        const std::string ssid_text(reinterpret_cast<const char *>(record.ssid), ssid_len);
        const std::string ssid_hex = bytes_to_hex(record.ssid, ssid_len);
        ESP_LOGI(
            kTag,
            "scan[%u]: ssid='%s' bytes=%u hex=[%s] rssi=%d auth=%d",
            static_cast<unsigned>(i),
            ssid_text.c_str(),
            static_cast<unsigned>(ssid_len),
            ssid_hex.c_str(),
            static_cast<int>(record.rssi),
            static_cast<int>(record.authmode));
    }
}

bool connect_station(const DeviceConfig &config)
{
    ESP_LOGI(
        kTag,
        "connecting to sta ssid='%s' (bytes=%u)",
        config.ssid.c_str(),
        static_cast<unsigned>(config.ssid.size()));
    g_ui.in_config_mode = false;
    g_wifi_connected.store(false);
    if (g_wifi_event_group != nullptr) {
        xEventGroupClearBits(g_wifi_event_group, kWifiConnectedBit | kWifiDisconnectedBit);
    }

    stop_wifi_safely();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {};
    if (!copy_wifi_ssid_bytes(wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), config.ssid)) {
        ESP_LOGE(kTag, "cannot connect: ssid byte length exceeds ESP32 limit");
        return false;
    }
    copy_to_buffer(reinterpret_cast<char *>(wifi_config.sta.password), sizeof(wifi_config.sta.password), config.pass);
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        g_wifi_event_group,
        kWifiConnectedBit | kWifiDisconnectedBit,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(kWifiConnectTimeoutMs));

    if ((bits & kWifiConnectedBit) != 0) {
        ESP_LOGI(kTag, "connected to wifi ssid=%s", config.ssid.c_str());
        return true;
    }

    log_visible_wifi_networks();
    publish_wifi_connect_failed_event(config.ssid, g_mqtt.last_wifi_disconnect_reason, g_mqtt.wifi_retry_count);
    ESP_LOGW(kTag, "wifi connect timed out for ssid='%s'", config.ssid.c_str());
    return false;
}

std::string build_config_page_html()
{
    std::string html =
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>DeskBuddy Config</title><style>"
        "body{font-family:sans-serif;max-width:420px;margin:30px auto;padding:24px;background:#0c1929;color:#e8f4fc;}"
        "h1{color:#5ba3f5;margin-bottom:8px;}input{width:100%;padding:10px;margin:6px 0;border:1px solid #2d4a6f;border-radius:6px;"
        "box-sizing:border-box;background:#1a2d47;color:#e8f4fc;}button{width:100%;padding:12px;background:#3498db;color:#fff;border:none;"
        "border-radius:6px;font-size:16px;cursor:pointer;margin-top:16px;}label{display:block;margin-top:14px;color:#8ab4e8;font-size:14px;}"
        ".section{margin-top:20px;padding-top:16px;border-top:1px solid #1e3a5f;}.section-title{color:#5ba3f5;font-size:13px;margin-bottom:8px;}"
        "</style></head><body><h1>DeskBuddy Setup</h1><form action=\"/save\" method=\"POST\" accept-charset=\"UTF-8\">"
        "<label>WiFi SSID</label><input name=\"ssid\" placeholder=\"Your WiFi name\" value=\"";
    html += html_escape(g_config.ssid);
    html += "\"><label>WiFi Password</label><input name=\"pass\" type=\"password\" placeholder=\"WiFi password\">";
    html += "<div class=\"section\"><div class=\"section-title\">Weather (Server Sync)</div>"
            "<label>Location Name (Optional)</label><input name=\"city\" placeholder=\"e.g. Shenzhen\" value=\"";
    html += html_escape(g_config.city);
    html += "\"><label>Latitude</label><input name=\"latitude\" placeholder=\"e.g. 22.5431\" value=\"";
    html += html_escape(g_config.latitude);
    html += "\"><label>Longitude</label><input name=\"longitude\" placeholder=\"e.g. 114.0579\" value=\"";
    html += html_escape(g_config.longitude);
    html += "\"></div><div class=\"section\"><div class=\"section-title\">Time</div>"
            "<label>Timezone</label><input name=\"tz\" placeholder=\"e.g. Asia/Shanghai\" value=\"";
    html += html_escape(g_config.tz);
    html += "\"></div><button type=\"submit\">Save &amp; Reboot</button></form></body></html>";
    return html;
}

void draw_config_screen()
{
    ESP_LOGI(kTag, "drawing config screen");
    g_display.clear_display();
    draw_tiny_text(0, 0, "配置模式", 1, 1);
    draw_tiny_text(0, 18, "连接热点：", 1, 1);
    draw_tiny_text(0, 27, kConfigApSsid, 1, 1);
    draw_tiny_text(0, 40, "打开 192.168.4.1", 1, 1);
    g_display.display();
}

esp_err_t handle_config_root(httpd_req_t *req)
{
    ESP_LOGI(kTag, "http GET /");
    const std::string html = build_config_page_html();
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html.c_str(), static_cast<int>(html.size()));
}

esp_err_t handle_config_save(httpd_req_t *req)
{
    ESP_LOGI(kTag, "http POST /save content_len=%d", req->content_len);
    std::string body;
    body.resize(static_cast<size_t>(req->content_len));

    int total_received = 0;
    while (total_received < req->content_len) {
        const int received = httpd_req_recv(req, body.data() + total_received, req->content_len - total_received);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        total_received += received;
    }

    const auto pairs = parse_form_urlencoded(body);
    DeviceConfig next = g_config;
    next.ssid = get_form_value(pairs, "ssid");
    next.pass = get_form_value(pairs, "pass");
    next.city = get_form_value(pairs, "city");
    next.latitude = get_form_value(pairs, "latitude");
    next.longitude = get_form_value(pairs, "longitude");
    next.tz = get_form_value(pairs, "tz");

    if (next.pass.empty()) {
        next.pass = g_config.pass;
    }

    if (next.ssid.empty()) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_sendstr(req, "SSID required");
    }

    const esp_err_t save_err = save_config(next);
    if (save_err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_sendstr(req, "Save failed");
    }

    g_config = next;
    httpd_resp_set_type(req, "text/html");
    const esp_err_t send_err = httpd_resp_sendstr(
        req,
        "<html><body style='font-family:sans-serif;background:#0c1929;color:#e8f4fc;padding:40px;'>"
        "<h2 style='color:#5ba3f5'>Saved!</h2><p>Rebooting in 2 seconds...</p></body></html>");
    schedule_restart();
    return send_err;
}

void start_http_server()
{
    if (g_http_server != nullptr) {
        ESP_LOGI(kTag, "http server already started");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    ESP_ERROR_CHECK(httpd_start(&g_http_server, &config));

    httpd_uri_t root = {};
    root.uri = "/";
    root.method = HTTP_GET;
    root.handler = handle_config_root;
    ESP_ERROR_CHECK(httpd_register_uri_handler(g_http_server, &root));

    httpd_uri_t save = {};
    save.uri = "/save";
    save.method = HTTP_POST;
    save.handler = handle_config_save;
    ESP_ERROR_CHECK(httpd_register_uri_handler(g_http_server, &save));
    ESP_LOGI(kTag, "http config server started");
}

void start_config_portal(const char *reason)
{
    ESP_LOGW(kTag, "entering config portal mode");
    publish_config_portal_entered_event(reason);
    g_ui.in_config_mode = true;
    g_wifi_connected.store(false);
    stop_mqtt();
    stop_wifi_safely();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_config_t ap_config = {};
    copy_to_buffer(reinterpret_cast<char *>(ap_config.ap.ssid), sizeof(ap_config.ap.ssid), kConfigApSsid);
    copy_to_buffer(reinterpret_cast<char *>(ap_config.ap.password), sizeof(ap_config.ap.password), kConfigApPass);
    ap_config.ap.ssid_len = sizeof(kConfigApSsid) - 1;
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap_config.ap.pmf_cfg.required = false;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info = {};
    if (g_ap_netif != nullptr && esp_netif_get_ip_info(g_ap_netif, &ip_info) == ESP_OK) {
        ESP_LOGI(kTag, "config ap started: ssid='%s' ip=" IPSTR, kConfigApSsid, IP2STR(&ip_info.ip));
    } else {
        ESP_LOGI(kTag, "config ap started: ssid='%s'", kConfigApSsid);
    }

    start_http_server();
    draw_config_screen();
}

std::string json_string(cJSON *obj, const char *name)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, name);
    if (!cJSON_IsString(item) || item->valuestring == nullptr) {
        return {};
    }
    return item->valuestring;
}

double json_number(cJSON *obj, const char *name, double fallback = 0.0)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, name);
    if (!cJSON_IsNumber(item)) {
        return fallback;
    }
    return item->valuedouble;
}

bool local_time_ready(struct tm *out_time)
{
    std::time_t now = 0;
    std::time(&now);
    if (now < 1700000000) {
        return false;
    }
    localtime_r(&now, out_time);
    return true;
}

std::string resolve_posix_timezone(const std::string &timezone)
{
    if (timezone.empty()) {
        return "UTC0";
    }

    for (const auto &alias : kAliases) {
        if (timezone == alias.iana) {
            return alias.posix;
        }
    }
    return timezone;
}

void apply_timezone()
{
    const std::string tz = resolve_posix_timezone(g_config.tz);
    setenv("TZ", tz.c_str(), 1);
    tzset();
    ESP_LOGI(kTag, "timezone applied: source='%s' posix='%s'", g_config.tz.c_str(), tz.c_str());
}

bool sync_time()
{
    ESP_LOGI(kTag, "starting SNTP sync using %s", kNtpServer);
    esp_sntp_stop();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, const_cast<char *>(kNtpServer));
    esp_sntp_init();

    for (int attempt = 0; attempt < 20; ++attempt) {
        struct tm time_info = {};
        if (local_time_ready(&time_info)) {
            ESP_LOGI(kTag, "time sync completed on attempt %d", attempt + 1);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGW(kTag, "time sync timed out");
    return false;
}

void update_mood_based_on_weather()
{
    int mood = MOOD_NORMAL;
    if (g_weather.weather_main == "Clear") {
        mood = MOOD_HAPPY;
    } else if (g_weather.weather_main == "Rain" || g_weather.weather_main == "Drizzle") {
        mood = MOOD_SAD;
    } else if (g_weather.weather_main == "Thunderstorm") {
        mood = MOOD_SURPRISED;
    } else if (g_weather.temperature > 25.0F) {
        mood = MOOD_EXCITED;
    } else if (g_weather.temperature < 5.0F) {
        mood = MOOD_SLEEPY;
    }
    g_eyes.current_mood = mood;
}

std::string build_weather_req_id()
{
    char buffer[80] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "weather-%llu-%08lx",
        static_cast<unsigned long long>(current_timestamp_ms()),
        static_cast<unsigned long>(esp_random()));
    return buffer;
}

int weather_response_status_code(const std::string &code)
{
    if (code == "DONE") {
        return 200;
    }
    if (code == "BAD_PAYLOAD") {
        return 400;
    }
    if (code == "UNSUPPORTED_TYPE") {
        return 422;
    }
    if (code == "TIMEOUT") {
        return 408;
    }
    if (code == "RATE_LIMITED") {
        return 429;
    }
    if (code == "UPSTREAM_ERROR") {
        return 502;
    }
    if (code == "INTERNAL_ERROR") {
        return 500;
    }
    return -60;
}

bool parse_forecast_label(const std::string &date, std::string *label)
{
    if (label == nullptr) {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }

    struct tm forecast_tm = {};
    forecast_tm.tm_year = year - 1900;
    forecast_tm.tm_mon = month - 1;
    forecast_tm.tm_mday = day;
    forecast_tm.tm_isdst = -1;
    if (mktime(&forecast_tm) == static_cast<std::time_t>(-1)) {
        return false;
    }

    static const std::array<const char *, 7> kZhWeekdays = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    const int weekday = (forecast_tm.tm_wday >= 0 && forecast_tm.tm_wday < 7) ? forecast_tm.tm_wday : 0;
    *label = kZhWeekdays[weekday];
    return true;
}

void mark_weather_refresh_failed(const char *stage, int status_code, const char *status_line)
{
    clear_pending_weather_request();
    g_weather.stale = g_weather.has_current || g_weather.has_forecast;
    if (status_line != nullptr) {
        g_weather.status_line = status_line;
    }
    publish_weather_refresh_failed_event(stage, status_code);
    publish_telemetry_if_connected();
}

bool parse_weather_response_payload(cJSON *payload_obj, WeatherData *next_weather)
{
    if (!cJSON_IsObject(payload_obj) || next_weather == nullptr) {
        ESP_LOGW(
            kTag,
            "weather payload parse failed: payloadIsObject=%s nextWeather=%s",
            cJSON_IsObject(payload_obj) ? "true" : "false",
            next_weather != nullptr ? "true" : "false");
        return false;
    }

    cJSON *city_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "city");
    cJSON *timezone_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "timezone");
    cJSON *weather_main_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "weatherMain");
    cJSON *weather_desc_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "weatherDesc");
    cJSON *temperature_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "temperature");
    cJSON *feels_like_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "feelsLike");
    cJSON *humidity_item = cJSON_GetObjectItemCaseSensitive(payload_obj, "humidity");
    cJSON *forecast_obj = cJSON_GetObjectItemCaseSensitive(payload_obj, "weatherForecast");
    cJSON *days = forecast_obj != nullptr ? cJSON_GetObjectItemCaseSensitive(forecast_obj, "days") : nullptr;

    if (!cJSON_IsString(city_item) ||
        !cJSON_IsString(timezone_item) ||
        !cJSON_IsString(weather_main_item) ||
        !cJSON_IsString(weather_desc_item) ||
        !cJSON_IsNumber(temperature_item) ||
        !cJSON_IsNumber(feels_like_item) ||
        !cJSON_IsNumber(humidity_item) ||
        !cJSON_IsArray(days)) {
        ESP_LOGW(
            kTag,
            "weather payload fields invalid: city=%s timezone=%s weatherMain=%s weatherDesc=%s temperature=%s feelsLike=%s humidity=%s days=%s",
            cJSON_IsString(city_item) ? "ok" : "bad",
            cJSON_IsString(timezone_item) ? "ok" : "bad",
            cJSON_IsString(weather_main_item) ? "ok" : "bad",
            cJSON_IsString(weather_desc_item) ? "ok" : "bad",
            cJSON_IsNumber(temperature_item) ? "ok" : "bad",
            cJSON_IsNumber(feels_like_item) ? "ok" : "bad",
            cJSON_IsNumber(humidity_item) ? "ok" : "bad",
            cJSON_IsArray(days) ? "ok" : "bad");
        return false;
    }

    std::array<ForecastDay, 3> next_forecast = {};
    const int day_count = cJSON_GetArraySize(days);
    const int consume_count = std::min(day_count, static_cast<int>(next_forecast.size()));
    if (consume_count <= 0) {
        ESP_LOGW(kTag, "weather payload has no forecast days: dayCount=%d", day_count);
        return false;
    }

    for (int i = 0; i < consume_count; ++i) {
        cJSON *entry = cJSON_GetArrayItem(days, i);
        if (!cJSON_IsObject(entry)) {
            ESP_LOGW(kTag, "weather forecast entry invalid: index=%d isObject=%s", i, cJSON_IsObject(entry) ? "true" : "false");
            return false;
        }

        cJSON *date_item = cJSON_GetObjectItemCaseSensitive(entry, "date");
        cJSON *entry_main_item = cJSON_GetObjectItemCaseSensitive(entry, "weatherMain");
        cJSON *entry_desc_item = cJSON_GetObjectItemCaseSensitive(entry, "weatherDesc");
        cJSON *temp_min_item = cJSON_GetObjectItemCaseSensitive(entry, "tempMin");
        cJSON *temp_max_item = cJSON_GetObjectItemCaseSensitive(entry, "tempMax");
        cJSON *entry_humidity_item = cJSON_GetObjectItemCaseSensitive(entry, "humidity");

        if (!cJSON_IsString(date_item) ||
            !cJSON_IsString(entry_main_item) ||
            !cJSON_IsString(entry_desc_item) ||
            !cJSON_IsNumber(temp_min_item) ||
            !cJSON_IsNumber(temp_max_item) ||
            !cJSON_IsNumber(entry_humidity_item)) {
            ESP_LOGW(
                kTag,
                "weather forecast fields invalid: index=%d date=%s weatherMain=%s weatherDesc=%s tempMin=%s tempMax=%s humidity=%s",
                i,
                cJSON_IsString(date_item) ? "ok" : "bad",
                cJSON_IsString(entry_main_item) ? "ok" : "bad",
                cJSON_IsString(entry_desc_item) ? "ok" : "bad",
                cJSON_IsNumber(temp_min_item) ? "ok" : "bad",
                cJSON_IsNumber(temp_max_item) ? "ok" : "bad",
                cJSON_IsNumber(entry_humidity_item) ? "ok" : "bad");
            return false;
        }

        ForecastDay forecast = {};
        forecast.date = date_item->valuestring;
        forecast.weather_main = entry_main_item->valuestring;
        forecast.weather_desc = to_display_text(entry_desc_item->valuestring);
        forecast.temp_min = static_cast<float>(temp_min_item->valuedouble);
        forecast.temp_max = static_cast<float>(temp_max_item->valuedouble);
        forecast.humidity = static_cast<int>(std::lround(entry_humidity_item->valuedouble));
        if (!parse_forecast_label(forecast.date, &forecast.day_name)) {
            ESP_LOGW(kTag, "weather forecast date invalid: index=%d date='%s'", i, forecast.date.c_str());
            return false;
        }
        forecast.valid = true;
        next_forecast[static_cast<size_t>(i)] = forecast;
        ESP_LOGI(
            kTag,
            "weather forecast parsed: index=%d date='%s' day='%s' main='%s' min=%.1f max=%.1f humidity=%d",
            i,
            forecast.date.c_str(),
            forecast.day_name.c_str(),
            forecast.weather_main.c_str(),
            static_cast<double>(forecast.temp_min),
            static_cast<double>(forecast.temp_max),
            forecast.humidity);
    }

    next_weather->temperature = static_cast<float>(temperature_item->valuedouble);
    next_weather->feels_like = static_cast<float>(feels_like_item->valuedouble);
    next_weather->humidity = static_cast<int>(std::lround(humidity_item->valuedouble));
    next_weather->weather_main = weather_main_item->valuestring;
    next_weather->weather_desc = to_display_text(weather_desc_item->valuestring);
    next_weather->forecast = next_forecast;
    next_weather->has_current = true;
    next_weather->has_forecast = true;
    next_weather->stale = false;
    next_weather->status_line = "OK";
    return true;
}

void handle_mqtt_response(const std::string &payload)
{
    ESP_LOGI(kTag, "weather resp raw payload: %s", payload.c_str());
    cJSON *root = cJSON_ParseWithLength(payload.c_str(), payload.size());
    if (root == nullptr) {
        ESP_LOGW(kTag, "mqtt resp ignored: invalid json");
        return;
    }

    const int schema_version = static_cast<int>(json_number(root, "schemaVersion", -1));
    const std::string type = json_string(root, "type");
    const std::string req_id = json_string(root, "reqId");
    if (schema_version != 1 || type != "getWeather" || req_id.empty()) {
        ESP_LOGI(kTag, "mqtt resp ignored: schema=%d type='%s' reqId='%s'", schema_version, type.c_str(), req_id.c_str());
        cJSON_Delete(root);
        return;
    }

    if (!g_mqtt.weather_req_pending || req_id != g_mqtt.weather_req_id) {
        ESP_LOGI(kTag, "mqtt resp ignored: pending=%s expectedReqId='%s' actualReqId='%s'",
            g_mqtt.weather_req_pending ? "true" : "false",
            g_mqtt.weather_req_id.c_str(),
            req_id.c_str());
        cJSON_Delete(root);
        return;
    }

    cJSON *ok_item = cJSON_GetObjectItemCaseSensitive(root, "ok");
    cJSON *code_item = cJSON_GetObjectItemCaseSensitive(root, "code");
    cJSON *payload_obj = cJSON_GetObjectItemCaseSensitive(root, "payload");
    if (!cJSON_IsBool(ok_item) || !cJSON_IsString(code_item)) {
        ESP_LOGW(
            kTag,
            "weather resp missing top-level fields: ok=%s code=%s payloadIsObject=%s",
            cJSON_IsBool(ok_item) ? "ok" : "bad",
            cJSON_IsString(code_item) ? "ok" : "bad",
            cJSON_IsObject(payload_obj) ? "true" : "false");
        cJSON_Delete(root);
        mark_weather_refresh_failed("parse", -50, "RESP PARSE");
        return;
    }

    const bool ok = cJSON_IsTrue(ok_item);
    const std::string code = code_item->valuestring != nullptr ? code_item->valuestring : "";
    ESP_LOGI(
        kTag,
        "weather resp matched: reqId=%s ok=%s code=%s pendingForMs=%lu",
        req_id.c_str(),
        ok ? "true" : "false",
        code.c_str(),
        millis() - g_mqtt.weather_req_sent_at);
    if (!ok) {
        cJSON_Delete(root);
        mark_weather_refresh_failed("response", weather_response_status_code(code), "RESP FAIL");
        return;
    }

    WeatherData next_weather = g_weather;
    if (!parse_weather_response_payload(payload_obj, &next_weather)) {
        cJSON_Delete(root);
        mark_weather_refresh_failed("parse", -51, "RESP PARSE");
        return;
    }

    g_weather = next_weather;
    clear_pending_weather_request();
    update_mood_based_on_weather();
    publish_telemetry_if_connected();
    ESP_LOGI(
        kTag,
        "weather updated from mqtt: temp=%.1f feels=%.1f humidity=%d main='%s' forecast=%s",
        static_cast<double>(g_weather.temperature),
        static_cast<double>(g_weather.feels_like),
        g_weather.humidity,
        g_weather.weather_main.c_str(),
        g_weather.has_forecast ? "true" : "false");
    cJSON_Delete(root);
}

void poll_weather_request_timeout()
{
    if (!g_mqtt.weather_req_pending) {
        return;
    }

    const unsigned long now = millis();
    if (now - g_mqtt.weather_req_sent_at < kWeatherRequestTimeoutMs) {
        return;
    }

    ESP_LOGW(
        kTag,
        "weather request timed out: reqId=%s waitedMs=%lu timeoutMs=%lu",
        g_mqtt.weather_req_id.c_str(),
        now - g_mqtt.weather_req_sent_at,
        kWeatherRequestTimeoutMs);
    mark_weather_refresh_failed("timeout", -41, "TIMEOUT");
}

void update_weather()
{
    if (!g_wifi_connected.load()) {
        g_weather.status_line = "NO WIFI";
        ESP_LOGW(
            kTag,
            "weather skipped: wifi disconnected city='%s' lat='%s' lon='%s' tz='%s'",
            g_config.city.c_str(),
            g_config.latitude.c_str(),
            g_config.longitude.c_str(),
            g_config.tz.c_str());
        return;
    }
    if (!g_mqtt_connected.load()) {
        g_weather.status_line = "NO MQTT";
        ESP_LOGW(
            kTag,
            "weather skipped: mqtt disconnected city='%s' lat='%s' lon='%s' tz='%s'",
            g_config.city.c_str(),
            g_config.latitude.c_str(),
            g_config.longitude.c_str(),
            g_config.tz.c_str());
        return;
    }
    if (!weather_location_configured()) {
        g_weather.status_line = "SET WEATHER";
        ESP_LOGW(
            kTag,
            "weather skipped: latitude/longitude/timezone is incomplete city='%s' lat='%s' lon='%s' tz='%s'",
            g_config.city.c_str(),
            g_config.latitude.c_str(),
            g_config.longitude.c_str(),
            g_config.tz.c_str());
        publish_weather_refresh_failed_event("parse", -32);
        return;
    }
    if (g_mqtt.weather_req_pending) {
        ESP_LOGI(
            kTag,
            "weather request already pending: reqId=%s elapsedMs=%lu",
            g_mqtt.weather_req_id.c_str(),
            millis() - g_mqtt.weather_req_sent_at);
        return;
    }

    double lat = 0.0;
    double lon = 0.0;
    if (!resolve_weather_coordinates(&lat, &lon)) {
        ESP_LOGW(
            kTag,
            "weather resolve coordinates failed: city='%s' lat='%s' lon='%s' tz='%s'",
            g_config.city.c_str(),
            g_config.latitude.c_str(),
            g_config.longitude.c_str(),
            g_config.tz.c_str());
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *payload_obj = cJSON_CreateObject();
    if (root == nullptr || payload_obj == nullptr) {
        if (payload_obj != nullptr) {
            cJSON_Delete(payload_obj);
        }
        if (root != nullptr) {
            cJSON_Delete(root);
        }
        mark_weather_refresh_failed("request", -42, "REQ FAIL");
        return;
    }

    const std::string req_id = build_weather_req_id();
    cJSON_AddNumberToObject(root, "schemaVersion", 1);
    cJSON_AddStringToObject(root, "reqId", req_id.c_str());
    cJSON_AddStringToObject(root, "type", "getWeather");
    cJSON_AddNumberToObject(root, "ts", static_cast<double>(std::time(nullptr)));
    cJSON_AddStringToObject(payload_obj, "city", g_config.city.c_str());
    cJSON_AddNumberToObject(payload_obj, "latitude", lat);
    cJSON_AddNumberToObject(payload_obj, "longitude", lon);
    cJSON_AddStringToObject(payload_obj, "timezone", g_config.tz.c_str());
    cJSON_AddItemToObject(root, "payload", payload_obj);

    char *request_json = cJSON_PrintUnformatted(root);
    if (request_json != nullptr) {
        ESP_LOGI(kTag, "weather req raw payload: %s", request_json);
        cJSON_free(request_json);
    }

    if (!mqtt_publish_json(g_mqtt.topic_req, root, 1)) {
        cJSON_Delete(root);
        mark_weather_refresh_failed("request", -40, "REQ FAIL");
        return;
    }

    cJSON_Delete(root);
    g_mqtt.weather_req_pending = true;
    g_mqtt.weather_req_id = req_id;
    g_mqtt.weather_req_sent_at = millis();
    g_weather.status_line = "WAIT RESP";
    ESP_LOGI(
        kTag,
        "weather request sent: reqId=%s city='%s' lat=%.6f lon=%.6f tz='%s'",
        req_id.c_str(),
        g_config.city.c_str(),
        lat,
        lon,
        g_config.tz.c_str());
}
