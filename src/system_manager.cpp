#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include "config.h"
#include "system_manager.h"
#include "wifi_manager.h"
#include "audio_manager.h"
#include "display_manager.h"
#include "input_handler.h"
#include "log_manager.h"
#include "string_utils.h"

// Определяем глобальную переменную состояния
SystemState systemState = STATE_STA; // По умолчанию - режим станции

static unsigned long lastStatusPrint = 0;

// === ПРОВЕРКА СТАБИЛЬНОСТИ ПИТАНИЯ ПРИ СТАРТЕ ===
void check_power_stability() {
    Serial.println("\n⚡ ПРОВЕРКА ПИТАНИЯ...");
    
    // 1. Задержка для стабилизации питания после включения
    // Конденсаторы на плате должны зарядиться
    delay(POWER_STABILIZATION_DELAY);
    
    // 2. Проверка что не было brown-out при старте
    esp_reset_reason_t reason = esp_reset_reason();
    bool wasBrownout = (reason == ESP_RST_BROWNOUT);
    
    if (wasBrownout) {
        Serial.println("⚠️ ВНИМАНИЕ: Предыдущий сброс был из-за brown-out!");
        Serial.println("⚡ Даем дополнительное время для стабилизации питания...");
        delay(POWER_BROWNOUT_EXTRA_DELAY); // Дополнительная задержка если был brown-out
    }
    
    // 3. Информация о brown-out detector
    Serial.println("✅ Brown-out Detector активен (порог: 2.51V)");
    Serial.println("📊 Минимальное напряжение для стабильной работы:");
    Serial.println("   - ESP32-C3: 2.3V - 3.6V (номинал: 3.3V)");
    Serial.println("   - Рекомендуемое: 3.0V - 3.3V");
    Serial.println("   - USB питание: 5.0V → 3.3V (LDO на плате)");
    
    // 4. Рекомендации по питанию
    if (wasBrownout) {
        Serial.println("\n🔴 КРИТИЧНО: Проблема с питанием обнаружена!");
        Serial.println("Рекомендации:");
        Serial.println("  1. Используйте качественный USB кабель (короткий, толстый)");
        Serial.println("  2. USB порт должен давать ≥500mA (лучше ≥1A)");
        Serial.println("  3. Проверьте контакты питания (VCC/GND)");
        Serial.println("  4. Для MAX98357A используйте отдельное питание 5V/2A");
        Serial.println("  5. Добавьте конденсатор 100-470uF на 5V линию\n");
    } else {
        Serial.println("✅ Питание стабильно. Продолжаем загрузку...\n");
    }
}

// === ПРОВЕРКА ПРИЧИНЫ СБРОСА ===
void check_reset_reason() {
    esp_reset_reason_t reason = esp_reset_reason();
    
    String resetReason;
    bool isCritical = false;
    
    switch (reason) {
        case ESP_RST_POWERON:
            resetReason = "Power-on reset (холодный старт)";
            break;
        case ESP_RST_SW:
            resetReason = "Software reset (ESP.restart())";
            break;
        case ESP_RST_PANIC:
            resetReason = "⚠️ PANIC! Программный сбой (exception/abort)";
            isCritical = true;
            break;
        case ESP_RST_INT_WDT:
            resetReason = "⚠️ WATCHDOG! Interrupt watchdog timeout";
            isCritical = true;
            break;
        case ESP_RST_TASK_WDT:
            resetReason = "⚠️ WATCHDOG! Task watchdog timeout";
            isCritical = true;
            break;
        case ESP_RST_WDT:
            resetReason = "⚠️ WATCHDOG! Other watchdog reset";
            isCritical = true;
            break;
        case ESP_RST_DEEPSLEEP:
            resetReason = "Wake up from deep sleep";
            break;
        case ESP_RST_BROWNOUT:
            resetReason = "🔴 BROWN-OUT! Критическое падение напряжения питания";
            isCritical = true;
            break;
        case ESP_RST_SDIO:
            resetReason = "Reset over SDIO";
            break;
        default:
            resetReason = "Unknown reset (" + String((int)reason) + ")";
            break;
    }
    
    if (isCritical) {
        Serial.println("\n========================================");
        Serial.println("⚠️⚠️⚠️ ВНИМАНИЕ: КРИТИЧЕСКИЙ СБРОС ⚠️⚠️⚠️");
        Serial.println("Причина: " + resetReason);
        Serial.println("========================================\n");
        log_message(concatOptimized("КРИТИЧЕСКИЙ СБРОС: ", resetReason));
        
        // Если был brown-out - предупреждаем о проблеме питания
        if (reason == ESP_RST_BROWNOUT) {
            log_message("⚡ ПРОВЕРЬТЕ ПИТАНИЕ! Напряжение упало ниже 2.51V");
            log_message("Возможные причины:");
            log_message("- Слабый USB кабель/порт");
            log_message("- Недостаточный ток (нужно >500mA)");
            log_message("- Плохой контакт питания");
        }
    } else {
        Serial.println("Причина сброса: " + resetReason);
        log_message(concatOptimized("Сброс: ", resetReason));
    }
}

void loop_system_tasks() {
    // WiFi recovery теперь вызывается напрямую из main loop
    
    if (millis() - lastStatusPrint > SYSTEM_STATUS_INTERVAL) {
        print_system_status();
        lastStatusPrint = millis();
    }
}

void print_system_status() {
    const char* stateNames[] = {"IDLE", "CONNECTING", "STARTING", "PLAYING", "ERROR"};
    
    Serial.println("\n=== СТАТУС ===");
    if (WiFi.getMode() == WIFI_AP) {
        Serial.println("Режим: Точка доступа (AP)");
        Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("Режим: Станция (STA)");
        Serial.printf("WiFi: %s (%ld dBm)\n", 
                      WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "Отключен", 
                      WiFi.RSSI());
    }

    if (!stations.empty()) {
        Serial.printf("Аудио: %s\n", stateNames[audioState]);
        Serial.printf("Станция: %s (%s)\n", 
                      stations[currentStation].name.c_str(),
                      stations[currentStation].isAvailable ? "OK" : "Недоступна");
        Serial.printf("Громкость: %.2f\n", volume);
    } else {
        Serial.println("Аудио: Нет станций для воспроизведения.");
    }
    
    Serial.printf("RAM: %d байт\n", ESP.getFreeHeap());
    Serial.printf("Время: %lu мин\n", millis() / 60000);
    Serial.println("=================");
}

void shutdown_system() {
    Serial.println("Выключение системы. Переход в глубокий сон.");
    show_message("Goodbye!", "", 500);
    
    // ✅ Сохраняем отложенную громкость до save_state()
    force_save_volume();
    save_state(); // Сохраняем состояние перед выключением
    
    force_audio_reset();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    
    turn_off_display();
    delay(100);
    
    // Настраиваем пробуждение по кнопке энкодера (из config.h)
    // Кнопка при нажатии подтягивает пин к земле (LOW)
    const uint64_t ext_wakeup_pin_mask = 1ULL << ENCODER_SW;
    esp_deep_sleep_enable_gpio_wakeup(ext_wakeup_pin_mask, ESP_GPIO_WAKEUP_GPIO_LOW); 
    
    Serial.println("Вход в глубокий сон...");
    esp_deep_sleep_start();
}
