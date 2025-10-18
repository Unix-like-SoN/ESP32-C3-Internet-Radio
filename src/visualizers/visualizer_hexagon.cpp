#include "visualizer_hexagon.h"
#include "../trig_tables.h"

void VisualizerHexagon::drawHexagon(Adafruit_SSD1306& display, int centerX, int centerY, int size) {
    if (size < 2) return;
    
    // Гексагон: 6 вершин, шаг 60° = каждые 2.67 индекса (16/6)
    // Индексы: 0, 3, 5, 8, 11, 13 (приблизительно)
    const int hexIndices[6] = {0, 3, 5, 8, 11, 13};
    
    for (int i = 0; i < 6; i++) {
        int idx1 = hexIndices[i];
        int idx2 = hexIndices[(i + 1) % 6];
        
        int x1 = centerX + ((size * fast_cos(idx1)) >> 8);
        int y1 = centerY + ((size * fast_sin(idx1)) >> 8);
        int x2 = centerX + ((size * fast_cos(idx2)) >> 8);
        int y2 = centerY + ((size * fast_sin(idx2)) >> 8);
        
        display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }
}

void IRAM_ATTR VisualizerHexagon::draw(Adafruit_SSD1306& display, int* bands, int bandCount) {
    // Вычисляем среднюю амплитуду
    long totalAmp = 0;
    for (int i = 0; i < bandCount; i++) {
        totalAmp += bands[i];
    }
    int avgAmp = totalAmp / bandCount;
    
    // Определяем целевое количество колец сот (1-6 колец)
    targetRings = map(avgAmp, 0, SCREEN_HEIGHT, 1, 6);
    targetRings = constrain(targetRings, 1, 6);
    
    // Плавное изменение количества колец
    if (targetRings > currentRings) {
        currentRings++;  // Быстрый рост
    } else if (targetRings < currentRings) {
        currentRings--;  // Плавное уменьшение
    }
    currentRings = constrain(currentRings, 1, 6);
    
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2;
    
    // Рисуем концентрические гексагоны (соты)
    // Размер каждого кольца увеличивается
    int baseSize = 4;  // Базовый размер
    int sizeStep = 5;  // Шаг увеличения размера
    
    for (int ring = 1; ring <= currentRings; ring++) {
        int hexSize = baseSize + (ring - 1) * sizeStep;
        drawHexagon(display, centerX, centerY, hexSize);
    }
    
    // Центральная точка - пульсирует в такт музыке
    int centerDotSize = map(avgAmp, 0, SCREEN_HEIGHT, 1, 2);
    display.fillCircle(centerX, centerY, centerDotSize, SSD1306_WHITE);
    
    // Добавляем угловые акценты при высокой интенсивности (>50%)
    if (avgAmp > SCREEN_HEIGHT / 2) {
        // Маленькие гексагоны в 4 углах
        int cornerSize = map(avgAmp, SCREEN_HEIGHT / 2, SCREEN_HEIGHT, 2, 4);
        
        drawHexagon(display, 12, 8, cornerSize);
        drawHexagon(display, SCREEN_WIDTH - 12, 8, cornerSize);
        drawHexagon(display, 12, SCREEN_HEIGHT - 8, cornerSize);
        drawHexagon(display, SCREEN_WIDTH - 12, SCREEN_HEIGHT - 8, cornerSize);
    }
}
