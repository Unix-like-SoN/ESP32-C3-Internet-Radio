#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "config.h"
#include "display_manager.h"
#include "audio_manager.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DisplayMode currentDisplayMode = INFO;
unsigned long lastInteractionTime = 0;
const unsigned long inactivityTimeout = DISPLAY_INACTIVITY_TIMEOUT;

int visualizerBands[16] = {0};

String ap_ip_address = "";
String message_line1, message_line2;
float shutdownProgress = 0.0;

// IP Display with pause
String ip_display_address = "";
unsigned long ip_display_start_time = 0;
unsigned long ip_display_duration = 2000;
bool ip_display_paused = false;

// Для плавной шкалы громкости
float displayVolume = 0.0;

void draw_info_screen();
void draw_visualizer();
void draw_ap_mode_screen();
void draw_message_screen();
void draw_ip_display_screen();
void draw_shutdown_screen();

// ⚠️ Статус инициализации OLED
static bool displayInitialized = false;
static unsigned long lastDisplayInitAttempt = 0;

void setup_display() {
    Wire.begin(OLED_SDA, OLED_SCL);
    
    // ⚡ Fast Mode I2C (I2C_FAST_MODE_FREQ) для уменьшения времени блокировки
    // Стандартный режим: 100 kHz (~10-15ms на кадр)
    // Fast Mode: 400 kHz (~3-5ms на кадр)
    Wire.setClock(I2C_FAST_MODE_FREQ);
    Serial.printf("⚡ I2C Fast Mode: %d Hz\n", I2C_FAST_MODE_FREQ);
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.printf("⚠️ ОЛЕД не найден! Повторные попытки каждые %d мс...\n", I2C_RETRY_INTERVAL);
        displayInitialized = false;
        lastDisplayInitAttempt = millis();
        return;
    }
    
    displayInitialized = true;
    Serial.printf("✅ OLED инициализирован успешно (0x3C, %dkHz)\n", I2C_FAST_MODE_FREQ / 1000);
    
    display.setRotation(displayRotation); // Применяем сохраненную настройку
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.println("Radio Ready!");
    display.display();
    delay(DISPLAY_INIT_DELAY);
    reset_inactivity_timer();
}

// ⚠️ Попытка переинициализации OLED (вызывается из loop)
void try_reinit_display() {
    if (displayInitialized) return; // Уже инициализирован
    
    unsigned long now = millis();
    if (now - lastDisplayInitAttempt < I2C_RETRY_INTERVAL) return; // Еще не прошло I2C_RETRY_INTERVAL
    
    lastDisplayInitAttempt = now;
    Serial.println("🔄 Попытка переинициализации OLED...");
    
    // 🛡️ GRACEFUL CLEANUP: очищаем I2C шину перед реинициализацией
    // Предотвращаем конфликты с предыдущими командами
    Wire.end();
    delay(100); // Даем время на очистку I2Cбуферов
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(I2C_FAST_MODE_FREQ);
    
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        displayInitialized = true;
        Serial.println("✅ OLED инициализирован после повторной попытки!");
        display.setRotation(displayRotation);
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.println("Display recovered!");
        display.display();
        reset_inactivity_timer();
    }
}

void IRAM_ATTR loop_display() {
    // ⚠️ Проверка инициализации и попытка восстановления
    try_reinit_display();
    if (!displayInitialized) return; // Дисплей не готов, пропускаем отрисовку
    
    // Плавное изменение громкости для анимации
    if (abs(displayVolume - volume) > 0.01) {
        displayVolume += (volume - displayVolume) * 0.2;
    } else {
        displayVolume = volume;
    }

    switch (currentDisplayMode) {
        case INFO:
            if (millis() - lastInteractionTime > inactivityTimeout) currentDisplayMode = VISUALIZER;
            draw_info_screen();
            break;
        case VISUALIZER:
            draw_visualizer();
            break;
        case AP_MODE:
            draw_ap_mode_screen();
            break;
        case MESSAGE:
            draw_message_screen();
            break;
        case IP_DISPLAY:
            // Auto-transition to INFO after timeout (if not paused)
            if (!ip_display_paused && millis() - ip_display_start_time > ip_display_duration) {
                currentDisplayMode = INFO;
                reset_inactivity_timer();
            }
            draw_ip_display_screen();
            break;
        case SHUTDOWN_ANIM:
            draw_shutdown_screen();
            break;
    }
}

void reset_inactivity_timer() {
    lastInteractionTime = millis();
    if (currentDisplayMode != INFO && currentDisplayMode != AP_MODE && currentDisplayMode != IP_DISPLAY) {
        currentDisplayMode = INFO;
        display.clearDisplay();
    }
}

void set_display_mode_ap(String ip) {
    currentDisplayMode = AP_MODE;
    ap_ip_address = ip;
    loop_display(); // Обновить экран сразу
}

void show_shutdown_progress(float progress) {
    currentDisplayMode = SHUTDOWN_ANIM;
    shutdownProgress = progress;
}

void turn_off_display() {
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
}

void show_message(const String& line1, const String& line2, int delay_ms) {
    message_line1 = line1;
    message_line2 = line2;
    currentDisplayMode = MESSAGE;
    loop_display(); 
    if (delay_ms > 0) delay(delay_ms);
}

// === IP DISPLAY WITH PAUSE ===
void show_ip_address(const String& ip, unsigned long display_time_ms) {
    ip_display_address = ip;
    ip_display_start_time = millis();
    ip_display_duration = display_time_ms;
    ip_display_paused = false;
    currentDisplayMode = IP_DISPLAY;
    Serial.printf("📶 IP Display started: %s (duration: %lums)\n", ip.c_str(), display_time_ms);
}

