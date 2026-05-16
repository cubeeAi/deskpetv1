#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>

#include "ui/ui_runtime.h"

#include "services/app_services.h"
#include "app/app_types.h"
#include "display/display_assets.h"
#include "display/oled_display.h"
#include "display/text_render.h"

extern "C" {
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

void draw_twinkle_star(int16_t x, int16_t y)
{
    g_display.draw_pixel(x, y, 1);
    g_display.draw_pixel(static_cast<int16_t>(x - 1), y, 1);
    g_display.draw_pixel(static_cast<int16_t>(x + 1), y, 1);
    g_display.draw_pixel(x, static_cast<int16_t>(y - 1), 1);
    g_display.draw_pixel(x, static_cast<int16_t>(y + 1), 1);
}

void draw_weather_astronaut(int16_t x, int16_t y)
{
    const unsigned long now = millis();
    const int16_t bob = static_cast<int16_t>(std::lround(std::sin(static_cast<double>(now) / 350.0) * 2.0));
    const bool wave_pose = ((now / 420U) % 2U) == 0U;
    const bool star_left = ((now / 260U) % 2U) == 0U;
    const bool star_right = ((now / 340U) % 2U) == 0U;

    const int16_t px = x;
    const int16_t py = static_cast<int16_t>(y + bob);

    if (star_left) {
        draw_twinkle_star(static_cast<int16_t>(px - 4), static_cast<int16_t>(py + 4));
    }
    if (star_right) {
        draw_twinkle_star(static_cast<int16_t>(px + 19), static_cast<int16_t>(py + 8));
    }

    // Helmet
    g_display.fill_round_rect(static_cast<int16_t>(px + 4), py, 10, 10, 4, 1);
    g_display.fill_round_rect(static_cast<int16_t>(px + 6), static_cast<int16_t>(py + 2), 6, 4, 2, 0);
    g_display.draw_pixel(static_cast<int16_t>(px + 11), static_cast<int16_t>(py + 3), 1);

    // Body and backpack
    g_display.fill_round_rect(static_cast<int16_t>(px + 5), static_cast<int16_t>(py + 10), 8, 9, 2, 1);
    g_display.fill_rect(static_cast<int16_t>(px + 13), static_cast<int16_t>(py + 11), 2, 6, 1);

    // Arms
    g_display.fill_rect(static_cast<int16_t>(px + 2), static_cast<int16_t>(py + 12), 3, 2, 1);
    if (wave_pose) {
        g_display.fill_rect(static_cast<int16_t>(px + 12), static_cast<int16_t>(py + 8), 2, 5, 1);
        g_display.fill_rect(static_cast<int16_t>(px + 13), static_cast<int16_t>(py + 6), 2, 2, 1);
    } else {
        g_display.fill_rect(static_cast<int16_t>(px + 12), static_cast<int16_t>(py + 12), 3, 2, 1);
    }

    // Legs and boots
    g_display.fill_rect(static_cast<int16_t>(px + 7), static_cast<int16_t>(py + 19), 2, 4, 1);
    g_display.fill_rect(static_cast<int16_t>(px + 10), static_cast<int16_t>(py + 19), 2, 4, 1);
    g_display.fill_rect(static_cast<int16_t>(px + 6), static_cast<int16_t>(py + 23), 3, 2, 1);
    g_display.fill_rect(static_cast<int16_t>(px + 9), static_cast<int16_t>(py + 23), 3, 2, 1);

    // Small tether for motion feel
    g_display.draw_line(static_cast<int16_t>(px + 15), static_cast<int16_t>(py + 13), static_cast<int16_t>(px + 19), static_cast<int16_t>(py + 11), 1);
}

void draw_eyelid_mask(float x, float y, float w, float h, int mood, bool is_left)
{
    const int ix = static_cast<int>(x);
    const int iy = static_cast<int>(y);
    const int iw = static_cast<int>(w);
    const int ih = static_cast<int>(h);

    if (mood == MOOD_ANGRY) {
        if (is_left) {
            for (int i = 0; i < 16; ++i) g_display.draw_line(ix, iy + i, ix + iw, iy - 6 + i, 0);
        } else {
            for (int i = 0; i < 16; ++i) g_display.draw_line(ix, iy - 6 + i, ix + iw, iy + i, 0);
        }
    } else if (mood == MOOD_SAD) {
        if (is_left) {
            for (int i = 0; i < 16; ++i) g_display.draw_line(ix, iy - 6 + i, ix + iw, iy + i, 0);
        } else {
            for (int i = 0; i < 16; ++i) g_display.draw_line(ix, iy + i, ix + iw, iy - 6 + i, 0);
        }
    } else if (mood == MOOD_HAPPY || mood == MOOD_LOVE || mood == MOOD_EXCITED) {
        g_display.fill_rect(ix, static_cast<int16_t>(iy + ih - 12), iw, 14, 0);
        g_display.fill_circle(static_cast<int16_t>(ix + iw / 2), static_cast<int16_t>(iy + ih + 6), static_cast<int16_t>(iw / 1.3F), 0);
    } else if (mood == MOOD_SLEEPY) {
        g_display.fill_rect(ix, iy, iw, static_cast<int16_t>(ih / 2 + 2), 0);
    } else if (mood == MOOD_SUSPICIOUS) {
        if (is_left) {
            g_display.fill_rect(ix, iy, iw, static_cast<int16_t>(ih / 2 - 2), 0);
        } else {
            g_display.fill_rect(ix, static_cast<int16_t>(iy + ih - 8), iw, 8, 0);
        }
    }
}

void draw_ultra_pro_eye(Eye &eye, bool is_left)
{
    const int ix = static_cast<int>(eye.x);
    const int iy = static_cast<int>(eye.y);
    const int iw = static_cast<int>(eye.w);
    const int ih = static_cast<int>(eye.h);

    int radius = 8;
    if (iw < 20) {
        radius = 3;
    }
    g_display.fill_round_rect(ix, iy, iw, ih, radius, 1);

    const int cx = ix + iw / 2;
    const int cy = iy + ih / 2;
    const int pw = std::max(4, static_cast<int>(iw / 2.2F));
    const int ph = std::max(4, static_cast<int>(ih / 2.2F));

    int px = cx + static_cast<int>(eye.pupil_x) - (pw / 2);
    int py = cy + static_cast<int>(eye.pupil_y) - (ph / 2);

    px = std::max(ix, std::min(px, ix + iw - pw));
    py = std::max(iy, std::min(py, iy + ih - ph));

    g_display.fill_round_rect(px, py, pw, ph, std::max(1, radius / 2), 0);
    if (iw > 15 && ih > 15) {
        g_display.fill_circle(static_cast<int16_t>(px + pw - 4), static_cast<int16_t>(py + 4), 2, 1);
    }

    draw_eyelid_mask(eye.x, eye.y, eye.w, eye.h, g_eyes.current_mood, is_left);
}

void update_physics_and_mood()
{
    const unsigned long now = millis();
    g_eyes.breath_val = std::sin(static_cast<float>(now) / 800.0F) * 1.5F;

    if (now > g_eyes.left_eye.next_blink_time) {
        g_eyes.left_eye.blinking = true;
        g_eyes.right_eye.blinking = true;
        g_eyes.left_eye.last_blink = now;
        g_eyes.right_eye.last_blink = now;
        g_eyes.left_eye.next_blink_time = now + static_cast<unsigned long>(random(2000, 6000));
    }

    if (g_eyes.left_eye.blinking) {
        g_eyes.left_eye.target_h = 2;
        g_eyes.right_eye.target_h = 2;
        if (now - g_eyes.left_eye.last_blink > 120) {
            g_eyes.left_eye.blinking = false;
            g_eyes.right_eye.blinking = false;
        }
    }

    if (!g_eyes.left_eye.blinking && now - g_eyes.last_saccade > g_eyes.saccade_interval) {
        g_eyes.last_saccade = now;
        g_eyes.saccade_interval = static_cast<unsigned long>(random(500, 3000));

        int dir = static_cast<int>(random(0, 10));
        float lx = 0.0F;
        float ly = 0.0F;

        if (dir == 4) {
            lx = -6.0F;
            ly = -4.0F;
        } else if (dir == 5) {
            lx = 6.0F;
            ly = -4.0F;
        } else if (dir == 6) {
            lx = -6.0F;
            ly = 4.0F;
        } else if (dir == 7) {
            lx = 6.0F;
            ly = 4.0F;
        } else if (dir == 8) {
            lx = 8.0F;
        } else if (dir == 9) {
            lx = -8.0F;
        }

        g_eyes.left_eye.target_pupil_x = lx;
        g_eyes.left_eye.target_pupil_y = ly;
        g_eyes.right_eye.target_pupil_x = lx;
        g_eyes.right_eye.target_pupil_y = ly;

        g_eyes.left_eye.target_x = 18.0F + (lx * 0.3F);
        g_eyes.left_eye.target_y = 14.0F + (ly * 0.3F);
        g_eyes.right_eye.target_x = 74.0F + (lx * 0.3F);
        g_eyes.right_eye.target_y = 14.0F + (ly * 0.3F);
    }

    if (!g_eyes.left_eye.blinking) {
        float base_w = 36.0F;
        float base_h = 36.0F + g_eyes.breath_val;

        switch (g_eyes.current_mood) {
        case MOOD_HAPPY:
        case MOOD_LOVE:
            g_eyes.left_eye.target_w = 40.0F;
            g_eyes.left_eye.target_h = 32.0F;
            g_eyes.right_eye.target_w = 40.0F;
            g_eyes.right_eye.target_h = 32.0F;
            break;
        case MOOD_SURPRISED:
            g_eyes.left_eye.target_w = 30.0F;
            g_eyes.left_eye.target_h = 45.0F;
            g_eyes.right_eye.target_w = 30.0F;
            g_eyes.right_eye.target_h = 45.0F;
            g_eyes.left_eye.target_pupil_x += static_cast<float>(random(-1, 2));
            break;
        case MOOD_SLEEPY:
            g_eyes.left_eye.target_w = 38.0F;
            g_eyes.left_eye.target_h = 30.0F;
            g_eyes.right_eye.target_w = 38.0F;
            g_eyes.right_eye.target_h = 30.0F;
            break;
        case MOOD_ANGRY:
            g_eyes.left_eye.target_w = 34.0F;
            g_eyes.left_eye.target_h = 32.0F;
            g_eyes.right_eye.target_w = 34.0F;
            g_eyes.right_eye.target_h = 32.0F;
            break;
        case MOOD_SAD:
            g_eyes.left_eye.target_w = 34.0F;
            g_eyes.left_eye.target_h = 40.0F;
            g_eyes.right_eye.target_w = 34.0F;
            g_eyes.right_eye.target_h = 40.0F;
            break;
        case MOOD_SUSPICIOUS:
            g_eyes.left_eye.target_w = 36.0F;
            g_eyes.left_eye.target_h = 20.0F;
            g_eyes.right_eye.target_w = 36.0F;
            g_eyes.right_eye.target_h = 42.0F;
            break;
        default:
            g_eyes.left_eye.target_w = base_w;
            g_eyes.left_eye.target_h = base_h;
            g_eyes.right_eye.target_w = base_w;
            g_eyes.right_eye.target_h = base_h;
            break;
        }
    }

    g_eyes.left_eye.update();
    g_eyes.right_eye.update();
}

void draw_emo_page()
{
    update_physics_and_mood();

    if (g_eyes.current_mood == MOOD_LOVE) {
        g_display.draw_bitmap(56, 0, bmp_heart, 16, 16, 1);
    } else if (g_eyes.current_mood == MOOD_SLEEPY) {
        g_display.draw_bitmap(110, 0, bmp_zzz, 16, 16, 1);
    } else if (g_eyes.current_mood == MOOD_ANGRY) {
        g_display.draw_bitmap(56, 0, bmp_anger, 16, 16, 1);
    }

    draw_ultra_pro_eye(g_eyes.left_eye, true);
    draw_ultra_pro_eye(g_eyes.right_eye, false);
}

void draw_forecast_page()
{
    g_display.fill_rect(0, 0, 128, 16, 1);
    draw_tiny_text(28, 4, "三日预报", 0, 1);
    g_display.draw_line(42, 16, 42, 63, 1);
    g_display.draw_line(85, 16, 85, 63, 1);

    for (size_t i = 0; i < g_weather.forecast.size(); ++i) {
        const int x_start = static_cast<int>(i) * 43;
        const int center_x = x_start + 21;
        const ForecastDay &forecast = g_weather.forecast[i];
        const std::string label = forecast.valid ? forecast.day_name : "待定";
        draw_tiny_text(static_cast<int16_t>(center_x - (tiny_text_width(label, 1) / 2)), 20, label, 1, 1);
        const std::string icon = forecast.valid ? forecast.weather_main : "Clouds";
        g_display.draw_bitmap(static_cast<int16_t>(center_x - 8), 28, get_mini_icon(icon), 16, 16, 1);

        const std::string temp = forecast.valid ? std::to_string(static_cast<int>(std::lround(forecast.temp_max))) : "--";
        draw_tiny_text(static_cast<int16_t>(center_x - (tiny_text_width(temp, 2) / 2)), 49, temp, 1, 2);
        g_display.fill_circle(static_cast<int16_t>(center_x + (tiny_text_width(temp, 2) / 2) + 2), 50, 2, 1);
    }
}

void draw_clock_page()
{
    struct tm time_info = {};
    if (!local_time_ready(&time_info)) {
        draw_tiny_text(28, 28, "同步中", 1, 2);
        return;
    }

    const std::string am_pm = time_info.tm_hour >= 12 ? "PM" : "AM";
    int hour12 = time_info.tm_hour % 12;
    if (hour12 == 0) {
        hour12 = 12;
    }

    char time_text[16] = {};
    std::snprintf(time_text, sizeof(time_text), "%02d:%02d", hour12, time_info.tm_min);
    char date_text[20] = {};
    std::strftime(date_text, sizeof(date_text), "%a, %b %d", &time_info);

    draw_tiny_text(110, 0, am_pm, 1, 1);
    const std::string time_str = time_text;
    draw_big_text(static_cast<int16_t>((kScreenWidth - big_text_width(time_str)) / 2), 16, time_str);
    const std::string date_str = to_display_text(date_text);
    draw_tiny_text(static_cast<int16_t>((kScreenWidth - tiny_text_width(date_str, 1)) / 2), 54, date_str, 1, 1);
}

struct WeatherPageView {
    std::string title;
    std::string subtitle;
    std::string temp;
    std::string feels;
    std::string humidity;
    int humidity_value = 0;
};

int active_weather_theme()
{
    int theme = WEATHER_THEME_CLASSIC;
    if (g_ui.weather_theme_picker_active) {
        return normalize_weather_theme(g_ui.weather_theme_preview);
    }
    if (parse_weather_theme(g_config.weather_theme, &theme)) {
        return normalize_weather_theme(theme);
    }
    return WEATHER_THEME_CLASSIC;
}

WeatherPageView build_weather_page_view()
{
    WeatherPageView view = {};
    view.title = truncate_text(to_display_text(g_weather.weather_main), 6);
    view.subtitle = truncate_text(to_display_text(g_weather.weather_desc), 6);
    view.temp = std::to_string(static_cast<int>(std::lround(g_weather.temperature)));
    view.feels = std::to_string(static_cast<int>(std::lround(g_weather.feels_like))) + "C";
    view.humidity = std::to_string(g_weather.humidity) + "%";
    view.humidity_value = std::max(0, std::min(100, g_weather.humidity));
    if (view.title.empty()) {
        view.title = "Clear";
    }
    if (view.subtitle.empty()) {
        view.subtitle = "--";
    }
    return view;
}

bool has_weather_location_config()
{
    return !g_config.latitude.empty() && !g_config.longitude.empty() && !g_config.tz.empty();
}

void draw_stale_badge()
{
    draw_tiny_text(92, 54, "旧数据", 1, 1);
}

void draw_meter(int16_t x, int16_t y, int16_t w, int16_t h, int value)
{
    const int clamped = std::max(0, std::min(100, value));
    g_display.fill_round_rect(x, y, w, h, static_cast<int16_t>(h / 2), 1);
    if (w > 2 && h > 2) {
        g_display.fill_round_rect(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1), static_cast<int16_t>(w - 2), static_cast<int16_t>(h - 2), static_cast<int16_t>((h - 2) / 2), 0);
        const int fill_width = ((w - 2) * clamped) / 100;
        if (fill_width > 0) {
            g_display.fill_round_rect(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1), static_cast<int16_t>(fill_width), static_cast<int16_t>(h - 2), static_cast<int16_t>((h - 2) / 2), 1);
        }
    }
}

