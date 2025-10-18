#include <Arduino.h>
#include "wifi_manager.h"
#include "audio_manager.h"
#include "display_manager.h"
#include "web_server_manager.h"
#include "input_handler.h"
#include "system_manager.h"
#include "log_manager.h"
#include "string_utils.h"
#include "config.h"

// === ПОТОКОБЕЗОПАСНАЯ СИСТЕМА КОМАНД (FreeRTOS Queue) ===
// Типы команд от веб-интерфейса
enum CommandType {
    CMD_NONE = 0,
    CMD_VOLUME,          // Изменить громкость
    CMD_NEXT_STATION,    // Следующая станция
    CMD_PREV_STATION,    // Предыдущая станция
    CMD_REBOOT,          // Перезагрузка
    CMD_SAVE_STATIONS    // Сохранить конфигурацию станций
};

struct SystemCommand {
    CommandType type;
    float floatValue;    // Для CMD_VOLUME
};

// Очередь команд (потокобезопасная)
QueueHandle_t commandQueue = nullptr;

// === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ОТПРАВКИ КОМАНД ===
// Используются в web_server_manager.cpp

bool sendCommand(CommandType type, float value = 0.0f) {
    if (commandQueue == nullptr) return false;
    
    SystemCommand cmd;
    cmd.type = type;
    cmd.floatValue = value;
    
    // Попытка отправить команду в очередь (без ожидания)
    BaseType_t result = xQueueSend(commandQueue, &cmd, 0);
    return (result == pdTRUE);
}

// Удобные обертки
bool sendVolumeCommand(float volume) {
    return sendCommand(CMD_VOLUME, volume);
}

bool sendNextStationCommand() {
    return sendCommand(CMD_NEXT_STATION);
}

bool sendPrevStationCommand() {
    return sendCommand(CMD_PREV_STATION);
}

bool sendRebootCommand() {
    return sendCommand(CMD_REBOOT);
}

bool sendSaveStationsCommand() {
    return sendCommand(CMD_SAVE_STATIONS);
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(SERIAL_INIT_DELAY);
    
    // ⚡ ПЕРВЫМ ДЕЛОМ: Проверка стабильности питания
    // ЭТО КРИТИЧНО! Должно быть ДО всех остальных инициализаций
    check_power_stability();
    
    setup_filesystem();
    setup_logging(); // Инициализируем и очищаем логи
    
    // Инициализация очереди команд FreeRTOS
    commandQueue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(SystemCommand));
    if (commandQueue == nullptr) {
        Serial.println("❌ ОШИБКА: Не удалось создать очередь команд!");
    }
    
    log_message("ESP32-C3 Радио v4.5 (Stable Arch)");
    log_message("====================================");
    
    // Проверяем причину сброса (включая brown-out detection)
    check_reset_reason();
    
    bool wifi_config_exists = load_wifi_config();
    load_stations_config();
    load_state();
    save_state();  // Сохраняем инкрементированный rebootCounter
    
    // Применяем загруженный стиль визуализатора
    visualizerManager.setStyle(visualizerStyle);
    
    setup_display();
    setup_input();
    
    // Если конфиг WiFi есть - пытаемся подключиться
    if (wifi_config_exists && connect_to_wifi()) {
        // --- РЕЖИМ НОРМАЛЬНОЙ РАБОТЫ (STA) ---
        systemState = STATE_STA;
        log_message("Подключено к WiFi. Запуск в режиме станции (STA).");
        setup_audio();
        start_web_server_sta(); // Запускаем веб-сервер для управления станциями
    } else {
        // --- РЕЖИМ НАСТРОЙКИ (AP) ---
        systemState = STATE_AP;
        log_message("Не удалось подключиться к WiFi. Запуск в режиме точки доступа (AP).");
        setup_ap_mode();
        start_web_server_ap(); // Запускаем веб-сервер для настройки WiFi
    }
    
    reset_inactivity_timer();
    log_message("\n=== СИСТЕМА ГОТОВА ===");
    print_system_status();
}

void loop() {
    // === ОБРАБОТКА КОМАНД ИЗ ОЧЕРЕДИ (потокобезопасно) ===
    SystemCommand cmd;
    while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
        switch (cmd.type) {
            case CMD_VOLUME:
                set_volume(cmd.floatValue);
                // ✅ Сохранение громкости теперь через debounce в loop_input()
                log_message(formatString("🔊 Громкость: %.2f", cmd.floatValue));
                break;
                
            case CMD_NEXT_STATION:
                next_station();
                log_message("⏭️ Следующая станция");
                break;
                
            case CMD_PREV_STATION:
                previous_station();
                log_message("⏮️ Предыдущая станция");
                break;
                
            case CMD_REBOOT:
                log_message("🔄 Перезагрузка...");
                delay(500);
                ESP.restart();
                break;
                
            case CMD_SAVE_STATIONS:
                save_stations_config();
                log_message("💾 Конфигурация станций сохранена");
                break;
                
            default:
                break;
        }
    }

    // ПРИОРИТЕТ 1: Audio (КАЖДУЮ итерацию для плавного стрима!)
    // НО! ПРИОСТАНОВИТЬ если веб-сервер загружает HTML!
    static unsigned long htmlLoadStartTime = 0;
    // ATOMIC: memory_order_relaxed - не нужен строгий порядок, только атомарность
    if (web_loading_html.load(std::memory_order_relaxed)) {
        if (htmlLoadStartTime == 0) {
            htmlLoadStartTime = millis();
        }
        // Автосброс через 200ms (на случай если зависло)
        if (millis() - htmlLoadStartTime > 200) {
            web_loading_html.store(false, std::memory_order_relaxed);
            htmlLoadStartTime = 0;
        }
        // ПРОПУСКАЕМ audio во время загрузки HTML
    } else {
        htmlLoadStartTime = 0;
        if (systemState == STATE_STA) {
            loop_audio();  // Без задержек!
        }
    }
    
    // ПРИОРИТЕТ 2: Input (часто, для отзывчивости)
    loop_input();
    
    // ПРИОРИТЕТ 3: Display (только каждые 16ms = 60 FPS)
    // 🎯 ПРОПУСКАЕМ display во время активного декодирования MP3
    static unsigned long lastDisplayUpdate = 0;
    bool audioDecoding = audio_decoding_active.load(std::memory_order_relaxed);
    
    if (!audioDecoding && millis() - lastDisplayUpdate >= 16) {
        loop_display();
        lastDisplayUpdate = millis();
    }
    
    // ПРИОРИТЕТ 4: WiFi Recovery (каждые 500ms для точного мониторинга)
    if (systemState == STATE_STA) {
        static unsigned long lastWiFiRecoveryCheck = 0;
        if (millis() - lastWiFiRecoveryCheck >= 500) {
            handle_wifi_recovery();
            lastWiFiRecoveryCheck = millis();
        }
    }
    
    // ПРИОРИТЕТ 5: System tasks (редко - каждую секунду)
    if (systemState == STATE_STA) {
        static unsigned long lastSystemUpdate = 0;
        if (millis() - lastSystemUpdate >= 1000) {
            loop_system_tasks();
            lastSystemUpdate = millis();
        }
    }
    
    // НЕТ delay()! Максимальная скорость для audio!
    yield();  // Даем время WiFi/веб-серверу
}
