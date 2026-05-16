#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include "arduino_compat.h"

extern "C" {
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
}

class Ssd1306Display;
extern Ssd1306Display g_display;

constexpr char kTag[] = "deskbuddy";
constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr uint8_t kOledColumnOffset = 0x02;
constexpr uint8_t kOledAddress = 0x3C;
constexpr gpio_num_t kOledSdaPin = GPIO_NUM_8;
constexpr gpio_num_t kOledSclPin = GPIO_NUM_9;
constexpr gpio_num_t kTouchPin = GPIO_NUM_7;
constexpr i2c_port_t kI2cPort = I2C_NUM_0;
constexpr uint32_t kI2cClockHz = 100000;

constexpr char kConfigApSsid[] = "DeskBuddy-Setup";
constexpr char kConfigApPass[] = "12345678";
constexpr char kNtpServer[] = "pool.ntp.org";
constexpr char kConfigNamespace[] = "deskbuddy";
constexpr char kFactoryNvsPartition[] = "factory_nvs";
constexpr char kFactoryConfigNamespace[] = "factory_cfg";

constexpr unsigned long kConfigHoldMs = 3000;
constexpr unsigned long kLongPressTimeMs = 800;
constexpr unsigned long kDoubleTapDelayMs = 300;
constexpr unsigned long kTelemetryIntervalMs = 40000;
constexpr unsigned long kWeatherRefreshMs = 600000;
constexpr unsigned long kWeatherRequestTimeoutMs = 15000;
constexpr unsigned long kRestartDelayMs = 1500;
constexpr int kWifiConnectTimeoutMs = 15000;
constexpr size_t kRecentReqCacheSize = 12;
constexpr char kFirmwareVersion[] = "1.0.0";
// MQTT 默认配置仅用于开发兜底；量产时应优先从 factory_nvs 分区读取一机一码配置。
constexpr char kMqttBrokerHost[] = "43.153.134.2";
constexpr int kMqttBrokerPort = 1883;
constexpr char kMqttDeviceId[] = "deskbuddy-v1-000011";
constexpr char kMqttSecret[] = "REPLACE_WITH_DEVICE_SECRET";

constexpr EventBits_t kWifiConnectedBit = BIT0;
constexpr EventBits_t kWifiDisconnectedBit = BIT1;

enum Page : int {
    PAGE_EMO = 0,
    PAGE_CLOCK = 1,
    PAGE_WEATHER = 2,
    PAGE_WORLD_CLOCK = 3,
    PAGE_FORECAST = 4,
    PAGE_PIXEL_TEST = 5,
};

enum Mood : int {
    MOOD_NORMAL = 0,
    MOOD_HAPPY = 1,
    MOOD_SURPRISED = 2,
    MOOD_SLEEPY = 3,
    MOOD_ANGRY = 4,
    MOOD_SAD = 5,
    MOOD_EXCITED = 6,
    MOOD_LOVE = 7,
    MOOD_SUSPICIOUS = 8,
};

enum WeatherTheme : int {
    WEATHER_THEME_CLASSIC = 0,
    WEATHER_THEME_COMPACT = 1,
    WEATHER_THEME_CENTERED = 2,
    WEATHER_THEME_DASHBOARD = 3,
    WEATHER_THEME_MAGAZINE = 4,
};

struct ForecastDay {
    std::string date;
    std::string day_name;
    std::string weather_main;
    std::string weather_desc;
    float temp_min = 0.0F;
    float temp_max = 0.0F;
    int humidity = 0;
    bool valid = false;
};

struct DeviceConfig {
    std::string ssid;
    std::string pass;
    std::string city;
    std::string latitude;
    std::string longitude;
    std::string tz;
    std::string weather_theme = "classic";
};

struct WeatherData {
    float temperature = 0.0F;
    float feels_like = 0.0F;
    int humidity = 0;
    std::string weather_main = "LOADING";
    std::string weather_desc = "WAIT...";
    std::array<ForecastDay, 3> forecast = {};
    bool has_current = false;
    bool has_forecast = false;
    bool stale = false;
    std::string status_line = "WAIT...";
};