void draw_vertical_meter(int16_t x, int16_t y, int16_t w, int16_t h, int value)
{
    const int clamped = std::max(0, std::min(100, value));
    g_display.fill_round_rect(x, y, w, h, static_cast<int16_t>(w / 2), 1);
    if (w > 2 && h > 2) {
        g_display.fill_round_rect(
            static_cast<int16_t>(x + 1),
            static_cast<int16_t>(y + 1),
            static_cast<int16_t>(w - 2),
            static_cast<int16_t>(h - 2),
            static_cast<int16_t>((w - 2) / 2),
            0);
        const int fill_height = ((h - 2) * clamped) / 100;
        if (fill_height > 0) {
            g_display.fill_round_rect(
                static_cast<int16_t>(x + 1),
                static_cast<int16_t>(y + h - 1 - fill_height),
                static_cast<int16_t>(w - 2),
                static_cast<int16_t>(fill_height),
                static_cast<int16_t>((w - 2) / 2),
                1);
        }
    }
}

void draw_temperature_with_degree(int16_t x, int16_t y, const std::string &temp, bool compact)
{
    if (compact) {
        draw_tiny_text(x, y, temp, 1, 2);
        g_display.fill_circle(static_cast<int16_t>(x + tiny_text_width(temp, 2) + 3), static_cast<int16_t>(y + 3), 2, 1);
    } else {
        draw_big_text(x, y, temp);
        g_display.fill_circle(static_cast<int16_t>(x + big_text_width(temp) + 4), static_cast<int16_t>(y + 4), 3, 1);
    }
}

