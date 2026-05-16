#include "display/text_render.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>

#include "display/oled_display.h"

constexpr int kCjkGlyphWidth = 12;
constexpr int kCjkGlyphHeight = 12;
constexpr int kCjkGlyphBytesPerRow = 2;
constexpr int kCjkGlyphBytes = kCjkGlyphBytesPerRow * kCjkGlyphHeight;
constexpr int kCjkAdvance = 13;

extern const uint8_t _binary_cjk_3000_303f_12_bin_start[] asm("_binary_cjk_3000_303f_12_bin_start");
extern const uint8_t _binary_cjk_3400_9fff_12_bin_start[] asm("_binary_cjk_3400_9fff_12_bin_start");
extern const uint8_t _binary_cjk_ff00_ffef_12_bin_start[] asm("_binary_cjk_ff00_ffef_12_bin_start");

// 兼容旧的资源符号命名，避免构建缓存或残留引用导致编译失败。
#define _binary_assets_cjk_3000_303f_12_bin_start _binary_cjk_3000_303f_12_bin_start
#define _binary_assets_cjk_3400_9fff_12_bin_start _binary_cjk_3400_9fff_12_bin_start
#define _binary_assets_cjk_ff00_ffef_12_bin_start _binary_cjk_ff00_ffef_12_bin_start


struct Glyph5x7 {
    std::array<uint8_t, 5> columns;
};

Glyph5x7 glyph_for(char raw)
{
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
    switch (c) {
    case '0': return {{0x3E, 0x51, 0x49, 0x45, 0x3E}};
    case '1': return {{0x00, 0x42, 0x7F, 0x40, 0x00}};
    case '2': return {{0x42, 0x61, 0x51, 0x49, 0x46}};
    case '3': return {{0x21, 0x41, 0x45, 0x4B, 0x31}};
    case '4': return {{0x18, 0x14, 0x12, 0x7F, 0x10}};
    case '5': return {{0x27, 0x45, 0x45, 0x45, 0x39}};
    case '6': return {{0x3C, 0x4A, 0x49, 0x49, 0x30}};
    case '7': return {{0x01, 0x71, 0x09, 0x05, 0x03}};
    case '8': return {{0x36, 0x49, 0x49, 0x49, 0x36}};
    case '9': return {{0x06, 0x49, 0x49, 0x29, 0x1E}};
    case 'A': return {{0x7E, 0x11, 0x11, 0x11, 0x7E}};
    case 'B': return {{0x7F, 0x49, 0x49, 0x49, 0x36}};
    case 'C': return {{0x3E, 0x41, 0x41, 0x41, 0x22}};
    case 'D': return {{0x7F, 0x41, 0x41, 0x22, 0x1C}};
    case 'E': return {{0x7F, 0x49, 0x49, 0x49, 0x41}};
    case 'F': return {{0x7F, 0x09, 0x09, 0x09, 0x01}};
    case 'G': return {{0x3E, 0x41, 0x49, 0x49, 0x7A}};
    case 'H': return {{0x7F, 0x08, 0x08, 0x08, 0x7F}};
    case 'I': return {{0x00, 0x41, 0x7F, 0x41, 0x00}};
    case 'J': return {{0x20, 0x40, 0x41, 0x3F, 0x01}};
    case 'K': return {{0x7F, 0x08, 0x14, 0x22, 0x41}};
    case 'L': return {{0x7F, 0x40, 0x40, 0x40, 0x40}};
    case 'M': return {{0x7F, 0x02, 0x0C, 0x02, 0x7F}};
    case 'N': return {{0x7F, 0x04, 0x08, 0x10, 0x7F}};
    case 'O': return {{0x3E, 0x41, 0x41, 0x41, 0x3E}};
    case 'P': return {{0x7F, 0x09, 0x09, 0x09, 0x06}};
    case 'Q': return {{0x3E, 0x41, 0x51, 0x21, 0x5E}};
    case 'R': return {{0x7F, 0x09, 0x19, 0x29, 0x46}};
    case 'S': return {{0x46, 0x49, 0x49, 0x49, 0x31}};
    case 'T': return {{0x01, 0x01, 0x7F, 0x01, 0x01}};
    case 'U': return {{0x3F, 0x40, 0x40, 0x40, 0x3F}};
    case 'V': return {{0x1F, 0x20, 0x40, 0x20, 0x1F}};
    case 'W': return {{0x7F, 0x20, 0x18, 0x20, 0x7F}};
    case 'X': return {{0x63, 0x14, 0x08, 0x14, 0x63}};
    case 'Y': return {{0x03, 0x04, 0x78, 0x04, 0x03}};
    case 'Z': return {{0x61, 0x51, 0x49, 0x45, 0x43}};
    case ' ': return {{0x00, 0x00, 0x00, 0x00, 0x00}};
    case '-': return {{0x08, 0x08, 0x08, 0x08, 0x08}};
    case '.': return {{0x00, 0x40, 0x60, 0x00, 0x00}};
    case ',': return {{0x00, 0x50, 0x30, 0x00, 0x00}};
    case ':': return {{0x00, 0x36, 0x36, 0x00, 0x00}};
    case '!': return {{0x00, 0x00, 0x5F, 0x00, 0x00}};
    case '?': return {{0x02, 0x01, 0x51, 0x09, 0x06}};
    case '/': return {{0x20, 0x10, 0x08, 0x04, 0x02}};
    case '%': return {{0x63, 0x13, 0x08, 0x64, 0x63}};
    case '~': return {{0x08, 0x04, 0x08, 0x10, 0x08}};
    case '\'': return {{0x00, 0x05, 0x03, 0x00, 0x00}};
    default: return {{0x02, 0x01, 0x51, 0x09, 0x06}};
    }
}