void pause_ip_display() {
    if (currentDisplayMode == IP_DISPLAY) {
        ip_display_paused = true;
        Serial.println("⏸️ IP Display PAUSED");
    }
}

void resume_ip_display() {
    if (currentDisplayMode == IP_DISPLAY && ip_display_paused) {
        ip_display_paused = false;
        // Transition to INFO mode immediately
        currentDisplayMode = INFO;
        reset_inactivity_timer();
        Serial.println("▶️ IP Display RESUMED - continuing to audio");
    }
}

bool is_ip_display_active() {
    return currentDisplayMode == IP_DISPLAY;
}

bool is_ip_display_paused() {
    return ip_display_paused;
}

void draw_info_screen() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // Название станции или IP адрес
    if (!stations.empty()) {
        display.setCursor(0, 0);
        display.print(stations[currentStation].name);
    } else {
        // Нет станций - показываем IP адрес
        display.setCursor(0, 0);
        display.print("No stations");
        
        if (WiFi.status() == WL_CONNECTED) {
            display.setCursor(0, 10);
            display.print("IP:");
            display.setCursor(0, 20);
            display.print(WiFi.localIP().toString());
        }
    }
    
    // Статус WiFi (только если есть станции)
    if (!stations.empty() && WiFi.status() == WL_CONNECTED) {
        String rssi_str = String(WiFi.RSSI()) + "dBm";
        display.setCursor(SCREEN_WIDTH - rssi_str.length() * 6, 0);
        display.print(rssi_str);
    }

    // Шкала громкости (только если есть станции)
    if (!stations.empty()) {
        int bar_x = 0;
        int bar_y = 22;
        int bar_w = SCREEN_WIDTH;
        int bar_h = 10;
        int corner_radius = 3;
        // Рисуем контур
        display.drawRoundRect(bar_x, bar_y, bar_w, bar_h, corner_radius, SSD1306_WHITE);
        // Рисуем заполнение
        int fill_w = (int)(displayVolume * (bar_w - 4));
        if (fill_w > 0) {
            display.fillRoundRect(bar_x + 2, bar_y + 2, fill_w, bar_h - 4, corner_radius - 1, SSD1306_WHITE);
        }
    }
    
    display.display();
}

void draw_shutdown_screen() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    String text = "Shutting down...";
    display.setCursor((SCREEN_WIDTH - text.length() * 6) / 2, 8);
    display.print(text);

    int bar_x = 14;
    int bar_y = 22;
    int bar_w = 100;
    int bar_h = 8;
    display.drawRect(bar_x, bar_y, bar_w, bar_h, SSD1306_WHITE);
    display.fillRect(bar_x, bar_y, (int)(bar_w * shutdownProgress), bar_h, SSD1306_WHITE);
    
    display.display();
}

// Визуализатор - делегируем рисование менеджеру
void draw_visualizer() {
    display.clearDisplay();
    visualizerManager.draw(display, visualizerBands, 16);
    display.display();
}

void draw_ap_mode_screen() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(20, 0);
    display.println("WiFi Setup Mode");
    display.setCursor(0, 12);
    display.println("SSID: ESP32-Radio-Setup");
    display.setCursor(0, 24);
    display.print("IP: ");
    display.print(ap_ip_address);
    display.display();
}

void draw_message_screen() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 8);
    display.println(message_line1);
    if (message_line2 != "") {
        display.setCursor(0, 18);
        display.println(message_line2);
    }
    display.display();
}

void draw_ip_display_screen() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // Title
    String title = "WiFi Connected!";
    display.setCursor((SCREEN_WIDTH - title.length() * 6) / 2, 0);
    display.println(title);
    
    // IP Address centered
    display.setTextSize(1);
    int ip_x = (SCREEN_WIDTH - ip_display_address.length() * 6) / 2;
    display.setCursor(ip_x, 14);
    display.println(ip_display_address);
    
    // If paused - show loader animation and hint
    if (ip_display_paused) {
        // Animated spinner
        static uint8_t spinnerFrame = 0;
        const char spinner[] = {'|', '/', '-', '\\'};
        spinnerFrame = (millis() / 150) % 4;
        
        // Draw spinner
        display.setTextSize(2);
        display.setCursor(SCREEN_WIDTH / 2 - 6, 24);
        display.print(spinner[spinnerFrame]);
        
        // Hint text
        display.setTextSize(1);
        String hint = "Click to continue";
        display.setCursor((SCREEN_WIDTH - hint.length() * 6) / 2, 50);
        display.println(hint);
    } else {
        // Auto-continuing - show countdown
        unsigned long remaining = ip_display_duration - (millis() - ip_display_start_time);
        if (remaining > ip_display_duration) remaining = 0; // Overflow protection
        
        String hint = "Starting in " + String(remaining / 1000 + 1) + "s";
        display.setTextSize(1);
        display.setCursor((SCREEN_WIDTH - hint.length() * 6) / 2, 50);
        display.println(hint);
        
        // Progress bar
        int bar_w = 100;
        int bar_x = (SCREEN_WIDTH - bar_w) / 2;
        int bar_y = 38;
        int bar_h = 4;
        float progress = (float)(millis() - ip_display_start_time) / ip_display_duration;
        if (progress > 1.0) progress = 1.0;
        
        display.drawRect(bar_x, bar_y, bar_w, bar_h, SSD1306_WHITE);
        display.fillRect(bar_x, bar_y, (int)(bar_w * progress), bar_h, SSD1306_WHITE);
    }
    
    display.display();
}

// Изменение поворота дисплея
void set_display_rotation(uint8_t rotation) {
    displayRotation = rotation;
    display.setRotation(displayRotation);
    // Перерисуем текущий экран
    reset_inactivity_timer();
}