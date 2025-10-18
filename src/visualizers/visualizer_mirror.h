#ifndef VISUALIZER_MIRROR_H
#define VISUALIZER_MIRROR_H

#include "../visualizer_base.h"
#include "../config.h"

// Визуализатор: Зеркальные полосы (симметрия по центру)
class VisualizerMirror : public VisualizerBase {
private:
    int displayBands[16] = {0};  // Текущая высота для плавной анимации
    
public:
    void draw(Adafruit_SSD1306& display, int* bands, int bandCount) override;
    const char* getName() override { return "Mirror"; }
};

#endif // VISUALIZER_MIRROR_H
