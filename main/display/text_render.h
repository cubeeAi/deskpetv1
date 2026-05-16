#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

int tiny_text_width(const std::string &text, int scale = 1);
void draw_tiny_text(int16_t x, int16_t y, const std::string &text, uint8_t color, int scale = 1);
int big_text_width(const std::string &text);
void draw_big_text(int16_t x, int16_t y, const std::string &text);
bool text_contains_non_ascii(const std::string &text);
std::string to_display_text(const std::string &value);
std::string truncate_text(const std::string &value, size_t limit);
void copy_to_buffer(char *target, size_t target_size, const std::string &value);
bool copy_wifi_ssid_bytes(uint8_t *target, size_t target_size, const std::string &value);
std::string url_encode(const std::string &value);
std::string bytes_to_hex(const uint8_t *data, size_t len);
std::string html_escape(const std::string &value);
std::vector<std::pair<std::string, std::string>> parse_form_urlencoded(const std::string &body);
std::string get_form_value(const std::vector<std::pair<std::string, std::string>> &pairs, const std::string &key);
bool parse_double_string(const std::string &value, double *out_value);
