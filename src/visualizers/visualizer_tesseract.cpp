#include "visualizer_tesseract.h"
#include "../config.h"
#include <math.h>

VisualizerTesseract::VisualizerTesseract() {
    // Инициализация углов
    angleXY = 0;
    angleXZ = 0;
    angleXW = 0;
    angleYZ = 0;
    
    // Начальные скорости
    speedXY = 180;  // ~0.7 градуса/кадр
    speedXZ = 120;
    speedXW = 90;
    speedYZ = 150;
    
    cameraDistance = 4.0f;
    
    initTesseract();
}

void VisualizerTesseract::initTesseract() {
    // Создаём 16 вершин гиперкуба (все комбинации ±1 для x, y, z, w)
    for (int i = 0; i < VERTEX_COUNT; i++) {
        // Извлекаем биты для определения знака каждой координаты
        vertices[i].x = (i & 1) ? 1.0f : -1.0f;
        vertices[i].y = (i & 2) ? 1.0f : -1.0f;
        vertices[i].z = (i & 4) ? 1.0f : -1.0f;
        vertices[i].w = (i & 8) ? 1.0f : -1.0f;
    }
    
    // Создаём 32 ребра
    // Ребро соединяет две вершины, которые отличаются ровно в одном бите
    int edgeIndex = 0;
    for (int i = 0; i < VERTEX_COUNT; i++) {
        for (int j = i + 1; j < VERTEX_COUNT; j++) {
            // XOR показывает, в скольких битах вершины отличаются
            int diff = i ^ j;
            // Подсчитываем количество единичных битов (должно быть ровно 1)
            int bitCount = 0;
            int temp = diff;
            while (temp) {
                bitCount += temp & 1;
                temp >>= 1;
            }
            
            if (bitCount == 1) {
                // Соседние вершины - создаём ребро
                edges[edgeIndex][0] = i;
                edges[edgeIndex][1] = j;
                edgeIndex++;
                if (edgeIndex >= EDGE_COUNT) break;
            }
        }
        if (edgeIndex >= EDGE_COUNT) break;
    }
}

float VisualizerTesseract::fastSin(int16_t angle) {
    // angle в диапазоне 0-65535 (0-360°)
    float rad = (angle / 32768.0f) * 3.14159265f;
    return sin(rad);
}

float VisualizerTesseract::fastCos(int16_t angle) {
    float rad = (angle / 32768.0f) * 3.14159265f;
    return cos(rad);
}

VisualizerTesseract::Vertex4D VisualizerTesseract::rotate4D(const Vertex4D& v, int16_t aXY, int16_t aXZ, int16_t aXW, int16_t aYZ) {
    Vertex4D result = v;
    
    // Вращение в плоскости XY
    if (aXY != 0) {
        float cosA = fastCos(aXY);
        float sinA = fastSin(aXY);
        float newX = result.x * cosA - result.y * sinA;
        float newY = result.x * sinA + result.y * cosA;
        result.x = newX;
        result.y = newY;
    }
    
    // Вращение в плоскости XZ
    if (aXZ != 0) {
        float cosA = fastCos(aXZ);
        float sinA = fastSin(aXZ);
        float newX = result.x * cosA - result.z * sinA;
        float newZ = result.x * sinA + result.z * cosA;
        result.x = newX;
        result.z = newZ;
    }
    
    // Вращение в плоскости XW (4D!)
    if (aXW != 0) {
        float cosA = fastCos(aXW);
        float sinA = fastSin(aXW);
        float newX = result.x * cosA - result.w * sinA;
        float newW = result.x * sinA + result.w * cosA;
        result.x = newX;
        result.w = newW;
    }
    
    // Вращение в плоскости YZ
    if (aYZ != 0) {
        float cosA = fastCos(aYZ);
        float sinA = fastSin(aYZ);
        float newY = result.y * cosA - result.z * sinA;
        float newZ = result.y * sinA + result.z * cosA;
        result.y = newY;
        result.z = newZ;
    }
    
    return result;
}

VisualizerTesseract::Vertex3D VisualizerTesseract::project4Dto3D(const Vertex4D& v, float wDistance) {
    // Стереографическая проекция из 4D в 3D
    // Точка наблюдения находится на расстоянии wDistance по оси W
    Vertex3D result;
    
    float scale = wDistance / (wDistance - v.w);
    result.x = v.x * scale;
    result.y = v.y * scale;
    result.z = v.z * scale;
    
    return result;
}

VisualizerTesseract::Vertex2D VisualizerTesseract::project3Dto2D(const Vertex3D& v, float distance) {
    // Перспективная проекция из 3D в 2D
    Vertex2D result;
    
    // Защита от деления на 0 и отрицательного масштаба
    float denominator = distance + v.z;
    if (denominator < 0.1f) denominator = 0.1f;  // Минимальный знаменатель
    
    float scale = distance / denominator;
    
    // Ограничиваем масштаб чтобы избежать слишком больших значений
    scale = constrain(scale, 0.1f, 5.0f);
    
    // Масштабируем и центрируем
    result.x = (int)(SCREEN_WIDTH / 2 + v.x * scale * 10);
    result.y = (int)(SCREEN_HEIGHT / 2 + v.y * scale * 10);
    
    return result;
}

