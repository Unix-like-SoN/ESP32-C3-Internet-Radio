#include "visualizer_plasma.h"
#include "../config.h"
#include <math.h>

// Lookup таблица для sin (256 значений, диапазон -127...127)
const int8_t VisualizerPlasma::sinTable[256] = {
    0, 3, 6, 9, 12, 15, 18, 21, 24, 28, 31, 34, 37, 40, 43, 46,
    48, 51, 54, 57, 60, 63, 65, 68, 71, 73, 76, 78, 81, 83, 85, 88,
    90, 92, 94, 96, 98, 100, 102, 104, 106, 108, 109, 111, 112, 114, 115, 117,
    118, 119, 120, 121, 122, 123, 124, 124, 125, 126, 126, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 126, 126, 125, 124, 124, 123, 122, 121, 120, 119,
    118, 117, 115, 114, 112, 111, 109, 108, 106, 104, 102, 100, 98, 96, 94, 92,
    90, 88, 85, 83, 81, 78, 76, 73, 71, 68, 65, 63, 60, 57, 54, 51,
    48, 46, 43, 40, 37, 34, 31, 28, 24, 21, 18, 15, 12, 9, 6, 3,
    0, -3, -6, -9, -12, -15, -18, -21, -24, -28, -31, -34, -37, -40, -43, -46,
    -48, -51, -54, -57, -60, -63, -65, -68, -71, -73, -76, -78, -81, -83, -85, -88,
    -90, -92, -94, -96, -98, -100, -102, -104, -106, -108, -109, -111, -112, -114, -115, -117,
    -118, -119, -120, -121, -122, -123, -124, -124, -125, -126, -126, -127, -127, -127, -127, -127,
    -127, -127, -127, -127, -127, -127, -126, -126, -125, -124, -124, -123, -122, -121, -120, -119,
    -118, -117, -115, -114, -112, -111, -109, -108, -106, -104, -102, -100, -98, -96, -94, -92,
    -90, -88, -85, -83, -81, -78, -76, -73, -71, -68, -65, -63, -60, -57, -54, -51,
    -48, -46, -43, -40, -37, -34, -31, -28, -24, -21, -18, -15, -12, -9, -6, -3
};

VisualizerPlasma::VisualizerPlasma() {
    // Инициализация фаз
    phase1 = 0;
    phase2 = 0;
    phase3 = 0;
    phase4 = 0;
    
    // Начальные скорости (плавное движение)
    speed1 = 4;   // Горизонтальная волна
    speed2 = 3;   // Вертикальная волна
    speed3 = 5;   // Диагональная волна
    speed4 = 2;   // Радиальная волна
    
    // Центр экрана для радиальной волны
    centerX = SCREEN_WIDTH / 2;
    centerY = SCREEN_HEIGHT / 2;
    
    // Средний порог
    threshold = 0;
}

int8_t VisualizerPlasma::fastSin(uint8_t angle) {
    // angle в диапазоне 0-255 соответствует 0-360°
    return sinTable[angle];
}

int16_t VisualizerPlasma::distance(int x1, int y1, int x2, int y2) {
    // Manhattan distance (быстрее чем Euclidean)
    return abs(x1 - x2) + abs(y1 - y2);
}

int16_t VisualizerPlasma::calculatePlasma(int x, int y) {
    // Интерференция 4 волн
    
    // Волна 1: Горизонтальная (зависит от X)
    int16_t v1 = fastSin((x * 4 + phase1) & 0xFF);
    
    // Волна 2: Вертикальная (зависит от Y)
    int16_t v2 = fastSin((y * 8 + phase2) & 0xFF);
    
    // Волна 3: Диагональная (зависит от X+Y)
    int16_t v3 = fastSin(((x + y) * 3 + phase3) & 0xFF);
    
    // Волна 4: Радиальная (зависит от расстояния до центра)
    int16_t dist = distance(x, y, centerX, centerY);
    int16_t v4 = fastSin((dist * 4 + phase4) & 0xFF);
    
    // Сумма всех волн
    return v1 + v2 + v3 + v4;
}

