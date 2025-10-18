#ifndef VISUALIZER_PLASMA_H
#define VISUALIZER_PLASMA_H

#include "../visualizer_base.h"

// Визуализатор: Жидкая плазма (интерференция волн)
// Математический эффект наложения синусоидальных волн
class VisualizerPlasma : public VisualizerBase {
private:
    // Фазы волн (инкрементируются каждый кадр)
    int16_t phase1;  // Горизонтальная волна
    int16_t phase2;  // Вертикальная волна
    int16_t phase3;  // Диагональная волна
    int16_t phase4;  // Радиальная волна
    
    // Скорости волн (зависят от музыки)
    int16_t speed1;
    int16_t speed2;
    int16_t speed3;
    int16_t speed4;
    
    // Порог отрисовки (зависит от общей энергии)
    int16_t threshold;
    
    // Позиция радиального центра
    int8_t centerX;
    int8_t centerY;
    
    // Lookup таблица для sin (256 значений, -127...127)
    static const int8_t sinTable[256];
    
    // Быстрый sin через lookup таблицу
    int8_t fastSin(uint8_t angle);
    
    // Вычисление плазмы для пикселя (x, y)
    int16_t calculatePlasma(int x, int y);
    
    // Быстрое вычисление расстояния (Manhattan distance)
    int16_t distance(int x1, int y1, int x2, int y2);
    
public:
    VisualizerPlasma();
    void draw(Adafruit_SSD1306& display, int* bands, int bandCount) override;
    const char* getName() override { return "Plasma"; }
};

#endif // VISUALIZER_PLASMA_H