int16_t big_temperature_span(const std::string &temp)
{
    return static_cast<int16_t>(big_text_width(temp) + 8);
}

int16_t compact_temperature_span(const std::string &temp)
{
    return static_cast<int16_t>(tiny_text_width(temp, 2) + 6);
}

void draw_metric_pair(int16_t x, int16_t y, const std::string &label, const std::string &value)
{
    draw_tiny_text(x, y, label, 1, 1);
    draw_tiny_text(static_cast<int16_t>(x + 28), y, value, 1, 1);
}

std::string weather_city_label()
{
    const std::string city = to_display_text(g_config.city);
    if (city.empty()) {
        return "--";
    }
    return truncate_text(city, 6);
}

void draw_city_chip(const std::string &city)
{
    const int text_width = tiny_text_width(city, 1);
    const int chip_width = std::min(44, std::max(22, text_width + 10));
    const int chip_x = 128 - chip_width - 6;
    g_display.fill_round_rect(static_cast<int16_t>(chip_x), 2, static_cast<int16_t>(chip_width), 13, 4, 1);
    draw_tiny_text(static_cast<int16_t>(chip_x + ((chip_width - text_width) / 2)), 5, city, 0, 1);
}

int16_t classic_temperature_span(const std::string &temp)
{
    return static_cast<int16_t>(tiny_text_width(temp, 3) + 6);
}

