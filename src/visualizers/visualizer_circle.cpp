#include "visualizer_circle.h"
#include "../trig_tables.h"

void IRAM_ATTR VisualizerCircle::draw(Adafruit_SSD1306& display, int* bands, int bandCount) {
    const int centerX = SCREEN_WIDTH / 2;   // 64
    const int centerY = SCREEN_HEIGHT / 2;  // 16
    
    // Максимальный радиус - до края экрана (по диагонали)
    // sqrt(64^2 + 16^2) ≈ 66, округляем до 70 для запаса
    const int maxRadius = 70;
    const int minRadius = 3;
    
    // Сглаживание с быстрой атакой и плавным затуханием
    for (int i = 0; i < bandCount; i++) {
        int target = bands[i];
        if (target > smoothedBands[i]) {
            // Быстрая атака - "выстрел" луча
            smoothedBands[i] = target;
        } else {
            // Плавное затухание - возврат к центру
            smoothedBands[i] = max(0, smoothedBands[i] - 3);
        }
    }
    
    // Рисуем лучи из центра (без float!)
    for (int i = 0; i < bandCount; i++) {
        // Индекс для lookup таблицы (0-15)
        int angleIdx = (i * 16) / bandCount;
        
        // Нормализуем амплитуду
        int radius = map(smoothedBands[i], 0, SCREEN_HEIGHT, minRadius, maxRadius);
        radius = constrain(radius, minRadius, maxRadius);
        
        // Координаты через lookup таблицы (fixed-point * 256)
        int endX = centerX + ((radius * fast_cos(angleIdx)) >> 8);
        int endY = centerY + ((radius * fast_sin(angleIdx)) >> 8);
        
        // Ограничиваем
        endX = constrain(endX, 0, SCREEN_WIDTH - 1);
        endY = constrain(endY, 0, SCREEN_HEIGHT - 1);
        
        // Рисуем основной луч
        display.drawLine(centerX, centerY, endX, endY, SSD1306_WHITE);
        
        // Свечение (соседний луч)
        if (radius > maxRadius / 2 && angleIdx < 15) {
            int endX2 = centerX + ((radius * fast_cos(angleIdx + 1)) >> 8);
            int endY2 = centerY + ((radius * fast_sin(angleIdx + 1)) >> 8);
            endX2 = constrain(endX2, 0, SCREEN_WIDTH - 1);
            endY2 = constrain(endY2, 0, SCREEN_HEIGHT - 1);
            display.drawLine(centerX, centerY, endX2, endY2, SSD1306_WHITE);
        }
    }
    
    // Рисуем пульсирующую центральную точку (зависит от средней амплитуды)
    long totalAmp = 0;
    for (int i = 0; i < bandCount; i++) {
        totalAmp += smoothedBands[i];
    }
    int avgAmp = totalAmp / bandCount;
    int centerSize = map(avgAmp, 0, SCREEN_HEIGHT, 1, 3);
    display.fillCircle(centerX, centerY, centerSize, SSD1306_WHITE);
}
