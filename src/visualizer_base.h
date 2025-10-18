#ifndef VISUALIZER_BASE_H
#define VISUALIZER_BASE_H

#include <Adafruit_SSD1306.h>

// Абстрактный базовый класс для всех визуализаторов
class VisualizerBase {
public:
    // Основной метод отрисовки
    // display - ссылка на OLED дисплей
    // bands - массив амплитуд (16 значений)
    // bandCount - количество полос
    virtual void draw(Adafruit_SSD1306& display, int* bands, int bandCount) = 0;
    
    // Название визуализатора для UI
    virtual const char* getName() = 0;
    
    // Виртуальный деструктор
    virtual ~VisualizerBase() {}
};

#endif // VISUALIZER_BASE_H
