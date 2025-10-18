#ifndef VISUALIZER_MANAGER_H
#define VISUALIZER_MANAGER_H

#include "visualizer_base.h"
#include "visualizer_styles.h"
#include <Adafruit_SSD1306.h>

// Менеджер визуализаторов - управляет всеми стилями
class VisualizerManager {
private:
    VisualizerBase* visualizers[VISUALIZER_STYLE_COUNT];
    VisualizerStyle currentStyle;
    
    // Lazy initialization для экономии памяти
    VisualizerBase* getVisualizer(VisualizerStyle style);
    
public:
    VisualizerManager();
    ~VisualizerManager();
    
    // Установить текущий стиль
    void setStyle(VisualizerStyle style);
    
    // Получить текущий стиль
    VisualizerStyle getStyle() const { return currentStyle; }
    
    // Отрисовать текущий визуализатор
    void draw(Adafruit_SSD1306& display, int* bands, int bandCount);
    
    // Получить название текущего стиля
    const char* getCurrentStyleName();
    
    // Получить название стиля по ID
    static const char* getStyleName(VisualizerStyle style);
};

// Глобальный экземпляр менеджера
extern VisualizerManager visualizerManager;

#endif // VISUALIZER_MANAGER_H
