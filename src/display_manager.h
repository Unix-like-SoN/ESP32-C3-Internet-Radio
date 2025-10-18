#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "config.h"
#include "visualizer_manager.h"
#include <Arduino.h>

enum DisplayMode {
    INFO,
    VISUALIZER,
    AP_MODE,
    MESSAGE,
    SHUTDOWN_ANIM
};

extern int visualizerBands[16];
extern uint8_t displayRotation; // 0=Normal, 2=Flipped 180°

void setup_display();
void set_display_rotation(uint8_t rotation);
void loop_display();
void reset_inactivity_timer();
void set_display_mode_ap(String ip);
void show_message(const String& line1, const String& line2 = "", int delay_ms = 0);
void show_shutdown_progress(float progress);
void turn_off_display();

#endif // DISPLAY_MANAGER_H