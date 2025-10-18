#include "visualizer_bars.h"

void IRAM_ATTR VisualizerBars::draw(Adafruit_SSD1306& display, int* bands, int bandCount) {
    const int bandWidth = SCREEN_WIDTH / bandCount;
    
    for (int i = 0; i < bandCount; i++) {
        int targetHeight = bands[i];
        int currentHeight = displayBands[i];
        
        // Плавная анимация
        if (targetHeight > currentHeight) {
            displayBands[i] = targetHeight;  // Мгновенный рост (attack)
        } else {
            displayBands[i] = max(0, currentHeight - 2);  // Плавное падение (gravity)
        }
        
        int bandHeight = constrain(displayBands[i], 0, SCREEN_HEIGHT);
        
        if (bandHeight > 0) {
            int x = i * bandWidth;
            display.fillRoundRect(
                x + 1,                           // x с отступом
                SCREEN_HEIGHT - bandHeight,      // y (снизу вверх)
                bandWidth - 2,                   // ширина с gap
                bandHeight,                      // высота
                1,                               // радиус скругления
                SSD1306_WHITE
            );
        }
    }
}
