#include "app/app_types.h"
#include "display/oled_display.h"
#include "services/app_services.h"
#include "ui/ui_runtime.h"

extern "C" {
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

extern "C" void app_main(void)
{
    ESP_LOGI(kTag, "app_main start");
    init_nvs();
    init_touch_gpio();
    ESP_ERROR_CHECK(g_display.begin());
    ESP_LOGI(kTag, "oled initialized at address 0x%02X", kOledAddress);
    apply_display_brightness(g_ui.brightness_percent);
    init_wifi_stack();

    const bool force_config = should_force_config_mode();
    load_config();

    g_eyes.left_eye.init(18.0F, 14.0F, 36.0F, 36.0F);
    g_eyes.right_eye.init(74.0F, 14.0F, 36.0F, 36.0F);
    ESP_LOGI(kTag, "eye state initialized");

    if (force_config || g_config.ssid.empty()) {
        ESP_LOGW(kTag, "starting in config mode: force=%s ssid_missing=%s",
            force_config ? "true" : "false",
            g_config.ssid.empty() ? "true" : "false");
        start_config_portal(force_config ? "boot_hold" : "missing_wifi_config");
    } else {
        play_boot_animation();
        draw_connecting_screen();

        if (connect_station(g_config)) {
            start_mqtt();
            apply_timezone();
            sync_time();
            g_ui.last_page_switch = millis();
            ESP_LOGI(kTag, "normal runtime initialized");
        } else {
            ESP_LOGW(kTag, "wifi connect failed, falling back to config portal");
            start_config_portal("wifi_connect_failed");
        }
    }

    unsigned long last_weather_update = millis();
    unsigned long last_telemetry_publish = millis();
    if (g_ui.last_page_switch == 0) {
        g_ui.last_page_switch = millis();
    }
    ESP_LOGI(kTag, "entering main loop");

    while (true) {
        maybe_restart();

        if (g_ui.in_config_mode) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        handle_touch();
        const unsigned long now = millis();
        poll_weather_request_timeout();

        if (g_wifi_connected.load() && now - last_weather_update > kWeatherRefreshMs) {
            update_weather();
            last_weather_update = now;
        }

        if (g_mqtt_connected.load() && now - last_telemetry_publish > kTelemetryIntervalMs) {
            publish_telemetry();
            last_telemetry_publish = now;
        }

        render_page();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