void draw_tiny_char(int16_t x, int16_t y, char c, uint8_t color, int scale = 1)
{
    const Glyph5x7 glyph = glyph_for(c);
    for (int column = 0; column < 5; ++column) {
        const uint8_t bits = glyph.columns[static_cast<size_t>(column)];
        for (int row = 0; row < 7; ++row) {
            if ((bits & (1U << row)) == 0) {
                continue;
            }
            g_display.fill_rect(
                static_cast<int16_t>(x + column * scale),
                static_cast<int16_t>(y + row * scale),
                static_cast<int16_t>(scale),
                static_cast<int16_t>(scale),
                color);
        }
    }
}

bool decode_utf8_codepoint(const std::string &text, size_t *index, uint32_t *codepoint)
{
    if (index == nullptr || codepoint == nullptr || *index >= text.size()) {
        return false;
    }

    const unsigned char b0 = static_cast<unsigned char>(text[*index]);
    if ((b0 & 0x80U) == 0) {
        *codepoint = b0;
        *index += 1;
        return true;
    }
    if ((b0 & 0xE0U) == 0xC0U && *index + 1 < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[*index + 1]);
        *codepoint = ((b0 & 0x1FU) << 6) | (b1 & 0x3FU);
        *index += 2;
        return true;
    }
    if ((b0 & 0xF0U) == 0xE0U && *index + 2 < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[*index + 1]);
        const unsigned char b2 = static_cast<unsigned char>(text[*index + 2]);
        *codepoint = ((b0 & 0x0FU) << 12) | ((b1 & 0x3FU) << 6) | (b2 & 0x3FU);
        *index += 3;
        return true;
    }
    if ((b0 & 0xF8U) == 0xF0U && *index + 3 < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[*index + 1]);
        const unsigned char b2 = static_cast<unsigned char>(text[*index + 2]);
        const unsigned char b3 = static_cast<unsigned char>(text[*index + 3]);
        *codepoint = ((b0 & 0x07U) << 18) | ((b1 & 0x3FU) << 12) | ((b2 & 0x3FU) << 6) | (b3 & 0x3FU);
        *index += 4;
        return true;
    }

    *codepoint = '?';
    *index += 1;
    return true;
}

