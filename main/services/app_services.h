#pragma once

#include <ctime>

#include "app/app_types.h"

extern "C" {
#include "esp_err.h"
}

void init_nvs();
void init_touch_gpio();
void init_wifi_stack();
void load_config();
esp_err_t save_config(const DeviceConfig &config);
void apply_timezone();
bool sync_time();
bool local_time_ready(struct tm *out_time);
bool connect_station(const DeviceConfig &config);
void start_mqtt();
void start_config_portal(const char *reason);
void update_weather();
void poll_weather_request_timeout();
void maybe_restart();
void publish_touch_event(const char *press_type, unsigned long duration_ms, int page_before, int page_after);
void publish_telemetry();
void publish_telemetry_if_connected();
void schedule_restart();
