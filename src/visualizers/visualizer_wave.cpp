#include "visualizer_wave.h"

void IRAM_ATTR VisualizerWave::draw(Adafruit_SSD1306& display, int* bands, int bandCount) {
    // Вычисляем среднюю амплитуду из всех полос
    long total = 0;
    for (int i = 0; i < bandCount; i++) {
        total += bands[i];
    }
    int avgAmplitude = total / bandCount;
    
    // Записываем новое значение в историю
    waveHistory[writePos] = avgAmplitude;
    writePos = (writePos + 1) % SCREEN_WIDTH;
    
    // Рисуем волну
    int prevY = SCREEN_HEIGHT / 2;
    
    for (int x = 0; x < SCREEN_WIDTH - 1; x++) {
        // Индекс в истории (старые значения слева, новые справа)
        int historyIndex = (writePos + x) % SCREEN_WIDTH;
        int nextHistoryIndex = (writePos + x + 1) % SCREEN_WIDTH;
        
        // Преобразуем амплитуду в Y координату (центр экрана ± амплитуда)
        int y1 = SCREEN_HEIGHT / 2 - waveHistory[historyIndex] / 2;
        int y2 = SCREEN_HEIGHT / 2 - waveHistory[nextHistoryIndex] / 2;
        
        // Ограничиваем значения
        y1 = constrain(y1, 0, SCREEN_HEIGHT - 1);
        y2 = constrain(y2, 0, SCREEN_HEIGHT - 1);
        
        // Рисуем линию между двумя точками (smooth curve)
        display.drawLine(x, y1, x + 1, y2, SSD1306_WHITE);
    }
    
    // Рисуем центральную линию для референса (тонкая)
    for (int x = 0; x < SCREEN_WIDTH; x += 4) {
        display.drawPixel(x, SCREEN_HEIGHT / 2, SSD1306_WHITE);
    }
}
