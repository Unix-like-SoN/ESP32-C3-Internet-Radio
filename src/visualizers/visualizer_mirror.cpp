#include "visualizer_mirror.h"

void IRAM_ATTR VisualizerMirror::draw(Adafruit_SSD1306& display, int* bands, int bandCount) {
    const int bandWidth = SCREEN_WIDTH / bandCount;
    const int centerY = SCREEN_HEIGHT / 2;
    const int maxHeight = SCREEN_HEIGHT / 2 - 1;
    
    for (int i = 0; i < bandCount; i++) {
        int targetHeight = bands[i];
        int currentHeight = displayBands[i];
        
        // Плавная анимация
        if (targetHeight > currentHeight) {
            displayBands[i] = targetHeight;
        } else {
            displayBands[i] = max(0, currentHeight - 2);
        }
        
        // Нормализуем высоту под половину экрана
        int bandHeight = map(displayBands[i], 0, SCREEN_HEIGHT, 0, maxHeight);
        bandHeight = constrain(bandHeight, 0, maxHeight);
        
        if (bandHeight > 0) {
            int x = i * bandWidth;
            
            // Верхняя половина (растет вниз от верха)
            display.fillRoundRect(
                x + 1,
                centerY - bandHeight,
                bandWidth - 2,
                bandHeight,
                1,
                SSD1306_WHITE
            );
            
            // Нижняя половина (растет вверх от низа)
            display.fillRoundRect(
                x + 1,
                centerY,
                bandWidth - 2,
                bandHeight,
                1,
                SSD1306_WHITE
            );
        }
    }
    
    // Центральная разделительная линия
    for (int x = 0; x < SCREEN_WIDTH; x += 2) {
        display.drawPixel(x, centerY, SSD1306_WHITE);
    }
}