void draw_classic_temperature(int16_t x, int16_t y, const std::string &temp)
{
    draw_tiny_text(x, y, temp, 1, 3);
    g_display.fill_circle(static_cast<int16_t>(x + tiny_text_width(temp, 3) + 3), static_cast<int16_t>(y + 4), 2, 1);
}

int16_t compact_panel_temperature_span(const std::string &temp)
{
    return static_cast<int16_t>(tiny_text_width(temp, 2) + 6);
}

void draw_compact_panel_temperature(int16_t x, int16_t y, const std::string &temp)
{
    draw_tiny_text(x, y, temp, 1, 2);
    g_display.fill_circle(static_cast<int16_t>(x + tiny_text_width(temp, 2) + 3), static_cast<int16_t>(y + 3), 2, 1);
}

void draw_weather_page_classic(const WeatherPageView &view)
{
    const std::string city = weather_city_label();
    const int16_t temp_span = classic_temperature_span(view.temp);
    const int16_t temp_x = std::max<int16_t>(78, static_cast<int16_t>(128 - temp_span - 8));

    g_display.draw_bitmap(8, 9, get_big_icon(g_weather.weather_main), 32, 32, 1);
    draw_city_chip(city);
    draw_classic_temperature(temp_x, 18, view.temp);

    g_display.draw_line(8, 45, 119, 45, 1);

    draw_tiny_text(8, 50, "湿度", 1, 1);
    draw_meter(38, 52, 30, 5, view.humidity_value);

    const std::string feels_label = "体感";
    const std::string feels_text = feels_label + " " + view.feels;
    const int feels_width = tiny_text_width(feels_text, 1);
    draw_tiny_text(static_cast<int16_t>(120 - feels_width), 50, feels_text, 1, 1);
}

