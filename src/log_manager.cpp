#include "log_manager.h"
#include "string_utils.h"
#include <LittleFS.h>

#define LOG_FILE "/log.txt"
#define MAX_LOG_SIZE 20480  // 20KB - максимальный размер лог-файла
#define MAX_LOG_RESPONSE 8192  // 8KB - максимум для веб-ответа

void setup_logging() {
    clear_logs();
    log_message("--- СИСТЕМА ЗАПУЩЕНА ---");
}

void log_message(const String& message) {
    // Дублируем в Serial
    Serial.println(message);

    // Проверяем размер лог-файла
    if (LittleFS.exists(LOG_FILE)) {
        File checkFile = LittleFS.open(LOG_FILE, "r");
        if (checkFile && checkFile.size() > MAX_LOG_SIZE) {
            checkFile.close();
            // Файл слишком большой - очищаем
            LittleFS.remove(LOG_FILE);
        } else if (checkFile) {
            checkFile.close();
        }
    }

    // Записываем в файл
    File logFile = LittleFS.open(LOG_FILE, "a");
    if (logFile) {
        logFile.println(message);
        logFile.close();
    }
}

void clear_logs() {
    if (LittleFS.exists(LOG_FILE)) {
        LittleFS.remove(LOG_FILE);
    }
}

String get_logs() {
    File logFile = LittleFS.open(LOG_FILE, "r");
    if (!logFile) {
        return "Логи отсутствуют.";
    }
    
    size_t fileSize = logFile.size();
    
    // Если файл большой - читаем только последние 8KB
    if (fileSize > MAX_LOG_RESPONSE) {
        logFile.seek(fileSize - MAX_LOG_RESPONSE);
        // Пропускаем неполную первую строку
        logFile.readStringUntil('\n');
    }
    
    String logs = logFile.readString();
    logFile.close();
    
    // Добавляем инфо если логи были обрезаны
    if (fileSize > MAX_LOG_RESPONSE) {
        char truncMsg[64];
        snprintf(truncMsg, sizeof(truncMsg), "[...обрезано %dKB...]\n", (fileSize - MAX_LOG_RESPONSE) / 1024);
        logs = String(truncMsg) + logs; // Одна конкатенация вместо трех
    }
    
    return logs;
}
