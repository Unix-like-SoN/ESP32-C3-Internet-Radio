#include "visualizer_lightning.h"
#include "../config.h"

VisualizerLightning::VisualizerLightning() {
    // Инициализация
    for (int i = 0; i < MAX_BOLTS; i++) {
        bolts[i].active = false;
    }
    
    // Стартовые позиции эмиттеров (центр экрана)
    leftPointY_current = (SCREEN_HEIGHT / 2) << 8;  // * 256
    leftPointY_target = leftPointY_current;
    rightPointY_current = (SCREEN_HEIGHT / 2) << 8;
    rightPointY_target = rightPointY_current;
    
    frameCounter = 0;
}

void IRAM_ATTR VisualizerLightning::draw(Adafruit_SSD1306& display, int* bands, int bandCount) {
    frameCounter++;
    
    // === 1. ВЫЧИСЛЯЕМ ЭНЕРГИЮ МУЗЫКИ ===
    long totalEnergy = 0;
    for (int i = 0; i < bandCount; i++) {
        totalEnergy += bands[i];
    }
    int avgAmp = totalEnergy / bandCount;
    
    // Низкие и высокие частоты для движения эмиттеров
    int lowFreqEnergy = (bands[0] + bands[1] + bands[2] + bands[3]) / 4;
    int highFreqEnergy = (bands[bandCount-4] + bands[bandCount-3] + bands[bandCount-2] + bands[bandCount-1]) / 4;
    
    // === 2. ОБНОВЛЯЕМ ПОЗИЦИИ ТОЧЕК-ЭМИТТЕРОВ ===
    // Левая точка танцует под низкие частоты
    int leftOffset = map(lowFreqEnergy, 0, SCREEN_HEIGHT, -10, 10);
    leftPointY_target = ((SCREEN_HEIGHT / 2) + leftOffset) << 8;
    
    // Правая точка танцует под высокие частоты
    int rightOffset = map(highFreqEnergy, 0, SCREEN_HEIGHT, -10, 10);
    rightPointY_target = ((SCREEN_HEIGHT / 2) + rightOffset) << 8;
    
    // Плавное движение (lerp с коэффициентом 0.25)
    leftPointY_current += (leftPointY_target - leftPointY_current) >> 2;
    rightPointY_current += (rightPointY_target - rightPointY_current) >> 2;
    
    // === 3. ГЕНЕРИРУЕМ НОВЫЕ МОЛНИИ ===
    // Частота генерации зависит от энергии
    int spawnChance = map(avgAmp, 0, SCREEN_HEIGHT, 10, 2);  // 10% -> 50%
    
    if (frameCounter % spawnChance == 0 || avgAmp > SCREEN_HEIGHT * 0.7) {
        // Ищем свободный слот
        for (int i = 0; i < MAX_BOLTS; i++) {
            if (!bolts[i].active) {
                int x1 = 4;  // Левый край с отступом
                int y1 = leftPointY_current >> 8;  // / 256
                int x2 = SCREEN_WIDTH - 4;  // Правый край с отступом
                int y2 = rightPointY_current >> 8;
                
                generateBolt(i, x1, y1, x2, y2, avgAmp, false);
                
                // При высокой энергии создаем ветвление
                if (avgAmp > SCREEN_HEIGHT * 0.6 && random(0, 100) < 40) {
                    // Найдем еще один слот для ветвления
                    for (int j = i + 1; j < MAX_BOLTS; j++) {
                        if (!bolts[j].active) {
                            createBranch(i, bolts[i].segmentCount / 2, avgAmp);
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    
    // === 4. ОБНОВЛЯЕМ И РИСУЕМ АКТИВНЫЕ МОЛНИИ ===
    for (int i = 0; i < MAX_BOLTS; i++) {
        if (bolts[i].active) {
            // Затухание
            if (bolts[i].brightness > 20) {
                bolts[i].brightness -= 18;  // Быстрое затухание (~4-5 кадров)
            } else {
                bolts[i].active = false;
                continue;
            }
            
            bolts[i].lifetime++;
            
            // Рисуем
            drawBolt(display, bolts[i]);
        }
    }
    
    // === 5. РИСУЕМ ТОЧКИ-ЭМИТТЕРЫ С ПУЛЬСАЦИЕЙ ===
    int leftY = leftPointY_current >> 8;
    int rightY = rightPointY_current >> 8;
    
    // Размер точки зависит от энергии
    int dotSize = map(avgAmp, 0, SCREEN_HEIGHT, 1, 2);
    
    display.fillCircle(4, leftY, dotSize, SSD1306_WHITE);
    display.fillCircle(SCREEN_WIDTH - 4, rightY, dotSize, SSD1306_WHITE);
    
    // Дополнительное свечение при высокой энергии
    if (avgAmp > SCREEN_HEIGHT * 0.6) {
        display.drawCircle(4, leftY, dotSize + 1, SSD1306_WHITE);
        display.drawCircle(SCREEN_WIDTH - 4, rightY, dotSize + 1, SSD1306_WHITE);
    }
}

void VisualizerLightning::generateBolt(int boltIndex, int x1, int y1, int x2, int y2, int energy, bool isBranch) {
    LightningBolt& bolt = bolts[boltIndex];
    
    // Начало и конец (fixed-point)
    bolt.points[0].x = x1 << 8;
    bolt.points[0].y = y1 << 8;
    bolt.points[1].x = x2 << 8;
    bolt.points[1].y = y2 << 8;
    int segCount = 2;
    
    // Яркость зависит от энергии
    bolt.brightness = map(energy, 0, SCREEN_HEIGHT, 150, 255);
    if (isBranch) bolt.brightness = bolt.brightness * 0.7;  // Ветвления тусклее
    
    bolt.lifetime = 0;
    bolt.isBranch = isBranch;
    
    // Вычисляем начальное смещение (зависит от энергии)
    int distance = abs(x2 - x1) + abs(y2 - y1);
    int displacement = (distance / 3) * energy / SCREEN_HEIGHT;
    displacement = constrain(displacement, 8, 25);
    
    // Процедурная генерация методом midpoint displacement
    subdivideBolt(bolt.points, segCount, 0, 1, displacement, 0);
    
    // Присваиваем результат
    bolt.segmentCount = segCount;
    
    bolt.active = true;
}

void VisualizerLightning::subdivideBolt(Segment* points, int& count, int startIdx, int endIdx, int displacement, int depth) {
    // Ограничение глубины рекурсии (максимум 11 сегментов)
    if (count >= 11 || displacement < 2 || depth > 3) {
        return;
    }
    
    // Находим середину между двумя точками
    int midX = (points[startIdx].x + points[endIdx].x) / 2;
    int midY = (points[startIdx].y + points[endIdx].y) / 2;
    
    // Добавляем случайное смещение перпендикулярно линии
    int dx = points[endIdx].x - points[startIdx].x;
    int dy = points[endIdx].y - points[startIdx].y;
    
    // Перпендикуляр: (-dy, dx)
    int perpX = -dy;
    int perpY = dx;
    
    // Нормализуем и применяем displacement
    int len = abs(perpX) + abs(perpY);  // Приблизительная длина (Manhattan)
    if (len > 0) {
        perpX = (perpX * displacement * 256) / len;
        perpY = (perpY * displacement * 256) / len;
    }
    
    // Случайное направление
    int randomOffset = random(-100, 100);
    midX += (perpX * randomOffset) / 100;
    midY += (perpY * randomOffset) / 100;
    
    // Вставляем новую точку в массив (сдвигаем элементы)
    for (int i = count; i > endIdx; i--) {
        points[i] = points[i - 1];
    }
    
    points[endIdx].x = midX;
    points[endIdx].y = midY;
    count++;
    
    // Рекурсивно делим левую и правую половины
    subdivideBolt(points, count, startIdx, endIdx, displacement / 2, depth + 1);
    subdivideBolt(points, count, endIdx, endIdx + 1, displacement / 2, depth + 1);
}

void VisualizerLightning::drawBolt(Adafruit_SSD1306& display, LightningBolt& bolt) {
    // Рисуем все сегменты молнии
    for (int i = 0; i < bolt.segmentCount - 1; i++) {
        int x1 = bolt.points[i].x >> 8;      // / 256
        int y1 = bolt.points[i].y >> 8;
        int x2 = bolt.points[i + 1].x >> 8;
        int y2 = bolt.points[i + 1].y >> 8;
        
        // Проверка границ
        if (x1 < 0 || x1 >= SCREEN_WIDTH || y1 < 0 || y1 >= SCREEN_HEIGHT ||
            x2 < 0 || x2 >= SCREEN_WIDTH || y2 < 0 || y2 >= SCREEN_HEIGHT) {
            continue;
        }
        
        // Основная линия
        display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
        
        // Эффект свечения для ярких молний
        if (bolt.brightness > 180 && !bolt.isBranch) {
            // Дополнительные линии рядом для "толщины"
            if (y1 + 1 < SCREEN_HEIGHT) {
                display.drawLine(x1, y1 + 1, x2, y2 + 1, SSD1306_WHITE);
            }
            if (y1 > 0) {
                display.drawLine(x1, y1 - 1, x2, y2 - 1, SSD1306_WHITE);
            }
        }
        
        // Случайные искры вдоль молнии (каждый 3-й сегмент)
        if (bolt.brightness > 100 && i % 3 == 0 && random(0, 100) < 60) {
            int sparkX = x1 + random(-2, 3);
            int sparkY = y1 + random(-2, 3);
            
            if (sparkX >= 0 && sparkX < SCREEN_WIDTH && sparkY >= 0 && sparkY < SCREEN_HEIGHT) {
                display.drawPixel(sparkX, sparkY, SSD1306_WHITE);
            }
        }
    }
}

void VisualizerLightning::createBranch(int sourceBoltIdx, int segmentIdx, int energy) {
    LightningBolt& source = bolts[sourceBoltIdx];
    
    if (segmentIdx >= source.segmentCount - 1) return;
    
    // Найти свободный слот для ветвления
    for (int i = 0; i < MAX_BOLTS; i++) {
        if (!bolts[i].active) {
            // Стартовая точка - середина основной молнии
            int startX = source.points[segmentIdx].x >> 8;
            int startY = source.points[segmentIdx].y >> 8;
            
            // Конечная точка - под углом 30-60° от основной
            int dx = (source.points[source.segmentCount - 1].x - source.points[0].x) >> 8;
            int dy = (source.points[source.segmentCount - 1].y - source.points[0].y) >> 8;
            
            // Длина ветвления - 40-60% от основной
            int branchLength = (abs(dx) + abs(dy)) * random(40, 60) / 100;
            
            // Случайное направление (вверх или вниз)
            int angle = random(0, 2) == 0 ? 1 : -1;
            
            int endX = startX + (dx * branchLength) / (abs(dx) + abs(dy));
            int endY = startY + angle * branchLength / 2;
            
            // Границы экрана
            endX = constrain(endX, 5, SCREEN_WIDTH - 5);
            endY = constrain(endY, 1, SCREEN_HEIGHT - 1);
            
            generateBolt(i, startX, startY, endX, endY, energy * 0.7, true);
            break;
        }
    }
}
