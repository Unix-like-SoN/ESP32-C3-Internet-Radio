#ifndef VISUALIZER_WAVE_H
#define VISUALIZER_WAVE_H

#include "../visualizer_base.h"
#include "../config.h"

// Визуализатор: Прокручивающаяся волна слева направо
class VisualizerWave : public VisualizerBase {
private:
    int waveHistory[SCREEN_WIDTH] = {0};  // История амплитуд для каждого пикселя
    int writePos = 0;                     // Текущая позиция записи
    
public:
    void draw(Adafruit_SSD1306& display, int* bands, int bandCount) override;
    const char* getName() override { return "Wave"; }
};

#endif // VISUALIZER_WAVE_H