void draw_weather_page_compact(const WeatherPageView &view)
{
    const std::string city = weather_city_label();
    const std::string header = "- " + city + " -";
    const int header_width = tiny_text_width(header, 1);
    const int16_t divider_x = 63;
    const int16_t divider_y = 38;
    const int16_t temp_span = compact_panel_temperature_span(view.temp);
    const int16_t temp_x = std::max<int16_t>(78, static_cast<int16_t>(123 - temp_span));

    g_display.fill_rect(0, 0, 128, 14, 1);
    draw_tiny_text(static_cast<int16_t>((128 - header_width) / 2), 3, header, 0, 1);

    g_display.draw_line(0, 14, 127, 14, 1);
    g_display.draw_line(divider_x, 14, divider_x, 63, 1);
    g_display.draw_line(0, divider_y, 127, divider_y, 1);

    g_display.draw_bitmap(24, 18, get_mini_icon(g_weather.weather_main), 16, 16, 1);
    draw_compact_panel_temperature(temp_x, 20, view.temp);

    draw_tiny_text(10, 48, "湿度:", 1, 1);
    draw_tiny_text(40, 48, view.humidity, 1, 1);

    const std::string feels_text = "体感:" + view.feels;
    draw_tiny_text(72, 48, feels_text, 1, 1);
}

void draw_weather_page_centered(const WeatherPageView &view)
{
    const std::string city = weather_city_label();
    const int city_width = tiny_text_width(city, 1);
    const int16_t city_x = static_cast<int16_t>((kScreenWidth - city_width) / 2);
    const int underline_width = city_width + 12;
    const int16_t underline_x = static_cast<int16_t>((kScreenWidth - underline_width) / 2);
    const int16_t temp_span = classic_temperature_span(view.temp);
    const int16_t temp_x = static_cast<int16_t>((128 - temp_span) / 2);
    const int feels_value_width = tiny_text_width(view.feels, 1);

    draw_tiny_text(city_x, 2, city, 1, 1);
    g_display.draw_line(underline_x, 13, static_cast<int16_t>(underline_x + underline_width), 13, 1);

    g_display.draw_bitmap(56, 16, get_mini_icon(g_weather.weather_main), 16, 16, 1);
    draw_classic_temperature(temp_x, 32, view.temp);

    draw_tiny_text(4, 39, "湿度", 1, 1);
    draw_tiny_text(4, 54, view.humidity, 1, 1);

    draw_tiny_text(96, 39, "体感", 1, 1);
    draw_tiny_text(static_cast<int16_t>(124 - feels_value_width), 54, view.feels, 1, 1);
}