bool text_contains_non_ascii(const std::string &text)
{
    for (unsigned char c : text) {
        if (c >= 0x80) {
            return true;
        }
    }
    return false;
}

std::string to_display_text(const std::string &value)
{
    std::string result;
    result.reserve(value.size());
    for (unsigned char c : value) {
        if (c == '\n') {
            result.push_back('\n');
        } else if (c < 0x80) {
            result.push_back(static_cast<char>(std::toupper(c)));
        } else {
            result.push_back(static_cast<char>(c));
        }
    }
    return result;
}

const uint8_t *find_cjk_glyph(uint32_t codepoint)
{
    if (codepoint >= 0x3000 && codepoint <= 0x303F) {
        return _binary_cjk_3000_303f_12_bin_start + (codepoint - 0x3000U) * kCjkGlyphBytes;
    }
    if (codepoint >= 0x3400 && codepoint <= 0x9FFF) {
        return _binary_cjk_3400_9fff_12_bin_start + (codepoint - 0x3400U) * kCjkGlyphBytes;
    }
    if (codepoint >= 0xFF00 && codepoint <= 0xFFEF) {
        return _binary_cjk_ff00_ffef_12_bin_start + (codepoint - 0xFF00U) * kCjkGlyphBytes;
    }
    return nullptr;
}

void draw_cjk_glyph(int16_t x, int16_t y, uint32_t codepoint, uint8_t color, int scale = 1)
{
    const uint8_t *glyph = find_cjk_glyph(codepoint);
    if (glyph == nullptr) {
        g_display.draw_line(x, y, static_cast<int16_t>(x + 8 * scale), static_cast<int16_t>(y + 8 * scale), color);
        g_display.draw_line(static_cast<int16_t>(x + 8 * scale), y, x, static_cast<int16_t>(y + 8 * scale), color);
        return;
    }

    for (int row = 0; row < kCjkGlyphHeight; ++row) {
        for (int col = 0; col < kCjkGlyphWidth; ++col) {
            const int byte_index = row * kCjkGlyphBytesPerRow + (col / 8);
            const uint8_t byte = glyph[byte_index];
            if ((byte & (0x80U >> (col & 7))) == 0) {
                continue;
            }
            g_display.fill_rect(
                static_cast<int16_t>(x + col * scale),
                static_cast<int16_t>(y + row * scale),
                static_cast<int16_t>(scale),
                static_cast<int16_t>(scale),
                color);
        }
    }
}

int tiny_codepoint_width(uint32_t codepoint, int scale = 1)
{
    if (codepoint < 0x80) {
        return 6 * scale;
    }
    return kCjkAdvance * scale;
}

int tiny_text_width(const std::string &text, int scale)
{
    int current_line = 0;
    int max_line = 0;
    size_t index = 0;
    while (index < text.size()) {
        uint32_t codepoint = 0;
        if (!decode_utf8_codepoint(text, &index, &codepoint)) {
            break;
        }
        if (codepoint == '\n') {
            max_line = std::max(max_line, current_line);
            current_line = 0;
            continue;
        }
        current_line += tiny_codepoint_width(codepoint, scale);
    }
    return std::max(max_line, current_line);
}

void draw_tiny_text(int16_t x, int16_t y, const std::string &text, uint8_t color, int scale)
{
    int16_t cursor_x = x;
    int16_t cursor_y = y;
    const int line_height = kCjkAdvance * scale;

    const std::string normalized = to_display_text(text);
    size_t index = 0;
    while (index < normalized.size()) {
        uint32_t codepoint = 0;
        if (!decode_utf8_codepoint(normalized, &index, &codepoint)) {
            break;
        }
        if (codepoint == '\n') {
            cursor_x = x;
            cursor_y = static_cast<int16_t>(cursor_y + line_height);
            continue;
        }

        if (codepoint < 0x80) {
            draw_tiny_char(cursor_x, cursor_y, static_cast<char>(codepoint), color, scale);
        } else {
            draw_cjk_glyph(cursor_x, cursor_y, codepoint, color, scale);
        }
        cursor_x = static_cast<int16_t>(cursor_x + tiny_codepoint_width(codepoint, scale));
    }
}

