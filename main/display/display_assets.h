#pragma once

#include <string>

extern const unsigned char bmp_tiny_drop[];
extern const unsigned char bmp_heart[];
extern const unsigned char bmp_zzz[];
extern const unsigned char bmp_anger[];

const unsigned char *get_big_icon(const std::string &weather);
const unsigned char *get_mini_icon(const std::string &weather);