void IRAM_ATTR VisualizerTesseract::draw(Adafruit_SSD1306& display, int* bands, int bandCount) {
    // === 1. ВЫЧИСЛЯЕМ ЭНЕРГИЮ МУЗЫКИ (только для эффектов) ===
    long totalEnergy = 0;
    for (int i = 0; i < bandCount; i++) {
        totalEnergy += bands[i];
    }
    int avgAmp = totalEnergy / bandCount;
    
    // === 2. ПЛАВНОЕ ВРАЩЕНИЕ (ПОСТОЯННЫЕ СКОРОСТИ) ===
    // Каждая плоскость вращается с своей скоростью
    // Не зависит от музыки - плавное плавание
    angleXY += speedXY;  // ~0.7°/кадр
    angleXZ += speedXZ;  // ~0.5°/кадр
    angleXW += speedXW;  // ~0.35°/кадр (4D!)
    angleYZ += speedYZ;  // ~0.6°/кадр
    
    // === 3. ВРАЩАЕМ И ПРОЕЦИРУЕМ ВЕРШИНЫ ===
    Vertex2D projectedVertices[VERTEX_COUNT];
    
    // Постоянное расстояние для 4D→3D проекции
    float wDistance = 3.0f;  // Оптимальный размер
    
    for (int i = 0; i < VERTEX_COUNT; i++) {
        // Вращение в 4D
        Vertex4D rotated = rotate4D(vertices[i], angleXY, angleXZ, angleXW, angleYZ);
        
        // Проекция 4D → 3D
        Vertex3D projected3D = project4Dto3D(rotated, wDistance);
        
        // Проекция 3D → 2D
        projectedVertices[i] = project3Dto2D(projected3D, cameraDistance);
    }
    
    // === 4. РИСУЕМ РЁБРА ===
    // drawLine сам обрежет линии по границам экрана (clipping)
    for (int i = 0; i < EDGE_COUNT; i++) {
        int v1 = edges[i][0];
        int v2 = edges[i][1];
        
        Vertex2D p1 = projectedVertices[v1];
        Vertex2D p2 = projectedVertices[v2];
        
        // Проверяем что линия не слишком далеко за пределами (оптимизация)
        // Рисуем если хотя бы часть линии может быть видна
        if (!((p1.x < -20 && p2.x < -20) || (p1.x > SCREEN_WIDTH + 20 && p2.x > SCREEN_WIDTH + 20) ||
              (p1.y < -20 && p2.y < -20) || (p1.y > SCREEN_HEIGHT + 20 && p2.y > SCREEN_HEIGHT + 20))) {
            display.drawLine(p1.x, p1.y, p2.x, p2.y, SSD1306_WHITE);
        }
    }
    
    // === 5. РИСУЕМ ВЕРШИНЫ (УЗЛЫ) ===
    // При высокой энергии подсвечиваем вершины
    if (avgAmp > SCREEN_HEIGHT * 0.5) {
        for (int i = 0; i < VERTEX_COUNT; i++) {
            Vertex2D p = projectedVertices[i];
            if (p.x >= 1 && p.x < SCREEN_WIDTH-1 && p.y >= 1 && p.y < SCREEN_HEIGHT-1) {
                // Крестик на вершине
                display.drawPixel(p.x, p.y, SSD1306_WHITE);
                display.drawPixel(p.x-1, p.y, SSD1306_WHITE);
                display.drawPixel(p.x+1, p.y, SSD1306_WHITE);
                display.drawPixel(p.x, p.y-1, SSD1306_WHITE);
                display.drawPixel(p.x, p.y+1, SSD1306_WHITE);
            }
        }
    }
    
    // === 6. ДОПОЛНИТЕЛЬНЫЙ ЭФФЕКТ: "ГЛИТЧ" ПРИ ПИКАХ ===
    // При очень высокой энергии - случайное смещение линий
    if (avgAmp > SCREEN_HEIGHT * 0.8 && random(0, 100) < 20) {
        // Рисуем несколько случайных рёбер со смещением (глитч эффект)
        for (int i = 0; i < 3; i++) {
            int edgeIdx = random(0, EDGE_COUNT);
            int v1 = edges[edgeIdx][0];
            int v2 = edges[edgeIdx][1];
            
            Vertex2D p1 = projectedVertices[v1];
            Vertex2D p2 = projectedVertices[v2];
            
            int offsetX = random(-2, 3);
            int offsetY = random(-2, 3);
            
            p1.x += offsetX;
            p1.y += offsetY;
            p2.x += offsetX;
            p2.y += offsetY;
            
            // Глитч может выходить за пределы - это часть эффекта
            display.drawLine(p1.x, p1.y, p2.x, p2.y, SSD1306_WHITE);
        }
    }
}