void draw_weather_page_dashboard(const WeatherPageView &view)
{
    const std::string city = weather_city_label();
    const int humidity_width = tiny_text_width(view.humidity, 1);
    const std::string feels_text = "体感 " + view.feels;
    const int feels_width = tiny_text_width(feels_text, 1);

    draw_tiny_text(4, 4, city, 1, 1);
    g_display.draw_bitmap(102, 6, get_mini_icon(g_weather.weather_main), 16, 16, 1);
    draw_compact_panel_temperature(4, 20, view.temp);

    draw_tiny_text(static_cast<int16_t>(124 - feels_width), 28, feels_text, 1, 1);

    g_display.draw_line(4, 44, 123, 44, 1);

    draw_tiny_text(4, 50, "湿度", 1, 1);
    draw_meter(34, 52, 48, 5, view.humidity_value);
    draw_tiny_text(static_cast<int16_t>(124 - humidity_width), 50, view.humidity, 1, 1);
}

void draw_weather_page_magazine(const WeatherPageView &view)
{
    const std::string city = weather_city_label();
    const std::string feels_text = "体感:" + view.feels;
    const std::string humidity_text = "湿度:" + view.humidity;
    const int feels_width = tiny_text_width(feels_text, 1);
    const int humidity_width = tiny_text_width(humidity_text, 1);

    g_display.fill_rect(0, 0, 48, 64, 1);
    draw_tiny_text(4, 50, city, 0, 1);
    g_display.draw_bitmap(8, 12, get_big_icon(g_weather.weather_main), 32, 32, 0);

    draw_classic_temperature(64, 6, view.temp);
    g_display.draw_line(56, 34, 120, 34, 1);
    draw_tiny_text(static_cast<int16_t>(124 - feels_width), 36, feels_text, 1, 1);
    draw_tiny_text(static_cast<int16_t>(124 - humidity_width), 53, humidity_text, 1, 1);
}

void draw_weather_picker_overlay(int theme)
{
    g_display.fill_rect(0, 52, 128, 12, 0);
    g_display.draw_line(0, 52, 127, 52, 1);
    (void)theme;
    draw_tiny_text(32, 54, "短+双-长存", 1, 1);
}

void draw_weather_page()
{
    if (!has_weather_location_config()) {
        draw_tiny_text(10, 16, "请配天气位置", 1, 1);
        draw_tiny_text(10, 40, "请到配置页设置", 1, 1);
        return;
    }
    if (!g_weather.has_current && !g_wifi_connected.load()) {
        draw_tiny_text(22, 24, "未连接WiFi", 1, 1);
        return;
    }
    if (!g_weather.has_current) {
        if (g_mqtt.weather_req_pending) {
            draw_tiny_text(18, 24, "天气加载中", 1, 1);
            return;
        }
        draw_tiny_text(18, 24, "天气加载中", 1, 1);
        return;
    }

    const WeatherPageView view = build_weather_page_view();
    const int theme = active_weather_theme();
    switch (theme) {
    case WEATHER_THEME_COMPACT:
        draw_weather_page_compact(view);
        break;
    case WEATHER_THEME_CENTERED:
        draw_weather_page_centered(view);
        break;
    case WEATHER_THEME_DASHBOARD:
        draw_weather_page_dashboard(view);
        break;
    case WEATHER_THEME_MAGAZINE:
        draw_weather_page_magazine(view);
        break;
    case WEATHER_THEME_CLASSIC:
    default:
        draw_weather_page_classic(view);
        break;
    }

    if (g_weather.stale &&
        theme != WEATHER_THEME_CLASSIC &&
        theme != WEATHER_THEME_COMPACT &&
        theme != WEATHER_THEME_CENTERED &&
        theme != WEATHER_THEME_DASHBOARD &&
        theme != WEATHER_THEME_MAGAZINE) {
        draw_stale_badge();
    }

    if (g_ui.weather_theme_picker_active) {
        draw_weather_picker_overlay(theme);
    }
}

