#ifndef VISUALIZER_STARS_H
#define VISUALIZER_STARS_H

#include "../visualizer_base.h"

// Визуализатор: Звездное поле (интенсивное!)
class VisualizerStars : public VisualizerBase {
private:
    struct Star {
        int16_t x;        // Позиция X * 256 (fixed-point)
        int16_t y;        // Позиция Y * 256
        int16_t speedX;   // Скорость X * 256
        int16_t speedY;   // Скорость Y * 256
        uint8_t brightness;
        uint8_t trailLength;
        bool active;
    };
    
    static const int MAX_STARS = 64;
    Star stars[MAX_STARS];
    int frameCount = 0;
    
public:
    VisualizerStars();
    void draw(Adafruit_SSD1306& display, int* bands, int bandCount) override;
    const char* getName() override { return "Stars"; }
};

#endif // VISUALIZER_STARS_H
