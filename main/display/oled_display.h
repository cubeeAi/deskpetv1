#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

#include "app/app_types.h"

extern "C" {
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/task.h"
}

class Ssd1306Display {
public:
    esp_err_t begin()
    {
        i2c_config_t config = {};
        config.mode = I2C_MODE_MASTER;
        config.sda_io_num = kOledSdaPin;
        config.scl_io_num = kOledSclPin;
        config.sda_pullup_en = GPIO_PULLUP_ENABLE;
        config.scl_pullup_en = GPIO_PULLUP_ENABLE;
        config.master.clk_speed = kI2cClockHz;

        ESP_ERROR_CHECK(i2c_param_config(kI2cPort, &config));
        ESP_ERROR_CHECK(i2c_driver_install(kI2cPort, config.mode, 0, 0, 0));

        static constexpr uint8_t init_commands[] = {
            0xAE,
            0xD5, 0x80,
            0xA8, 0x3F,
            0xD3, 0x00,
            0x40,
            0xAD, 0x8B,
            0xA1,
            0xC8,
            0xDA, 0x12,
            0x81, 0x80,
            0xD9, 0x22,
            0xDB, 0x40,
            0xA4,
            0xA6,
            0xAF,
        };

        for (uint8_t command : init_commands) {
            ESP_ERROR_CHECK(send_command(command));
        }

        clear_display();
        return display();
    }

    void clear_display()
    {
        buffer_.fill(0);
    }

    esp_err_t display()
    {
        std::array<uint8_t, 129> packet = {};
        packet[0] = 0x40;

        for (uint8_t page = 0; page < 8; ++page) {
            esp_err_t err = send_command(static_cast<uint8_t>(0xB0U | page));
            if (err != ESP_OK) {
                return err;
            }
            err = send_command(static_cast<uint8_t>(0x10U | ((kOledColumnOffset >> 4U) & 0x0FU)));
            if (err != ESP_OK) {
                return err;
            }
            err = send_command(static_cast<uint8_t>(0x00U | (kOledColumnOffset & 0x0FU)));
            if (err != ESP_OK) {
                return err;
            }

            const size_t offset = static_cast<size_t>(page) * kScreenWidth;
            std::memcpy(packet.data() + 1, buffer_.data() + offset, kScreenWidth);
            err = i2c_master_write_to_device(
                kI2cPort,
                kOledAddress,
                packet.data(),
                packet.size(),
                pdMS_TO_TICKS(100));
            if (err != ESP_OK) {
                return err;
            }
        }

        return ESP_OK;
    }

    void set_contrast(uint8_t contrast)
    {
        const esp_err_t first = send_command(0x81);
        if (first != ESP_OK) {
            ESP_LOGW(kTag, "set contrast command failed: %s", esp_err_to_name(first));
            return;
        }
        const esp_err_t second = send_command(contrast);
        if (second != ESP_OK) {
            ESP_LOGW(kTag, "set contrast value failed: %s", esp_err_to_name(second));
        }
    }

    void draw_pixel(int16_t x, int16_t y, uint8_t color)
    {
        if (x < 0 || x >= kScreenWidth || y < 0 || y >= kScreenHeight) {
            return;
        }

        const size_t index = static_cast<size_t>(x) + (static_cast<size_t>(y) / 8U) * kScreenWidth;
        const uint8_t mask = static_cast<uint8_t>(1U << (y & 0x07));
        if (color) {
            buffer_[index] |= mask;
        } else {
            buffer_[index] &= static_cast<uint8_t>(~mask);
        }
    }

    void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color)
    {
        int16_t dx = std::abs(x1 - x0);
        const int16_t sx = x0 < x1 ? 1 : -1;
        int16_t dy = -std::abs(y1 - y0);
        const int16_t sy = y0 < y1 ? 1 : -1;
        int16_t err = dx + dy;

        while (true) {
            draw_pixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) {
                break;
            }
            const int16_t err2 = static_cast<int16_t>(2 * err);
            if (err2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (err2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    void fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
    {
        if (w <= 0 || h <= 0) {
            return;
        }
        for (int16_t py = y; py < y + h; ++py) {
            for (int16_t px = x; px < x + w; ++px) {
                draw_pixel(px, py, color);
            }
        }
    }

    void fill_circle(int16_t x0, int16_t y0, int16_t radius, uint8_t color)
    {
        if (radius <= 0) {
            return;
        }
        for (int16_t dy = -radius; dy <= radius; ++dy) {
            for (int16_t dx = -radius; dx <= radius; ++dx) {
                if ((dx * dx) + (dy * dy) <= radius * radius) {
                    draw_pixel(static_cast<int16_t>(x0 + dx), static_cast<int16_t>(y0 + dy), color);
                }
            }
        }
    }

    void draw_bitmap(int16_t x, int16_t y, const unsigned char *bitmap, int16_t w, int16_t h, uint8_t color)
    {
        if (bitmap == nullptr || w <= 0 || h <= 0) {
            return;
        }

        const int16_t byte_width = static_cast<int16_t>((w + 7) / 8);
        for (int16_t py = 0; py < h; ++py) {
            for (int16_t px = 0; px < w; ++px) {
                const uint8_t byte = bitmap[py * byte_width + (px / 8)];
                if ((byte & (0x80U >> (px & 7))) != 0) {
                    draw_pixel(static_cast<int16_t>(x + px), static_cast<int16_t>(y + py), color);
                }
            }
        }
    }

    void fill_round_rect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color)
    {
        if (w <= 0 || h <= 0) {
            return;
        }

        const int16_t clamped_radius = std::max<int16_t>(0, std::min<int16_t>(radius, std::min(w, h) / 2));
        for (int16_t py = y; py < y + h; ++py) {
            for (int16_t px = x; px < x + w; ++px) {
                if (inside_rounded_rect(px, py, x, y, w, h, clamped_radius)) {
                    draw_pixel(px, py, color);
                }
            }
        }
    }

private:
    std::array<uint8_t, kScreenWidth * kScreenHeight / 8> buffer_ = {};

    esp_err_t send_command(uint8_t command)
    {
        const uint8_t payload[] = {0x00, command};
        return i2c_master_write_to_device(kI2cPort, kOledAddress, payload, sizeof(payload), pdMS_TO_TICKS(100));
    }

    static bool inside_rounded_rect(int16_t px, int16_t py, int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius)
    {
        if (radius <= 0) {
            return true;
        }

        if ((px >= x + radius && px < x + w - radius) || (py >= y + radius && py < y + h - radius)) {
            return true;
        }

        const int16_t left = static_cast<int16_t>(x + radius);
        const int16_t right = static_cast<int16_t>(x + w - radius - 1);
        const int16_t top = static_cast<int16_t>(y + radius);
        const int16_t bottom = static_cast<int16_t>(y + h - radius - 1);

        int16_t center_x = (px < left) ? left : right;
        int16_t center_y = (py < top) ? top : bottom;

        if (px >= left && px <= right) {
            center_x = px;
        }
        if (py >= top && py <= bottom) {
            center_y = py;
        }

        const int32_t dx = static_cast<int32_t>(px - center_x);
        const int32_t dy = static_cast<int32_t>(py - center_y);
        return (dx * dx) + (dy * dy) <= static_cast<int32_t>(radius) * static_cast<int32_t>(radius);
    }
};

extern Ssd1306Display g_display;