int big_char_width(char c)
{
    if (c == ':') {
        return 6;
    }
    if (c == '-') {
        return 10;
    }
    return 16;
}

int big_text_width(const std::string &text)
{
    int width = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        width += big_char_width(text[i]);
        if (i + 1 < text.size()) {
            width += 2;
        }
    }
    return width;
}

void draw_big_digit_segments(int16_t x, int16_t y, const std::array<bool, 7> &segments)
{
    constexpr int16_t w = 16;
    constexpr int16_t h = 26;
    constexpr int16_t t = 3;

    if (segments[0]) g_display.fill_rect(static_cast<int16_t>(x + t), y, static_cast<int16_t>(w - (2 * t)), t, 1);
    if (segments[1]) g_display.fill_rect(static_cast<int16_t>(x + w - t), static_cast<int16_t>(y + t), t, static_cast<int16_t>((h / 2) - t), 1);
    if (segments[2]) g_display.fill_rect(static_cast<int16_t>(x + w - t), static_cast<int16_t>(y + h / 2), t, static_cast<int16_t>((h / 2) - t), 1);
    if (segments[3]) g_display.fill_rect(static_cast<int16_t>(x + t), static_cast<int16_t>(y + h - t), static_cast<int16_t>(w - (2 * t)), t, 1);
    if (segments[4]) g_display.fill_rect(x, static_cast<int16_t>(y + h / 2), t, static_cast<int16_t>((h / 2) - t), 1);
    if (segments[5]) g_display.fill_rect(x, static_cast<int16_t>(y + t), t, static_cast<int16_t>((h / 2) - t), 1);
    if (segments[6]) g_display.fill_rect(static_cast<int16_t>(x + t), static_cast<int16_t>(y + (h / 2) - (t / 2)), static_cast<int16_t>(w - (2 * t)), t, 1);
}

void draw_big_char(int16_t x, int16_t y, char c)
{
    switch (c) {
    case '0': draw_big_digit_segments(x, y, {{true, true, true, true, true, true, false}}); break;
    case '1': draw_big_digit_segments(x, y, {{false, true, true, false, false, false, false}}); break;
    case '2': draw_big_digit_segments(x, y, {{true, true, false, true, true, false, true}}); break;
    case '3': draw_big_digit_segments(x, y, {{true, true, true, true, false, false, true}}); break;
    case '4': draw_big_digit_segments(x, y, {{false, true, true, false, false, true, true}}); break;
    case '5': draw_big_digit_segments(x, y, {{true, false, true, true, false, true, true}}); break;
    case '6': draw_big_digit_segments(x, y, {{true, false, true, true, true, true, true}}); break;
    case '7': draw_big_digit_segments(x, y, {{true, true, true, false, false, false, false}}); break;
    case '8': draw_big_digit_segments(x, y, {{true, true, true, true, true, true, true}}); break;
    case '9': draw_big_digit_segments(x, y, {{true, true, true, true, false, true, true}}); break;
    case ':':
        g_display.fill_rect(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 7), 4, 4, 1);
        g_display.fill_rect(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 17), 4, 4, 1);
        break;
    case '-':
        g_display.fill_rect(x, static_cast<int16_t>(y + 12), 10, 3, 1);
        break;
    default:
        break;
    }
}

void draw_big_text(int16_t x, int16_t y, const std::string &text)
{
    int16_t cursor = x;
    for (size_t i = 0; i < text.size(); ++i) {
        draw_big_char(cursor, y, text[i]);
        cursor = static_cast<int16_t>(cursor + big_char_width(text[i]));
        if (i + 1 < text.size()) {
            cursor = static_cast<int16_t>(cursor + 2);
        }
    }
}

