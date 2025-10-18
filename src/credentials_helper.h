#ifndef CREDENTIALS_HELPER_H
#define CREDENTIALS_HELPER_H

#include <Arduino.h>

// === ФУНКЦИИ РАБОТЫ С УЧЕТНЫМИ ДАННЫМИ ===

/**
 * SHA256 хеширование строки
 * @param input Входная строка
 * @return HEX представление SHA256 хеша
 */
String sha256Hash(const String& input);

/**
 * Загрузка учетных данных из credentials.json
 * @return true если успешно загружено
 */
bool load_credentials();

/**
 * Сохранение учетных данных в credentials.json
 * @param username Имя пользователя (минимум 5 символов)
 * @param password Пароль (минимум 5 символов)
 * @return true если успешно сохранено
 */
bool save_credentials(const String& username, const String& password);

/**
 * Проверка учетных данных
 * @param username Имя пользователя
 * @param password Пароль
 * @return true если учетные данные совпадают
 */
bool verify_credentials(const String& username, const String& password);

#endif // CREDENTIALS_HELPER_H
