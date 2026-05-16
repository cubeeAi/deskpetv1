#include "app/app_types.h"

#include "display/oled_display.h"

DeviceConfig g_config = {};
WeatherData g_weather = {};
UiState g_ui = {};
EyeState g_eyes = {};
MqttRuntime g_mqtt = {};
EventGroupHandle_t g_wifi_event_group = nullptr;
esp_event_handler_instance_t g_wifi_any_id = nullptr;
esp_event_handler_instance_t g_ip_got_ip = nullptr;
httpd_handle_t g_http_server = nullptr;
esp_netif_t *g_sta_netif = nullptr;
esp_netif_t *g_ap_netif = nullptr;
std::atomic_bool g_wifi_connected{false};
std::atomic_bool g_mqtt_connected{false};
std::atomic_bool g_restart_requested{false};
std::atomic_ulong g_restart_requested_at{0};


const char *page_to_string(int page)
{
    switch (page) {
    case PAGE_EMO: return "emo";
    case PAGE_CLOCK: return "clock";
    case PAGE_WEATHER: return "weather";
    case PAGE_WORLD_CLOCK: return "worldClock";
    case PAGE_FORECAST: return "forecast";
    case PAGE_PIXEL_TEST: return "pixelTest";
    default: return "emo";
    }
}

bool parse_page(const std::string &value, int *page)
{
    if (page == nullptr) {
        return false;
    }
    if (value == "emo") {
        *page = PAGE_EMO;
    } else if (value == "clock") {
        *page = PAGE_CLOCK;
    } else if (value == "weather") {
        *page = PAGE_WEATHER;
    } else if (value == "worldClock") {
        *page = PAGE_WORLD_CLOCK;
    } else if (value == "forecast") {
        *page = PAGE_FORECAST;
    } else {
        return false;
    }
    return true;
}

const char *mood_to_string(int mood)
{
    switch (mood) {
    case MOOD_NORMAL: return "normal";
    case MOOD_HAPPY: return "happy";
    case MOOD_SURPRISED: return "surprised";
    case MOOD_SLEEPY: return "sleepy";
    case MOOD_ANGRY: return "angry";
    case MOOD_SAD: return "sad";
    case MOOD_EXCITED: return "excited";
    case MOOD_LOVE: return "love";
    case MOOD_SUSPICIOUS: return "suspicious";
    default: return "normal";
    }
}

const char *weather_theme_to_string(int theme)
{
    switch (normalize_weather_theme(theme)) {
    case WEATHER_THEME_CLASSIC: return "classic";
    case WEATHER_THEME_COMPACT: return "compact";
    case WEATHER_THEME_CENTERED: return "centered";
    case WEATHER_THEME_DASHBOARD: return "dashboard";
    case WEATHER_THEME_MAGAZINE: return "magazine";
    default: return "classic";
    }
}

bool parse_weather_theme(const std::string &value, int *theme)
{
    if (theme == nullptr) {
        return false;
    }
    if (value == "classic") {
        *theme = WEATHER_THEME_CLASSIC;
    } else if (value == "compact") {
        *theme = WEATHER_THEME_COMPACT;
    } else if (value == "centered") {
        *theme = WEATHER_THEME_CENTERED;
    } else if (value == "dashboard") {
        *theme = WEATHER_THEME_DASHBOARD;
    } else if (value == "magazine") {
        *theme = WEATHER_THEME_MAGAZINE;
    } else {
        return false;
    }
    return true;
}

int normalize_weather_theme(int theme)
{
    if (theme < WEATHER_THEME_CLASSIC || theme > WEATHER_THEME_MAGAZINE) {
        return WEATHER_THEME_CLASSIC;
    }
    return theme;
}

bool emotion_to_mood(const std::string &emotion, int *mood)
{
    if (mood == nullptr) {
        return false;
    }
    if (emotion == "idle") {
        *mood = MOOD_NORMAL;
    } else if (emotion == "happy") {
        *mood = MOOD_HAPPY;
    } else if (emotion == "sad") {
        *mood = MOOD_SAD;
    } else if (emotion == "angry") {
        *mood = MOOD_ANGRY;
    } else if (emotion == "sleepy") {
        *mood = MOOD_SLEEPY;
    } else if (emotion == "excited") {
        *mood = MOOD_EXCITED;
    } else if (emotion == "confused") {
        *mood = MOOD_SUSPICIOUS;
    } else {
        return false;
    }
    return true;
}

void apply_display_brightness(int brightness_percent)
{
    const int clamped = std::max(0, std::min(100, brightness_percent));
    g_ui.brightness_percent = clamped;
    g_ui.high_brightness = clamped >= 50;
    const int contrast = std::max(1, (clamped * 255) / 100);
    g_display.set_contrast(static_cast<uint8_t>(contrast));
}
