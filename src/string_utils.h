#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <Arduino.h>

// === ОПТИМИЗИРОВАННЫЕ ФУНКЦИИ ДЛЯ РАБОТЫ СО STRING ===
// Минимизация фрагментации heap через использование буферов и reserve()

/**
 * Создание строки с форматированием (как sprintf но для String)
 * Предварительно выделяет память, минимизируя фрагментацию
 * 
 * @param format Формат строки (как в printf)
 * @param ... Аргументы для форматирования
 * @return Отформатированная String
 * 
 * Пример: formatString("WiFi: %s (%d dBm)", ip.c_str(), rssi)
 */
String formatString(const char* format, ...);

/**
 * Безопасная конкатенация двух строк с предварительным выделением памяти
 * 
 * @param str1 Первая строка
 * @param str2 Вторая строка
 * @return Объединенная строка
 */
inline String concatOptimized(const String& str1, const String& str2) {
    String result;
    result.reserve(str1.length() + str2.length() + 1);
    result = str1;
    result += str2;
    return result;
}

/**
 * Безопасная конкатенация трех строк
 */
inline String concatOptimized(const String& str1, const String& str2, const String& str3) {
    String result;
    result.reserve(str1.length() + str2.length() + str3.length() + 1);
    result = str1;
    result += str2;
    result += str3;
    return result;
}

/**
 * Конвертация числа в String с предварительным выделением памяти
 * Оптимизирована для частых вызовов
 */
inline String intToString(int value) {
    char buffer[12]; // Достаточно для int32_t (-2147483648 = 11 символов + \0)
    snprintf(buffer, sizeof(buffer), "%d", value);
    return String(buffer);
}

inline String floatToString(float value, int decimals = 2) {
    char buffer[16]; // Достаточно для float
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return String(buffer);
}

/**
 * Создание JSON строки с одним полем (оптимизировано)
 * Пример: jsonField("rotation", 2) -> {"rotation":2}
 */
inline String jsonField(const char* key, int value) {
    String result;
    result.reserve(strlen(key) + 20); // key + {"":12345}\0
    result = "{\"";
    result += key;
    result += "\":";
    result += value;
    result += "}";
    return result;
}

inline String jsonField(const char* key, const String& value) {
    String result;
    result.reserve(strlen(key) + value.length() + 10); // key + {"":""}\0
    result = "{\"";
    result += key;
    result += "\":\"";
    result += value;
    result += "\"}";
    return result;
}

#endif // STRING_UTILS_H
