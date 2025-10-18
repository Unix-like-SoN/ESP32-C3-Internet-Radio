#include "visualizer_manager.h"
#include "visualizers/visualizer_bars.h"
#include "visualizers/visualizer_wave.h"
#include "visualizers/visualizer_circle.h"
#include "visualizers/visualizer_hexagon.h"
#include "visualizers/visualizer_stars.h"
#include "visualizers/visualizer_mirror.h"
#include "visualizers/visualizer_lightning.h"
#include "visualizers/visualizer_tesseract.h"
#include "visualizers/visualizer_plasma.h"

// Глобальный экземпляр
VisualizerManager visualizerManager;

VisualizerManager::VisualizerManager() : currentStyle(STYLE_BARS) {
    // Инициализируем все указатели как nullptr (lazy loading)
    for (int i = 0; i < VISUALIZER_STYLE_COUNT; i++) {
        visualizers[i] = nullptr;
    }
}

VisualizerManager::~VisualizerManager() {
    // Освобождаем все созданные визуализаторы
    for (int i = 0; i < VISUALIZER_STYLE_COUNT; i++) {
        if (visualizers[i]) {
            delete visualizers[i];
            visualizers[i] = nullptr;
        }
    }
}

VisualizerBase* VisualizerManager::getVisualizer(VisualizerStyle style) {
    // Lazy initialization - создаем визуализатор только при первом обращении
    if (!visualizers[style]) {
        switch(style) {
            case STYLE_BARS:
                visualizers[style] = new VisualizerBars();
                break;
            case STYLE_WAVE:
                visualizers[style] = new VisualizerWave();
                break;
            case STYLE_CIRCLE:
                visualizers[style] = new VisualizerCircle();
                break;
            case STYLE_HEXAGON:
                visualizers[style] = new VisualizerHexagon();
                break;
            case STYLE_STARS:
                visualizers[style] = new VisualizerStars();
                break;
            case STYLE_MIRROR:
                visualizers[style] = new VisualizerMirror();
                break;
            case STYLE_LIGHTNING:
                visualizers[style] = new VisualizerLightning();
                break;
            case STYLE_TESSERACT:
                visualizers[style] = new VisualizerTesseract();
                break;
            case STYLE_PLASMA:
                visualizers[style] = new VisualizerPlasma();
                break;
        }
    }
    return visualizers[style];
}

void VisualizerManager::setStyle(VisualizerStyle style) {
    if (style < VISUALIZER_STYLE_COUNT) {
        currentStyle = style;
    }
}

void VisualizerManager::draw(Adafruit_SSD1306& display, int* bands, int bandCount) {
    VisualizerBase* viz = getVisualizer(currentStyle);
    if (viz) {
        viz->draw(display, bands, bandCount);
    }
}

const char* VisualizerManager::getCurrentStyleName() {
    VisualizerBase* viz = getVisualizer(currentStyle);
    return viz ? viz->getName() : "Unknown";
}

const char* VisualizerManager::getStyleName(VisualizerStyle style) {
    switch(style) {
        case STYLE_BARS: return "Bars";
        case STYLE_WAVE: return "Wave";
        case STYLE_CIRCLE: return "Circle";
        case STYLE_HEXAGON: return "Hexagon";
        case STYLE_STARS: return "Stars";
        case STYLE_MIRROR: return "Mirror";
        case STYLE_LIGHTNING: return "Lightning";
        case STYLE_TESSERACT: return "Tesseract";
        case STYLE_PLASMA: return "Plasma";
        default: return "Unknown";
    }
}
