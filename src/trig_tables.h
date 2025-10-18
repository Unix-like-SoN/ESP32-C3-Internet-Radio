#ifndef TRIG_TABLES_H
#define TRIG_TABLES_H

#include <stdint.h>

// Lookup таблицы для sin/cos (без float!)
// 16 значений для круга (22.5° шаги)
// Масштаб: * 256 для fixed-point

static const int16_t sin_table_16[16] = {
    0,    // 0°
    98,   // 22.5°
    181,  // 45°
    236,  // 67.5°
    256,  // 90°
    236,  // 112.5°
    181,  // 135°
    98,   // 157.5°
    0,    // 180°
    -98,  // 202.5°
    -181, // 225°
    -236, // 247.5°
    -256, // 270°
    -236, // 292.5°
    -181, // 315°
    -98   // 337.5°
};

static const int16_t cos_table_16[16] = {
    256,  // 0°
    236,  // 22.5°
    181,  // 45°
    98,   // 67.5°
    0,    // 90°
    -98,  // 112.5°
    -181, // 135°
    -236, // 157.5°
    -256, // 180°
    -236, // 202.5°
    -181, // 225°
    -98,  // 247.5°
    0,    // 270°
    98,   // 292.5°
    181,  // 315°
    236   // 337.5°
};

// Быстрый sin для индекса 0-15
inline int16_t fast_sin(int index) {
    return sin_table_16[index & 0x0F];
}

// Быстрый cos для индекса 0-15
inline int16_t fast_cos(int index) {
    return cos_table_16[index & 0x0F];
}

#endif // TRIG_TABLES_H
