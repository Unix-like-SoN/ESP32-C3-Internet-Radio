#include "url_validator.h"

// === ОСНОВНАЯ ВАЛИДАЦИЯ URL ===
URLValidationResult validateURL(const String& url) {
    // 1. Проверка на пустую строку
    if (url.length() == 0) {
        return URL_EMPTY;
    }
    
    // 2. Проверка длины
    if (url.length() < URL_MIN_LENGTH) {
        return URL_TOO_SHORT;
    }
    
    if (url.length() > URL_MAX_LENGTH) {
        return URL_TOO_LONG;
    }
    
    // 3. Проверка протокола (только http/https)
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        return URL_INVALID_PROTOCOL;
    }
    
    // 4. Проверка на опасные символы (XSS защита)
    // Разрешаем только безопасные символы в URL
    for (unsigned int i = 0; i < url.length(); i++) {
        char c = url.charAt(i);
        
        // Запрещаем управляющие символы
        if (c < 32 || c == 127) {
            return URL_DANGEROUS_CHARS;
        }
        
        // Запрещаем опасные HTML символы без encoding
        if (c == '<' || c == '>' || c == '"' || c == '\'') {
            return URL_DANGEROUS_CHARS;
        }
    }
    
    // 5. Извлечение hostname для проверки
    String hostname = extractHostname(url);
    if (hostname.length() == 0) {
        return URL_INVALID_FORMAT;
    }
    
    if (hostname.length() > URL_MAX_HOSTNAME_LENGTH) {
        return URL_INVALID_FORMAT;
    }
    
    // 6. Проверка на запрещенные адреса
    if (isBlockedIPAddress(hostname)) {
        // Определяем конкретный тип блокировки
        if (hostname.equals("localhost") || 
            hostname.startsWith("127.") ||
            hostname.equals("::1")) {
            return URL_LOCALHOST_BLOCKED;
        }
        
        if (hostname.startsWith("169.254.")) {
            return URL_LINK_LOCAL_BLOCKED;
        }
        
        return URL_PRIVATE_IP_BLOCKED;
    }
    
    return URL_VALID;
}

// === ИЗВЛЕЧЕНИЕ HOSTNAME ИЗ URL ===
String extractHostname(const String& url) {
    // Убираем протокол
    int startIdx = 0;
    if (url.startsWith("http://")) {
        startIdx = 7;  // length of "http://"
    } else if (url.startsWith("https://")) {
        startIdx = 8;  // length of "https://"
    } else {
        return "";
    }
    
    // Ищем конец hostname (первый '/', ':', '?', или '#')
    int endIdx = url.length();
    for (int i = startIdx; i < url.length(); i++) {
        char c = url.charAt(i);
        if (c == '/' || c == ':' || c == '?' || c == '#') {
            endIdx = i;
            break;
        }
    }
    
    if (endIdx <= startIdx) {
        return "";
    }
    
    String hostname = url.substring(startIdx, endIdx);
    hostname.toLowerCase(); // Hostname case-insensitive
    
    return hostname;
}

// === ПРОВЕРКА НА ЗАБЛОКИРОВАННЫЕ IP АДРЕСА ===
bool isBlockedIPAddress(const String& hostname) {
    // 1. Проверка на localhost
    if (hostname.equals("localhost") || 
        hostname.equals("localhost.localdomain") ||
        hostname.equals("::1")) {
        return true;
    }
    
    // 2. Попытка распарсить как IP адрес
    IPAddress ip;
    if (ip.fromString(hostname)) {
        // Это IP адрес - проверяем диапазоны
        return isPrivateIP(ip);
    }
    
    // 3. Это доменное имя - проверяем известные паттерны
    // Блокируем очевидные локальные домены
    if (hostname.endsWith(".local") || 
        hostname.endsWith(".localhost") ||
        hostname.startsWith("localhost.")) {
        return true;
    }
    
    // 4. Попытка резолвинга DNS (опционально, может замедлить)
    // Раскомментировать если нужна дополнительная защита:
    /*
    ip = WiFi.hostByName(hostname.c_str());
    if (ip != INADDR_NONE) {
        return isPrivateIP(ip);
    }
    */
    
    return false;
}

