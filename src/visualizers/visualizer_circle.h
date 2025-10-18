#ifndef VISUALIZER_CIRCLE_H
#define VISUALIZER_CIRCLE_H

#include "../visualizer_base.h"
#include "../config.h"

// Визуализатор: Круговой (лучи из центра)
class VisualizerCircle : public VisualizerBase {
private:
    int smoothedBands[16] = {0};  // Сглаженные значения для плавности
    
public:
    void draw(Adafruit_SSD1306& display, int* bands, int bandCount) override;
    const char* getName() override { return "Circle"; }
};

#endif // VISUALIZER_CIRCLE_H
