#ifndef VISUALIZER_BARS_H
#define VISUALIZER_BARS_H

#include "../visualizer_base.h"
#include "../config.h"

// Визуализатор: Вертикальные полосы с gravity effect
class VisualizerBars : public VisualizerBase {
private:
    int displayBands[16] = {0};  // Текущая высота для плавной анимации
    
public:
    void draw(Adafruit_SSD1306& display, int* bands, int bandCount) override;
    const char* getName() override { return "Bars"; }
};

#endif // VISUALIZER_BARS_H
