#ifndef VISUALIZER_LIGHTNING_H
#define VISUALIZER_LIGHTNING_H

#include "../visualizer_base.h"

// Визуализатор: Молнии между двумя точками
// Процедурная генерация с реакцией на музыку
class VisualizerLightning : public VisualizerBase {
private:
    // Сегмент молнии (вершина)
    struct Segment {
        int16_t x;  // Fixed-point (* 256)
        int16_t y;  // Fixed-point (* 256)
    };
    
    // Молния
    struct LightningBolt {
        Segment points[12];  // Максимум 12 точек на молнию
        uint8_t segmentCount;
        uint8_t brightness;  // Яркость для затухания
        uint8_t lifetime;    // Время жизни (кадры)
        bool active;
        bool isBranch;       // Ветвление или основная молния
    };
    
    static const int MAX_BOLTS = 8;
    LightningBolt bolts[MAX_BOLTS];
    
    // Позиции точек-эмиттеров (fixed-point)
    int16_t leftPointY_target;
    int16_t leftPointY_current;
    int16_t rightPointY_target;
    int16_t rightPointY_current;
    
    uint8_t frameCounter;
    
    // Генерация молнии между двумя точками
    void generateBolt(int boltIndex, int x1, int y1, int x2, int y2, int energy, bool isBranch = false);
    
    // Рекурсивное построение молнии (midpoint displacement)
    void subdivideBolt(Segment* points, int& count, int startIdx, int endIdx, int displacement, int depth);
    
    // Отрисовка одной молнии
    void drawBolt(Adafruit_SSD1306& display, LightningBolt& bolt);
    
    // Создать ответвление от основной молнии
    void createBranch(int sourceBoltIdx, int segmentIdx, int energy);
    
public:
    VisualizerLightning();
    void draw(Adafruit_SSD1306& display, int* bands, int bandCount) override;
    const char* getName() override { return "Lightning"; }
};

#endif // VISUALIZER_LIGHTNING_H