void copy_to_buffer(char *target, size_t target_size, const std::string &value)
{
    if (target_size == 0) {
        return;
    }
    std::strncpy(target, value.c_str(), target_size - 1);
    target[target_size - 1] = '\0';
}

bool copy_wifi_ssid_bytes(uint8_t *target, size_t target_size, const std::string &value)
{
    if (target == nullptr || target_size == 0) {
        return false;
    }

    std::memset(target, 0, target_size);
    if (value.size() > target_size) {
        ESP_LOGE(
            kTag,
            "ssid is too long in bytes: %u > %u",
            static_cast<unsigned>(value.size()),
            static_cast<unsigned>(target_size));
        return false;
    }

    if (!value.empty()) {
        std::memcpy(target, value.data(), value.size());
    }
    return true;
}

std::string url_encode(const std::string &value)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);

    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            encoded += "%20";
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 0x0F]);
        }
    }
    return encoded;
}

std::string bytes_to_hex(const uint8_t *data, size_t len)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string hex;
    if (data == nullptr || len == 0) {
        return hex;
    }

    hex.reserve(len * 3);
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) {
            hex.push_back(' ');
        }
        const uint8_t byte = data[i];
        hex.push_back(kHex[(byte >> 4) & 0x0F]);
        hex.push_back(kHex[byte & 0x0F]);
    }
    return hex;
}

std::string html_escape(const std::string &value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default: escaped.push_back(c); break;
        }
    }
    return escaped;
}

int from_hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

std::string url_decode(const std::string &value)
{
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') {
            decoded.push_back(' ');
        } else if (value[i] == '%' && i + 2 < value.size()) {
            const int high = from_hex(value[i + 1]);
            const int low = from_hex(value[i + 2]);
            decoded.push_back(static_cast<char>((high << 4) | low));
            i += 2;
        } else {
            decoded.push_back(value[i]);
        }
    }
    return decoded;
}

std::vector<std::pair<std::string, std::string>> parse_form_urlencoded(const std::string &body)
{
    std::vector<std::pair<std::string, std::string>> pairs;
    size_t start = 0;
    while (start <= body.size()) {
        const size_t end = body.find('&', start);
        const std::string token = body.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const size_t eq = token.find('=');
        const std::string key = url_decode(token.substr(0, eq));
        const std::string value = url_decode(eq == std::string::npos ? std::string() : token.substr(eq + 1));
        pairs.emplace_back(key, value);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return pairs;
}

std::string get_form_value(const std::vector<std::pair<std::string, std::string>> &pairs, const std::string &key)
{
    for (const auto &entry : pairs) {
        if (entry.first == key) {
            return entry.second;
        }
    }
    return {};
}

std::string truncate_text(const std::string &value, size_t limit)
{
    if (value.empty()) {
        return value;
    }

    size_t index = 0;
    size_t codepoints = 0;
    size_t last_good_index = 0;
    while (index < value.size() && codepoints < limit) {
        last_good_index = index;
        const unsigned char byte = static_cast<unsigned char>(value[index]);
        if ((byte & 0x80U) == 0) {
            index += 1;
        } else if ((byte & 0xE0U) == 0xC0U && index + 1 < value.size()) {
            index += 2;
        } else if ((byte & 0xF0U) == 0xE0U && index + 2 < value.size()) {
            index += 3;
        } else if ((byte & 0xF8U) == 0xF0U && index + 3 < value.size()) {
            index += 4;
        } else {
            index += 1;
        }
        codepoints += 1;
    }

    if (index >= value.size()) {
        return value;
    }
    if (limit <= 1) {
        return value.substr(0, index);
    }
    return value.substr(0, last_good_index) + "...";
}

bool parse_double_string(const std::string &value, double *out_value)
{
    if (out_value == nullptr || value.empty()) {
        return false;
    }

    char *end = nullptr;
    const double parsed = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || (end != nullptr && *end != '\0')) {
        return false;
    }

    *out_value = parsed;
    return true;
}