// === ПРОВЕРКА IP НА ПРИНАДЛЕЖНОСТЬ К ПРИВАТНЫМ ДИАПАЗОНАМ ===
bool isPrivateIP(IPAddress ip) {
    uint8_t first = ip[0];
    uint8_t second = ip[1];
    
    // 127.0.0.0/8 - Loopback
    if (first == 127) {
        return true;
    }
    
    // 169.254.0.0/16 - Link-local
    if (first == 169 && second == 254) {
        return true;
    }
    
    // 10.0.0.0/8 - Private
    if (first == 10) {
        return true;
    }
    
    // 172.16.0.0/12 - Private
    if (first == 172 && second >= 16 && second <= 31) {
        return true;
    }
    
    // 192.168.0.0/16 - Private
    if (first == 192 && second == 168) {
        return true;
    }
    
    // 0.0.0.0/8 - Current network
    if (first == 0) {
        return true;
    }
    
    // 224.0.0.0/4 - Multicast
    if (first >= 224 && first <= 239) {
        return true;
    }
    
    // 240.0.0.0/4 - Reserved
    if (first >= 240) {
        return true;
    }
    
    return false;
}

// === САНИТИЗАЦИЯ URL ДЛЯ ЛОГОВ (XSS ЗАЩИТА) ===
String sanitizeURLForLog(const String& url) {
    String sanitized;
    sanitized.reserve(min((int)url.length(), URL_SANITIZED_MAX_LENGTH));
    
    for (unsigned int i = 0; i < url.length() && sanitized.length() < URL_SANITIZED_MAX_LENGTH; i++) {
        char c = url.charAt(i);
        
        // Заменяем опасные символы на безопасные
        if (c == '<') {
            sanitized += "&lt;";
        } else if (c == '>') {
            sanitized += "&gt;";
        } else if (c == '"') {
            sanitized += "&quot;";
        } else if (c == '\'') {
            sanitized += "&#39;";
        } else if (c == '&') {
            sanitized += "&amp;";
        } else if (c < 32 || c == 127) {
            // Управляющие символы заменяем на пробел
            sanitized += ' ';
        } else {
            // Безопасный символ
            sanitized += c;
        }
    }
    
    // Обрезаем если слишком длинный
    if (url.length() > URL_SANITIZED_MAX_LENGTH) {
        sanitized += "... [truncated]";
    }
    
    return sanitized;
}

// === ПОЛУЧЕНИЕ СООБЩЕНИЯ ОБ ОШИБКЕ ===
String getValidationErrorMessage(URLValidationResult result) {
    switch (result) {
        case URL_VALID:
            return "URL валиден";
            
        case URL_EMPTY:
            return "URL не может быть пустым";
            
        case URL_TOO_SHORT:
            return "URL слишком короткий (минимум " + String(URL_MIN_LENGTH) + " символов)";
            
        case URL_TOO_LONG:
            return "URL слишком длинный (максимум " + String(URL_MAX_LENGTH) + " символов)";
            
        case URL_INVALID_PROTOCOL:
            return "Недопустимый протокол. Разрешены только http:// и https://";
            
        case URL_LOCALHOST_BLOCKED:
            return "Доступ к localhost запрещен (безопасность)";
            
        case URL_PRIVATE_IP_BLOCKED:
            return "Доступ к приватным IP адресам запрещен (безопасность)";
            
        case URL_LINK_LOCAL_BLOCKED:
            return "Доступ к link-local адресам запрещен (безопасность)";
            
        case URL_INVALID_FORMAT:
            return "Некорректный формат URL";
            
        case URL_DANGEROUS_CHARS:
            return "URL содержит недопустимые символы";
            
        default:
            return "Неизвестная ошибка валидации";
    }
}