void draw_world_clock_page()
{
    g_display.fill_rect(0, 0, 128, 16, 1);
    draw_tiny_text(30, 4, "世界时钟", 0, 1);
    g_display.draw_line(64, 18, 64, 54, 1);

    std::time_t now = 0;
    std::time(&now);
    const std::time_t india_epoch = now + (5 * 3600) + (30 * 60);
    const std::time_t sydney_epoch = now + (11 * 3600);

    struct tm india_tm = {};
    struct tm sydney_tm = {};
    gmtime_r(&india_epoch, &india_tm);
    gmtime_r(&sydney_epoch, &sydney_tm);

    char india_text[16] = {};
    char sydney_text[16] = {};
    std::snprintf(india_text, sizeof(india_text), "%02d:%02d", india_tm.tm_hour, india_tm.tm_min);
    std::snprintf(sydney_text, sizeof(sydney_text), "%02d:%02d", sydney_tm.tm_hour, sydney_tm.tm_min);

    draw_tiny_text(18, 22, "印度", 1, 1);
    draw_tiny_text(5, 37, india_text, 1, 2);
    draw_tiny_text(82, 22, "悉尼", 1, 1);
    draw_tiny_text(69, 37, sydney_text, 1, 2);
}

void draw_pixel_test_page()
{
    switch (g_ui.pixel_test_pattern) {
    case 0:
        g_display.fill_rect(0, 0, kScreenWidth, kScreenHeight, 1);
        break;
    case 1:
        break;
    case 2:
        for (int16_t x = 0; x < kScreenWidth; x += 2) {
            g_display.fill_rect(x, 0, 1, kScreenHeight, 1);
        }
        break;
    case 3:
        for (int16_t y = 0; y < kScreenHeight; y += 2) {
            g_display.fill_rect(0, y, kScreenWidth, 1, 1);
        }
        break;
    case 4:
        for (int16_t y = 0; y < kScreenHeight; ++y) {
            for (int16_t x = 0; x < kScreenWidth; ++x) {
                if (((x + y) & 0x01) == 0) {
                    g_display.draw_pixel(x, y, 1);
                }
            }
        }
        break;
    case 5:
    default:
        g_display.draw_line(0, 0, kScreenWidth - 1, 0, 1);
        g_display.draw_line(0, kScreenHeight - 1, kScreenWidth - 1, kScreenHeight - 1, 1);
        g_display.draw_line(0, 0, 0, kScreenHeight - 1, 1);
        g_display.draw_line(kScreenWidth - 1, 0, kScreenWidth - 1, kScreenHeight - 1, 1);
        g_display.draw_line(kScreenWidth / 2, 0, kScreenWidth / 2, kScreenHeight - 1, 1);
        g_display.draw_line(0, kScreenHeight / 2, kScreenWidth - 1, kScreenHeight / 2, 1);
        break;
    }
}

