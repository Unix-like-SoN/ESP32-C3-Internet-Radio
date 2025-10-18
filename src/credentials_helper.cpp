#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>

// === ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: SHA256 хеширование ===
String sha256Hash(const String& input) {
    byte hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)input.c_str(), input.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);
    
    // Конвертируем в HEX строку
    String hashString = "";
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf(hex, "%02x", hash[i]);
        hashString += hex;
    }
    return hashString;
}

// === ЗАГРУЗКА УЧЕТНЫХ ДАННЫХ ===
bool load_credentials() {
    File configFile = LittleFS.open("/credentials.json", "r");
    if (!configFile) {
        Serial.println("Файл credentials.json не найден. Требуется регистрация.");
        webCredentials.username = "";
        webCredentials.passwordHash = "";
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error) {
        Serial.print("Ошибка парсинга credentials.json: ");
        Serial.println(error.c_str());
        return false;
    }

    webCredentials.username = doc["username"].as<String>();
    webCredentials.passwordHash = doc["passwordHash"].as<String>();
    
    Serial.println("Учетные данные загружены.");
    return true;
}

// === СОХРАНЕНИЕ УЧЕТНЫХ ДАННЫХ ===
bool save_credentials(const String& username, const String& password) {
    // Валидация
    if (username.length() < 5 || password.length() < 5) {
        Serial.println("Ошибка: логин и пароль должны быть не менее 5 символов.");
        return false;
    }

    File configFile = LittleFS.open("/credentials.json", "w");
    if (!configFile) {
        Serial.println("Не удалось создать credentials.json для записи.");
        return false;
    }

    // Хешируем пароль
    String passwordHash = sha256Hash(password);

    JsonDocument doc;
    doc["username"] = username;
    doc["passwordHash"] = passwordHash;

    if (serializeJson(doc, configFile) == 0) {
        Serial.println("Ошибка записи в credentials.json.");
        configFile.close();
        return false;
    }

    configFile.close();
    
    // Обновляем глобальные переменные
    webCredentials.username = username;
    webCredentials.passwordHash = passwordHash;
    
    Serial.println("Учетные данные сохранены.");
    return true;
}

// === ПРОВЕРКА УЧЕТНЫХ ДАННЫХ ===
bool verify_credentials(const String& username, const String& password) {
    if (webCredentials.username.isEmpty() || webCredentials.passwordHash.isEmpty()) {
        return false;
    }

    String passwordHash = sha256Hash(password);
    
    return (username == webCredentials.username && passwordHash == webCredentials.passwordHash);
}