void IRAM_ATTR VisualizerPlasma::draw(Adafruit_SSD1306& display, int* bands, int bandCount) {
    // === 1. ВЫЧИСЛЯЕМ ЭНЕРГИЮ МУЗЫКИ ===
    long totalEnergy = 0;
    for (int i = 0; i < bandCount; i++) {
        totalEnergy += bands[i];
    }
    int avgAmp = totalEnergy / bandCount;
    
    // Разбиваем на частоты
    int lowFreq = (bands[0] + bands[1] + bands[2] + bands[3]) / 4;
    int midFreq = (bands[bandCount/2 - 2] + bands[bandCount/2 - 1] + bands[bandCount/2] + bands[bandCount/2 + 1]) / 4;
    int highFreq = (bands[bandCount-4] + bands[bandCount-3] + bands[bandCount-2] + bands[bandCount-1]) / 4;
    
    // === 2. ОБНОВЛЯЕМ СКОРОСТИ ВОЛН ОТ МУЗЫКИ ===
    // Низкие частоты → горизонтальная волна
    speed1 = map(lowFreq, 0, SCREEN_HEIGHT, 2, 8);
    
    // Средние частоты → вертикальная волна
    speed2 = map(midFreq, 0, SCREEN_HEIGHT, 1, 6);
    
    // Высокие частоты → диагональная волна
    speed3 = map(highFreq, 0, SCREEN_HEIGHT, 3, 10);
    
    // Общая энергия → радиальная волна
    speed4 = map(avgAmp, 0, SCREEN_HEIGHT, 1, 5);
    
    // Обновляем фазы
    phase1 += speed1;
    phase2 += speed2;
    phase3 += speed3;
    phase4 += speed4;
    
    // === 3. ПОРОГ ЗАВИСИТ ОТ ЭНЕРГИИ ===
    // Больше энергия → ниже порог → больше белых пикселей
    threshold = map(avgAmp, 0, SCREEN_HEIGHT, 100, -200);
    
    // === 4. ДВИЖЕНИЕ РАДИАЛЬНОГО ЦЕНТРА ===
    // Центр слегка плавает в такт басам
    int offsetX = map(lowFreq, 0, SCREEN_HEIGHT, -8, 8);
    int offsetY = map(midFreq, 0, SCREEN_HEIGHT, -4, 4);
    
    centerX = (SCREEN_WIDTH / 2) + offsetX;
    centerY = (SCREEN_HEIGHT / 2) + offsetY;
    
    // Ограничиваем центр
    centerX = constrain(centerX, 10, SCREEN_WIDTH - 10);
    centerY = constrain(centerY, 4, SCREEN_HEIGHT - 4);
    
    // === 5. РИСУЕМ ПЛАЗМУ ===
    // Оптимизация: можем пропускать каждый 2-й пиксель при низкой энергии
    int step = (avgAmp < SCREEN_HEIGHT / 3) ? 2 : 1;
    
    for (int y = 0; y < SCREEN_HEIGHT; y += step) {
        for (int x = 0; x < SCREEN_WIDTH; x += step) {
            int16_t value = calculatePlasma(x, y);
            
            // Рисуем если значение выше порога
            if (value > threshold) {
                display.drawPixel(x, y, SSD1306_WHITE);
                
                // Если step=2, заполняем соседний пиксель для плотности
                if (step == 2 && x + 1 < SCREEN_WIDTH) {
                    display.drawPixel(x + 1, y, SSD1306_WHITE);
                }
            }
        }
    }
    
    // === 6. ЭФФЕКТЫ ПРИ ПИКАХ ===
    if (avgAmp > SCREEN_HEIGHT * 0.75) {
        // При пике - пульсация центра (рисуем кольцо)
        int pulseRadius = map(avgAmp, SCREEN_HEIGHT * 0.75, SCREEN_HEIGHT, 5, 15);
        
        // Рисуем окружность в центре
        for (int angle = 0; angle < 256; angle += 16) {
            int px = centerX + (fastSin(angle) * pulseRadius) / 127;
            int py = centerY + (fastSin((angle + 64) & 0xFF) * pulseRadius / 2) / 127;
            
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                display.drawPixel(px, py, SSD1306_WHITE);
            }
        }
    }
    
    // === 7. ДОПОЛНИТЕЛЬНЫЙ СЛОЙ ПРИ ВЫСОКОЙ ЭНЕРГИИ ===
    // Быстрая рябь поверх основной плазмы
    if (avgAmp > SCREEN_HEIGHT * 0.6) {
        for (int y = 0; y < SCREEN_HEIGHT; y += 4) {
            for (int x = 0; x < SCREEN_WIDTH; x += 4) {
                // Дополнительная быстрая волна
                int16_t fastWave = fastSin(((x * 2 + y * 2 + phase1 * 3) & 0xFF));
                
                if (fastWave > 60) {
                    display.drawPixel(x, y, SSD1306_WHITE);
                }
            }
        }
    }
}
