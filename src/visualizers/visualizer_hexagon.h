#ifndef VISUALIZER_HEXAGON_H
#define VISUALIZER_HEXAGON_H

#include "../visualizer_base.h"
#include "../config.h"

// Визуализатор: Гексагональные соты (как в улье)
class VisualizerHexagon : public VisualizerBase {
private:
    int currentRings = 0;     // Текущее количество колец сот
    int targetRings = 0;      // Целевое количество колец
    
    // Вспомогательная функция для рисования гексагона
    void drawHexagon(Adafruit_SSD1306& display, int centerX, int centerY, int size);
    
public:
    void draw(Adafruit_SSD1306& display, int* bands, int bandCount) override;
    const char* getName() override { return "Hexagon"; }
};

#endif // VISUALIZER_HEXAGON_H
