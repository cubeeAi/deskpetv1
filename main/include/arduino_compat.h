#pragma once

#include <stdint.h>

#include "esp_random.h"
#include "esp_timer.h"

using byte = uint8_t;

inline unsigned long millis()
{
    return static_cast<unsigned long>(esp_timer_get_time() / 1000ULL);
}

inline long random(long max)
{
    if (max <= 0) {
        return 0;
    }
    return static_cast<long>(esp_random() % static_cast<uint32_t>(max));
}

inline long random(long min, long max)
{
    if (max <= min) {
        return min;
    }
    return min + random(max - min);
}
