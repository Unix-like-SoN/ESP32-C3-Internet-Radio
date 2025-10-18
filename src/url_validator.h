#ifndef URL_VALIDATOR_H
#define URL_VALIDATOR_H

#include <Arduino.h>
#include <WiFi.h>

// === РЕЗУЛЬТАТ ВАЛИДАЦИИ URL ===
enum URLValidationResult {
    URL_VALID,                  // URL валиден и безопасен
    URL_EMPTY,                  // Пустая строка
    URL_TOO_SHORT,              // Слишком короткий URL
    URL_TOO_LONG,               // Слишком длинный URL (DoS риск)
    URL_INVALID_PROTOCOL,       // Неподдерживаемый протокол
    URL_LOCALHOST_BLOCKED,      // Попытка доступа к localhost
    URL_PRIVATE_IP_BLOCKED,     // Попытка доступа к private IP
    URL_LINK_LOCAL_BLOCKED,     // Попытка доступа к link-local
    URL_INVALID_FORMAT,         // Некорректный формат URL
    URL_DANGEROUS_CHARS         // Опасные символы (XSS риск)
};

// === КОНСТАНТЫ ВАЛИДАЦИИ ===
#define URL_MIN_LENGTH              10      // Минимальная длина URL (http://a.b)
#define URL_MAX_LENGTH              2048    // Максимальная длина URL (защита от DoS)
#define URL_MAX_HOSTNAME_LENGTH     253     // RFC 1035
#define URL_SANITIZED_MAX_LENGTH    512     // Максимальная длина санитизированного URL для логов

// === ФУНКЦИИ ВАЛИДАЦИИ ===

/**
 * Основная функция валидации URL
 * @param url URL для проверки
 * @return Результат валидации
 */
URLValidationResult validateURL(const String& url);

/**
 * Проверка на локальные/приватные IP адреса
 * @param hostname Имя хоста или IP адрес
 * @return true если адрес заблокирован
 */
bool isBlockedIPAddress(const String& hostname);

/**
 * Санитизация URL для безопасного вывода в логи
 * Удаляет потенциально опасные символы (защита от XSS)
 * @param url Исходный URL
 * @return Безопасный URL для логирования
 */
String sanitizeURLForLog(const String& url);

/**
 * Получить текстовое описание ошибки валидации
 * @param result Результат валидации
 * @return Описание ошибки
 */
String getValidationErrorMessage(URLValidationResult result);

/**
 * Извлечь hostname из URL
 * @param url URL
 * @return Hostname или пустая строка при ошибке
 */
String extractHostname(const String& url);

/**
 * Проверка IP адреса на принадлежность к приватным диапазонам
 * @param ip IP адрес для проверки
 * @return true если IP в приватном диапазоне
 */
bool isPrivateIP(IPAddress ip);

#endif // URL_VALIDATOR_H