struct UiState {
    int current_page = PAGE_EMO;
    int pixel_test_pattern = 0;
    bool high_brightness = true;
    int brightness_percent = 100;
    bool weather_theme_picker_active = false;
    int weather_theme_preview = WEATHER_THEME_CLASSIC;
    int weather_theme_original = WEATHER_THEME_CLASSIC;
    int tap_counter = 0;
    unsigned long last_tap_time = 0;
    bool last_pin_state = false;
    unsigned long press_start_time = 0;
    bool long_press_handled = false;
    unsigned long last_page_switch = 0;
    bool in_config_mode = false;
};

struct Eye {
    float x = 0.0F;
    float y = 0.0F;
    float w = 0.0F;
    float h = 0.0F;
    float target_x = 0.0F;
    float target_y = 0.0F;
    float target_w = 0.0F;
    float target_h = 0.0F;
    float pupil_x = 0.0F;
    float pupil_y = 0.0F;
    float target_pupil_x = 0.0F;
    float target_pupil_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
    float vel_w = 0.0F;
    float vel_h = 0.0F;
    float pvel_x = 0.0F;
    float pvel_y = 0.0F;
    float k = 0.12F;
    float d = 0.60F;
    float pk = 0.08F;
    float pd = 0.50F;
    bool blinking = false;
    unsigned long last_blink = 0;
    unsigned long next_blink_time = 0;

    void init(float start_x, float start_y, float start_w, float start_h)
    {
        x = target_x = start_x;
        y = target_y = start_y;
        w = target_w = start_w;
        h = target_h = start_h;
        pupil_x = target_pupil_x = 0.0F;
        pupil_y = target_pupil_y = 0.0F;
        next_blink_time = millis() + static_cast<unsigned long>(random(1000, 4000));
    }

    void update()
    {
        const float ax = (target_x - x) * k;
        const float ay = (target_y - y) * k;
        const float aw = (target_w - w) * k;
        const float ah = (target_h - h) * k;

        vel_x = (vel_x + ax) * d;
        vel_y = (vel_y + ay) * d;
        vel_w = (vel_w + aw) * d;
        vel_h = (vel_h + ah) * d;

        x += vel_x;
        y += vel_y;
        w += vel_w;
        h += vel_h;

        const float pax = (target_pupil_x - pupil_x) * pk;
        const float pay = (target_pupil_y - pupil_y) * pk;
        pvel_x = (pvel_x + pax) * pd;
        pvel_y = (pvel_y + pay) * pd;
        pupil_x += pvel_x;
        pupil_y += pvel_y;
    }
};

struct EyeState {
    Eye left_eye = {};
    Eye right_eye = {};
    unsigned long last_saccade = 0;
    unsigned long saccade_interval = 3000;
    float breath_val = 0.0F;
    int current_mood = MOOD_NORMAL;
};

struct MqttRuntime {
    esp_mqtt_client_handle_t client = nullptr;
    bool connected = false;
    std::string broker_uri;
    std::string topic_cmd;
    std::string topic_resp;
    std::string topic_ack;
    std::string topic_telemetry;
    std::string topic_event;
    std::string topic_req;
    std::array<std::string, kRecentReqCacheSize> recent_req_ids = {};
    size_t recent_req_index = 0;
    bool weather_req_pending = false;
    std::string weather_req_id;
    unsigned long weather_req_sent_at = 0;
    int wifi_retry_count = 0;
    int last_wifi_disconnect_reason = 0;
};

extern DeviceConfig g_config;
extern WeatherData g_weather;
extern UiState g_ui;
extern EyeState g_eyes;
extern MqttRuntime g_mqtt;
extern EventGroupHandle_t g_wifi_event_group;
extern esp_event_handler_instance_t g_wifi_any_id;
extern esp_event_handler_instance_t g_ip_got_ip;
extern httpd_handle_t g_http_server;
extern esp_netif_t *g_sta_netif;
extern esp_netif_t *g_ap_netif;
extern std::atomic_bool g_wifi_connected;
extern std::atomic_bool g_mqtt_connected;
extern std::atomic_bool g_restart_requested;
extern std::atomic_ulong g_restart_requested_at;

const char *page_to_string(int page);
bool parse_page(const std::string &value, int *page);
const char *mood_to_string(int mood);
const char *weather_theme_to_string(int theme);
bool parse_weather_theme(const std::string &value, int *theme);
int normalize_weather_theme(int theme);
bool emotion_to_mood(const std::string &emotion, int *mood);
void apply_display_brightness(int brightness_percent);
