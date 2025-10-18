#include "visualizer_stars.h"
#include "../config.h"

VisualizerStars::VisualizerStars() {
    // Инициализируем все звезды как неактивные
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].active = false;
    }
}

void IRAM_ATTR VisualizerStars::draw(Adafruit_SSD1306& display, int* bands, int bandCount) {
    frameCount++;
    
    // Вычисляем общую энергию
    long totalEnergy = 0;
    for (int i = 0; i < bandCount; i++) {
        totalEnergy += bands[i];
    }
    int avgAmp = totalEnergy / bandCount;
    
    // ОБЫЧНЫЕ падающие звезды (всегда есть, 1-2 звезды)
    int newStarsCount = map(avgAmp, 0, SCREEN_HEIGHT, 0, 2);
    newStarsCount = constrain(newStarsCount, 0, 2);
    
    for (int i = 0; i < newStarsCount; i++) {
        for (int j = 0; j < MAX_STARS; j++) {
            if (!stars[j].active) {
                stars[j].x = random(0, SCREEN_WIDTH) << 8;  // * 256
                stars[j].y = 0;
                
                // Скорость: 128-384 (0.5-1.5 * 256)
                int speedScale = 128 + ((avgAmp * 256) / SCREEN_HEIGHT);
                stars[j].speedY = speedScale;
                
                // Горизонтальное смещение: 26-77 (0.1-0.3 * 256)
                int hSpeed = random(26, 77);
                stars[j].speedX = (random(0, 2) == 0) ? hSpeed : -hSpeed;
                
                stars[j].brightness = 180;
                stars[j].trailLength = 3;
                
                stars[j].active = true;
                break;
            }
        }
    }
    
    // КОМЕТЫ (редкие, быстрые, длинные) - только при высокой интенсивности!
    int threshold = (SCREEN_HEIGHT * 6) / 10;  // 0.6 * SCREEN_HEIGHT
    if (avgAmp > threshold && random(0, 10) < 2) {
        for (int j = 0; j < MAX_STARS; j++) {
            if (!stars[j].active) {
                stars[j].x = random(0, SCREEN_WIDTH) << 8;  // * 256
                stars[j].y = 0;
                
                // КОМЕТА: 640-1152 (2.5-4.5 * 256)
                int cometSpeed = 640 + ((avgAmp * 512) / SCREEN_HEIGHT);
                stars[j].speedY = cometSpeed;
                
                // Большое горизонтальное: 128-256 (0.5-1.0 * 256)
                int hSpeed = random(128, 256);
                stars[j].speedX = (random(0, 2) == 0) ? hSpeed : -hSpeed;
                
                stars[j].brightness = 255;
                stars[j].trailLength = 8;
                
                stars[j].active = true;
                break;
            }
        }
    }
    
    // Обновляем и рисуем все активные звезды
    for (int i = 0; i < MAX_STARS; i++) {
        if (stars[i].active) {
            // Движение
            stars[i].x += stars[i].speedX;
            stars[i].y += stars[i].speedY;
            
            // Деактивируем если вышла за пределы (fixed-point)
            int yPixel = stars[i].y >> 8;  // / 256
            int xPixel = stars[i].x >> 8;
            
            if (yPixel >= SCREEN_HEIGHT || xPixel < 0 || xPixel >= SCREEN_WIDTH) {
                stars[i].active = false;
                continue;
            }
            
            // Рисуем звезду с хвостом
            int x = xPixel;
            int y = yPixel;
            
            // Голова звезды (яркая точка)
            display.drawPixel(x, y, SSD1306_WHITE);
            
            // Хвост (след за звездой, направлен вверх)
            for (int t = 1; t <= stars[i].trailLength; t++) {
                int trailY = y - t;
                int trailX = x - ((stars[i].speedX * t) >> 9);  // Делим на 512 (0.5)
                
                if (trailY >= 0 && trailY < SCREEN_HEIGHT && 
                    trailX >= 0 && trailX < SCREEN_WIDTH) {
                    // Хвост тускнеет к концу
                    if (t <= stars[i].trailLength / 2 || (frameCount + i) % 2 == 0) {
                        display.drawPixel(trailX, trailY, SSD1306_WHITE);
                    }
                }
            }
            
            // Свечение вокруг яркой звезды
            if (stars[i].brightness > 200) {
                display.drawPixel(x+1, y, SSD1306_WHITE);
                display.drawPixel(x-1, y, SSD1306_WHITE);
            }
        }
    }
    
}
