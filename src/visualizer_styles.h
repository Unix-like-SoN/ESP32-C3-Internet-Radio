#ifndef VISUALIZER_STYLES_H
#define VISUALIZER_STYLES_H

#include <stdint.h>

// Доступные стили визуализаторов
enum VisualizerStyle : uint8_t {
    STYLE_BARS = 0,      // Вертикальные полосы (текущий)
    STYLE_WAVE = 1,      // Прокручивающаяся волна
    STYLE_CIRCLE = 2,    // Круговой визуализатор
    STYLE_HEXAGON = 3,   // Киберпанк гексагоны
    STYLE_STARS = 4,     // Звездное поле
    STYLE_MIRROR = 5,    // Зеркальные полосы
    STYLE_LIGHTNING = 6, // Молнии между точками
    STYLE_TESSERACT = 7, // 4D гиперкуб (тессеракт)
    STYLE_PLASMA = 8     // Жидкая плазма (интерференция волн)
};

// Количество доступных стилей
#define VISUALIZER_STYLE_COUNT 9

#endif // VISUALIZER_STYLES_H