void play_boot_animation()
{
    ESP_LOGI(kTag, "playing boot animation");
    constexpr int cx = 64;
    constexpr int cy = 32;
    for (int radius = 0; radius < 80; radius += 4) {
        g_display.clear_display();
        g_display.fill_circle(cx, cy, radius, 1);
        g_display.display();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    for (int radius = 0; radius < 80; radius += 4) {
        g_display.clear_display();
        g_display.fill_circle(cx, cy, 80, 1);
        g_display.fill_circle(cx, cy, radius, 0);
        g_display.display();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    const std::string text = "Cubee";
    g_display.clear_display();
    draw_tiny_text(static_cast<int16_t>((kScreenWidth - tiny_text_width(text, 2)) / 2), 24, text, 1, 2);
    g_display.display();
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void draw_connecting_screen()
{
    ESP_LOGI(kTag, "drawing connecting screen");
    g_display.clear_display();
    draw_tiny_text(20, 24, "连接中...", 1, 2);
    g_display.display();
}

int cycle_weather_theme_value(int theme, int step)
{
    const int min_theme = WEATHER_THEME_CLASSIC;
    const int max_theme = WEATHER_THEME_MAGAZINE;
    const int theme_count = max_theme - min_theme + 1;
    int next = normalize_weather_theme(theme);
    next = ((next - min_theme + step) % theme_count + theme_count) % theme_count;
    return next + min_theme;
}

void enter_weather_theme_picker()
{
    const int theme = active_weather_theme();
    g_ui.weather_theme_picker_active = true;
    g_ui.weather_theme_original = theme;
    g_ui.weather_theme_preview = theme;
}

void step_weather_theme_preview(int step)
{
    g_ui.weather_theme_preview = cycle_weather_theme_value(g_ui.weather_theme_preview, step);
}

void save_weather_theme_selection()
{
    DeviceConfig next = g_config;
    next.weather_theme = weather_theme_to_string(g_ui.weather_theme_preview);
    const esp_err_t err = save_config(next);
    if (err == ESP_OK) {
        g_config = next;
        g_ui.weather_theme_original = g_ui.weather_theme_preview;
        publish_telemetry_if_connected();
    } else {
        ESP_LOGE(kTag, "failed to save weather theme: %s", esp_err_to_name(err));
        g_ui.weather_theme_preview = g_ui.weather_theme_original;
    }
    g_ui.weather_theme_picker_active = false;
}

void handle_touch()
{
    const bool pin_state = gpio_get_level(kTouchPin) != 0;
    const unsigned long now = millis();
    const int page_before = g_ui.current_page;

    if (pin_state && !g_ui.last_pin_state) {
        g_ui.press_start_time = now;
        g_ui.long_press_handled = false;
    } else if (pin_state && g_ui.last_pin_state) {
        if ((now - g_ui.press_start_time > kLongPressTimeMs) && !g_ui.long_press_handled) {
            g_ui.last_page_switch = now;
            if (g_ui.weather_theme_picker_active) {
                save_weather_theme_selection();
            } else if (g_ui.current_page == PAGE_EMO) {
                g_eyes.current_mood += 1;
                if (g_eyes.current_mood > MOOD_SUSPICIOUS) {
                    g_eyes.current_mood = MOOD_NORMAL;
                }
                g_eyes.last_saccade = 0;
            } else if (g_ui.current_page == PAGE_CLOCK) {
                g_ui.current_page = PAGE_WORLD_CLOCK;
            } else if (g_ui.current_page == PAGE_WEATHER) {
                enter_weather_theme_picker();
            } else if (g_ui.current_page == PAGE_WORLD_CLOCK) {
                g_ui.current_page = PAGE_FORECAST;
            } else if (g_ui.current_page == PAGE_FORECAST) {
                g_ui.current_page = PAGE_PIXEL_TEST;
                g_ui.pixel_test_pattern = 0;
            } else if (g_ui.current_page == PAGE_PIXEL_TEST) {
                g_ui.current_page = PAGE_FORECAST;
            }
            g_ui.long_press_handled = true;
            publish_touch_event("long", now - g_ui.press_start_time, page_before, g_ui.current_page);
            publish_telemetry_if_connected();
        }
    } else if (!pin_state && g_ui.last_pin_state) {
        if ((now - g_ui.press_start_time < kLongPressTimeMs) && !g_ui.long_press_handled) {
            g_ui.tap_counter += 1;
            g_ui.last_tap_time = now;
        }
    }

    g_ui.last_pin_state = pin_state;

    if (g_ui.tap_counter > 0 && now - g_ui.last_tap_time > kDoubleTapDelayMs) {
        g_ui.last_page_switch = now;
        if (g_ui.tap_counter == 2) {
            if (g_ui.weather_theme_picker_active) {
                step_weather_theme_preview(-1);
            } else {
                apply_display_brightness(g_ui.high_brightness ? 10 : 100);
                publish_telemetry_if_connected();
            }
            publish_touch_event("double", now - g_ui.press_start_time, page_before, g_ui.current_page);
        } else {
            if (g_ui.weather_theme_picker_active) {
                step_weather_theme_preview(1);
            } else if (g_ui.current_page == PAGE_PIXEL_TEST) {
                g_ui.pixel_test_pattern = (g_ui.pixel_test_pattern + 1) % 6;
            } else if (g_ui.current_page == PAGE_WORLD_CLOCK) {
                g_ui.current_page = PAGE_CLOCK;
            } else if (g_ui.current_page == PAGE_FORECAST) {
                g_ui.current_page = PAGE_WEATHER;
            } else {
                g_ui.current_page += 1;
                if (g_ui.current_page > PAGE_WEATHER) {
                    g_ui.current_page = PAGE_EMO;
                }
            }
            publish_touch_event("short", now - g_ui.press_start_time, page_before, g_ui.current_page);
            if (!g_ui.weather_theme_picker_active) {
                publish_telemetry_if_connected();
            }
        }
        g_ui.tap_counter = 0;
    }
}

bool should_force_config_mode()
{
    ESP_LOGI(kTag, "checking force-config touch hold for %lu ms", kConfigHoldMs);
    const unsigned long start = millis();
    while (millis() - start < kConfigHoldMs) {
        if (gpio_get_level(kTouchPin) != 0) {
            ESP_LOGW(kTag, "touch detected during boot, forcing config mode");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(80));
    }
    ESP_LOGI(kTag, "no force-config touch detected");
    return false;
}

void render_page()
{
    g_display.clear_display();
    switch (g_ui.current_page) {
    case PAGE_EMO:
        draw_emo_page();
        break;
    case PAGE_CLOCK:
        draw_clock_page();
        break;
    case PAGE_WEATHER:
        draw_weather_page();
        break;
    case PAGE_WORLD_CLOCK:
        draw_world_clock_page();
        break;
    case PAGE_FORECAST:
        draw_forecast_page();
        break;
    case PAGE_PIXEL_TEST:
        draw_pixel_test_page();
        break;
    default:
        draw_emo_page();
        break;
    }
    g_display.display();
}
