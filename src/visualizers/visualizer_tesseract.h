#ifndef VISUALIZER_TESSERACT_H
#define VISUALIZER_TESSERACT_H

#include "../visualizer_base.h"

// Визуализатор: 4D Гиперкуб (Тессеракт)
// Вращение в 4D пространстве с проекцией в 2D
class VisualizerTesseract : public VisualizerBase {
private:
    // 4D вершина
    struct Vertex4D {
        float x, y, z, w;
    };
    
    // 3D вершина (после проекции 4D → 3D)
    struct Vertex3D {
        float x, y, z;
    };
    
    // 2D вершина (финальная проекция для экрана)
    struct Vertex2D {
        int x, y;
    };
    
    // Тессеракт имеет 16 вершин
    static const int VERTEX_COUNT = 16;
    Vertex4D vertices[VERTEX_COUNT];
    
    // 32 ребра (пары индексов вершин)
    static const int EDGE_COUNT = 32;
    int edges[EDGE_COUNT][2];
    
    // Углы вращения в разных плоскостях (фиксированная точка * 256)
    int16_t angleXY;  // Вращение в плоскости XY
    int16_t angleXZ;  // Вращение в плоскости XZ
    int16_t angleXW;  // Вращение в плоскости XW (4D!)
    int16_t angleYZ;  // Вращение в плоскости YZ
    
    // Скорости вращения (зависят от музыки)
    int16_t speedXY;
    int16_t speedXZ;
    int16_t speedXW;
    int16_t speedYZ;
    
    // Расстояние до камеры (для перспективы)
    float cameraDistance;
    
    // Инициализация вершин и рёбер тессеракта
    void initTesseract();
    
    // Вращение точки в 4D пространстве
    Vertex4D rotate4D(const Vertex4D& v, int16_t aXY, int16_t aXZ, int16_t aXW, int16_t aYZ);
    
    // Проекция 4D → 3D
    Vertex3D project4Dto3D(const Vertex4D& v, float wDistance);
    
    // Проекция 3D → 2D (перспективная)
    Vertex2D project3Dto2D(const Vertex3D& v, float distance);
    
    // Быстрый sin/cos (fixed-point арифметика)
    float fastSin(int16_t angle);
    float fastCos(int16_t angle);
    
public:
    VisualizerTesseract();
    void draw(Adafruit_SSD1306& display, int* bands, int bandCount) override;
    const char* getName() override { return "Tesseract"; }
};

#endif // VISUALIZER_TESSERACT_H
