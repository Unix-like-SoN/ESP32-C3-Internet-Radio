#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <LittleFS.h>
#include "config.h"
#include "web_server_manager.h"
#include "wifi_manager.h"
#include "audio_manager.h"
#include "display_manager.h"
#include "credentials_helper.h"
#include "log_manager.h"
#include "system_manager.h"
#include "url_validator.h"
#include "string_utils.h"

// Веб-сервер
AsyncWebServer server(80);

// === ФУНКЦИИ ОТПРАВКИ КОМАНД (из main.cpp) ===
extern bool sendVolumeCommand(float volume);
extern bool sendNextStationCommand();
extern bool sendPrevStationCommand();
extern bool sendRebootCommand();
extern bool sendSaveStationsCommand();

// Флаг: веб-сервер загружает большой файл (приостановить audio)
// ATOMIC: memory-safe для асинхронного доступа
std::atomic<bool> web_loading_html(false);

// Состояние авторизации
static bool isRegistered = false;
static bool isAuthenticated = false;

// Макрос для проверки авторизации (включая токен)
#define CHECK_AUTH() \
    if (checkSessionToken(request)) isAuthenticated = true; \
    if (!isAuthenticated) return request->send(401);

// Генерация случайного сессионного токена
String generateSessionToken() {
    const char* chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    String token = "";
    token.reserve(SESSION_TOKEN_LENGTH); // Предварительное выделение памяти
    for (int i = 0; i < SESSION_TOKEN_LENGTH; i++) {
        token += chars[random(0, 62)];
    }
    return token;
}

// Проверка сессионного токена из куки
bool checkSessionToken(AsyncWebServerRequest *request) {
    if (sessionToken.length() == 0) return false;
    
    // Проверяем куки
    if (request->hasHeader("Cookie")) {
        String cookie = request->header("Cookie");
        int tokenPos = cookie.indexOf("session=");
        if (tokenPos != -1) {
            int tokenStart = tokenPos + 8;
            int tokenEnd = cookie.indexOf(";", tokenStart);
            String clientToken = (tokenEnd == -1) ? 
                cookie.substring(tokenStart) : 
                cookie.substring(tokenStart, tokenEnd);
            
            if (clientToken == sessionToken) {
                return true;
            }
        }
    }
    return false;
}

// --- HTML страницы ---
const char* login_html = R"rawliteral(
<!DOCTYPE html><html><head><title>Login</title>
<meta name=viewport content="width=device-width,initial-scale=1">
<style>*{box-sizing:border-box;}
body{font-family:sans-serif;text-align:center;background:#282c34;color:white;}
input{width:200px;padding:10px;margin:10px;border:1px solid #555;background:#2a2a2a;color:#fff;border-radius:4px;}
form{border:2px solid #61afef;padding:20px;display:inline-block;}
.forgot-link{margin-top:20px;}
.forgot-link a{color:#61afef;text-decoration:none;font-size:14px;}
.forgot-link a:hover{text-decoration:underline;}
.password-wrapper{position:relative;display:inline-block;width:220px;margin:10px;}
.password-wrapper input{width:100%;margin:0;padding:10px 40px 10px 10px;box-sizing:border-box;}
.eye-icon{position:absolute;right:10px;top:50%;transform:translateY(-50%);cursor:pointer;width:24px;height:24px;opacity:0.6;transition:opacity 0.2s;color:#ccc;}
.eye-icon:hover{opacity:1;color:#61afef;}
</style></head><body>
<h1>Radio Login</h1><form action=/login method=post>
<input type=text name=username placeholder=Username><br>
<div class=password-wrapper>
<input type=password name=password id=password placeholder=Password>
<svg class=eye-icon onclick="togglePassword('password')" viewBox="0 0 24 24" fill=none stroke=currentColor stroke-width=2 stroke-linecap=round stroke-linejoin=round>
<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
<circle cx=12 cy=12 r=3></circle>
</svg>
</div><br>
<input type=submit value=Login></form>
<div class=forgot-link><a href=/forgot-password>Забыл пароль?</a></div>
<script>
function togglePassword(id){const i=document.getElementById(id);const e=event.currentTarget;if(i.type==='password'){i.type='text';e.innerHTML='<path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"></path><line x1=1 y1=1 x2=23 y2=23 stroke-linecap=round stroke-linejoin=round></line>';}else{i.type='password';e.innerHTML='<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx=12 cy=12 r=3></circle>';}}
</script>
</body></html>
)rawliteral";

// HTML страница сброса пароля (factory reset)
const char* forgot_password_html = R"rawliteral(
<!DOCTYPE html><html><head><title>Factory Reset</title>
<meta name=viewport content="width=device-width,initial-scale=1">
<style>body{font-family:sans-serif;text-align:center;background:#282c34;color:white;}
.container{max-width:500px;margin:50px auto;padding:30px;border:2px solid #e06c75;background:#21252b;}
h1{color:#e06c75;}
.warning{background:#3e2a2a;padding:15px;margin:20px 0;border-left:4px solid #e06c75;}
.warning-text{color:#e5c07b;font-weight:bold;font-size:18px;margin-bottom:10px;}
button{padding:15px 30px;margin:10px;font-size:16px;cursor:pointer;border:none;border-radius:4px;}
.btn-reset{background:#e06c75;color:white;font-weight:bold;}
.btn-reset:hover{background:#d55;}
.btn-cancel{background:#5c6370;color:white;}
.btn-cancel:hover{background:#4a4f5a;}
ul{text-align:left;color:#abb2bf;margin:20px 0;}
</style></head><body>
<div class=container>
<h1>⚠️ Factory Reset</h1>
<div class=warning>
<div class=warning-text>ВНИМАНИЕ!</div>
<p>Сброс к заводским настройкам удалит:</p>
<ul>
<li>Логин и пароль</li>
<li>Все радиостанции</li>
<li>Настройки WiFi</li>
<li>Логи системы</li>
<li>Сессии</li>
</ul>
<p style="color:#e5c07b;font-weight:bold;">Это действие необратимо!</p>
</div>
<button class=btn-reset onclick="confirmReset()">Выполнить сброс</button>
<button class=btn-cancel onclick="location.href='/login'">Отмена</button>
</div>
<script>
function confirmReset() {
  if (confirm('Вы уверены? Все данные будут удалены!')) {
    fetch('/api/forgot-password-reset', {method: 'POST'})
      .then(response => {
        if (response.ok) {
          alert('Сброс выполнен! Устройство перезагружается...');
          setTimeout(() => location.href='/register', 3000);
        } else {
          alert('Ошибка сброса');
        }
      });
  }
}
</script>
</body></html>
)rawliteral";

// --- Режим точки доступа (AP Mode) ---

void start_web_server_ap() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        web_loading_html.store(true, std::memory_order_relaxed);
        request->send(LittleFS, "/ap_mode.html", "text/html; charset=utf-8");
    });

    server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest *request){
        WiFi.scanNetworks(true);
        request->send(200, "text/plain", "Scan Started");
    });

    server.on("/api/scan-results", HTTP_GET, [](AsyncWebServerRequest *request){
        int n = WiFi.scanComplete();
        if (n == -1) {
            request->send(202, "text/plain", "Scanning...");
        } else {
            JsonDocument doc;
            JsonArray array = doc.to<JsonArray>();
            for (int i = 0; i < n; ++i) {
                JsonObject net = array.add<JsonObject>();
                net["ssid"] = WiFi.SSID(i);
                net["rssi"] = WiFi.RSSI(i);
            }
            WiFi.scanDelete();
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json; charset=utf-8", response);
        }
    });

    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
        String ssid;
        if (request->hasParam("ssid_manual", true) && !request->getParam("ssid_manual", true)->value().isEmpty()) {
            ssid = request->getParam("ssid_manual", true)->value();
        } else if (request->hasParam("ssid_select", true)) {
            ssid = request->getParam("ssid_select", true)->value();
        }

        if (request->hasParam("password", true) && !ssid.isEmpty()) {
            String password = request->getParam("password", true)->value();
            show_message("Saving config...", ssid);
            wifiConfig.ssid = ssid;
            wifiConfig.password = password;
            if (save_wifi_config()) {
                request->send(200, "text/plain", "WiFi settings saved. Rebooting...");
                delay(1000);
                ESP.restart();
            } else {
                request->send(500, "text/plain", "Failed to save WiFi settings.");
            }
        } else {
            request->send(400, "text/plain", "Bad Request: SSID or password missing.");
        }
    });

    server.begin();
    Serial.println("Web-сервер запущен в режиме AP.");
}


// --- Режим станции (STA Mode) ---

void start_web_server_sta() {
    // Проверяем есть ли зарегистрированные пользователи
    isRegistered = load_credentials();
    
    // API обработчики регистрируются ПЕРВЫМИ!
    
    // === МАРШРУТИЗАЦИЯ КОРНЕВОГО ПУТИ ===
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!isRegistered) {
            request->redirect("/register");
            return;
        }
        
        // Автоматическая авторизация по токену
        if (checkSessionToken(request)) {
            isAuthenticated = true;
        }
        
        // Если не авторизован - на логин
        if (!isAuthenticated) {
            request->redirect("/login");
            return;
        }
        
        // Авторизован - index.html с UTF-8 и no-cache
        // ПРИОРИТЕТ: приостановить audio для быстрой отдачи HTML!
        web_loading_html.store(true, std::memory_order_relaxed);
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html", "text/html; charset=utf-8");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
        // Флаг сбросится в main loop через 100ms
    });
    
    // === РЕГИСТРАЦИЯ (только при первом запуске) ===
    server.on("/register", HTTP_GET, [](AsyncWebServerRequest *request){
        if (isRegistered) {
            // Уже зарегистрирован - редирект на логин
            request->redirect("/login");
            return;
        }
        
        web_loading_html.store(true, std::memory_order_relaxed);
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/register.html", "text/html; charset=utf-8");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Pragma", "no-cache");
        response->addHeader("Expires", "0");
        request->send(response);
    });

    server.on("/register", HTTP_POST, [](AsyncWebServerRequest *request){
        if (isRegistered) {
            request->send(400, "text/plain", "Регистрация уже выполнена");
            return;
        }
        
        if (request->hasParam("username", true) && request->hasParam("password", true)) {
            String username = request->getParam("username", true)->value();
            String password = request->getParam("password", true)->value();
            
            if (username.length() < 5 || password.length() < 5) {
                request->send(400, "text/plain", "Логин и пароль должны быть не менее 5 символов");
                return;
            }
            
            if (save_credentials(username, password)) {
                isRegistered = true;
                request->send(200, "text/plain", "OK");
            } else {
                request->send(500, "text/plain", "Ошибка сохранения");
            }
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });
    
    // === ЛОГИН ===
    server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request){
        // Если не зарегистрирован - редирект на регистрацию
        if (!isRegistered) {
            request->redirect("/register");
        }
        // Если уже авторизован - на главную
        else if (isAuthenticated) {
            request->redirect("/");
        }
        // Показываем страницу логина
        else {
            request->send(200, "text/html; charset=utf-8", login_html);
        }
    });

    server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
        if (request->hasParam("username", true) && request->hasParam("password", true)) {
            String username = request->getParam("username", true)->value();
            String password = request->getParam("password", true)->value();
            
            if (verify_credentials(username, password)) {
                isAuthenticated = true;
                
                // Генерируем session token ТОЛЬКО если его нет
                if (sessionToken.length() == 0) {
                    sessionToken = generateSessionToken();
                    save_state();  // Сохраняем токен
                    Serial.println("🆕 Session token создан.");
                } else {
                    Serial.println("♻️ Используем существующий session token.");
                }
                
                // Отправляем токен в куки (действует до очистки)
                AsyncWebServerResponse *response = request->beginResponse(302);
                response->addHeader("Location", "/");
                response->addHeader("Set-Cookie", "session=" + sessionToken + "; Path=/; Max-Age=31536000");
                request->send(response);
                
                Serial.println("✅ Успешная авторизация.");
            } else {
                request->send(401, "text/plain", "Unauthorized");
            }
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
        isAuthenticated = false;
        
        // Очищаем session token
        sessionToken = "";
        save_state();
        
        // Очищаем куки
        AsyncWebServerResponse *response = request->beginResponse(302);
        response->addHeader("Location", "/login");
        response->addHeader("Set-Cookie", "session=; Path=/; Max-Age=0");
        request->send(response);
        
        Serial.println("🚪 Выход из системы. Session token очищен.");
    });
    
    // === ЗАБЫЛ ПАРОЛЬ (FACTORY RESET) ===
    server.on("/forgot-password", HTTP_GET, [](AsyncWebServerRequest *request){
        // Показываем страницу предупреждения
        request->send(200, "text/html; charset=utf-8", forgot_password_html);
    });
    
    // API: Выполнение factory reset без пароля (для забывших пароль)
    server.on("/api/forgot-password-reset", HTTP_POST, [](AsyncWebServerRequest *request){
        // НЕ требуем авторизацию - это для тех, кто забыл пароль!
        
        log_message("⚠️ FACTORY RESET через 'Forgot Password'. Удаление всех данных...");
        
        // Удаляем все конфигурационные файлы
        LittleFS.remove("/credentials.json");
        LittleFS.remove("/wifi.json");
        LittleFS.remove("/stations.json");
        LittleFS.remove("/state.json");
        LittleFS.remove("/log.txt");
        
        // Сбрасываем флаги
        isRegistered = false;
        isAuthenticated = false;
        sessionToken = "";
        
        Serial.println("🔥 Factory Reset выполнен! Перезагрузка...");
        
        request->send(200, "text/plain", "Factory reset complete. Rebooting...");
        delay(1000);
        ESP.restart();
    });

    // API: Информация о станциях (количество и лимит)
    server.on("/api/stations/info", HTTP_GET, [](AsyncWebServerRequest *request){
        if (checkSessionToken(request)) isAuthenticated = true;
        if (!isAuthenticated) return request->send(401);
        
        // 🛡️ ЗАЩИТА ОТ RACE CONDITION
        STATIONS_LOCK();
        size_t currentSize = stations.size();
        STATIONS_UNLOCK();
        
        String json = "{";
        json += "\"current\":" + String(currentSize) + ",";
        json += "\"max\":" + String(MAX_RADIO_STATIONS) + ",";
        json += "\"available\":" + String(MAX_RADIO_STATIONS - currentSize);
        json += "}";
        request->send(200, "application/json; charset=utf-8", json);
    });

    server.on("/api/stations", HTTP_GET, [](AsyncWebServerRequest *request){
        // Автоматическая авторизация по токену
        if (checkSessionToken(request)) isAuthenticated = true;
        if (!isAuthenticated) return request->send(401);
        
        // 🛡️ ЗАЩИТА ОТ RACE CONDITION: копируем stations под мьютексом
        STATIONS_LOCK();
        std::vector<RadioStation> stationsCopy = stations;
        STATIONS_UNLOCK();
        
        // Оптимизация: уменьшаем размер JSON
        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();
        for (const auto& station : stationsCopy) {
            JsonObject obj = array.add<JsonObject>();
            obj["name"] = station.name;
            obj["url"] = station.url;
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json; charset=utf-8", response);
    });

    server.on("/api/add", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        
        // 🛡️ ЗАЩИТА ОТ RACE CONDITION: проверяем лимит под мьютексом
        STATIONS_LOCK();
        bool limitReached = stations.size() >= MAX_RADIO_STATIONS;
        STATIONS_UNLOCK();
        
        if (limitReached) {
            request->send(400, "text/plain", "Maximum stations limit reached (" + String(MAX_RADIO_STATIONS) + ")");
            return;
        }
        
        if (request->hasParam("name", true) && request->hasParam("url", true)) {
            String name = request->getParam("name", true)->value();
            String url = request->getParam("url", true)->value();
            
            // 🛡️ ВАЛИДАЦИЯ URL
            URLValidationResult validation = validateURL(url);
            if (validation != URL_VALID) {
                String errorMsg = getValidationErrorMessage(validation);
                request->send(400, "text/plain", "Invalid URL: " + errorMsg);
                return;
            }
            
            // 🛡️ Добавление под мьютексом
            STATIONS_LOCK();
            stations.push_back({name, url, true});
            STATIONS_UNLOCK();
            
            sendSaveStationsCommand();  // ✅ Через FreeRTOS Queue
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        if (request->hasParam("name", true)) {
            String name = request->getParam("name", true)->value();
            
            // 🛡️ ЗАЩИТА ОТ RACE CONDITION: удаление под мьютексом
            STATIONS_LOCK();
            stations.erase(std::remove_if(stations.begin(), stations.end(), [&](const RadioStation& s){ return s.name == name; }), stations.end());
            STATIONS_UNLOCK();
            
            sendSaveStationsCommand();  // ✅ Через FreeRTOS Queue
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    server.on("/api/update", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        if (request->hasParam("originalName", true) && request->hasParam("name", true) && request->hasParam("url", true)) {
            String originalName = request->getParam("originalName", true)->value();
            String name = request->getParam("name", true)->value();
            String url = request->getParam("url", true)->value();
            
            // 🛡️ ВАЛИДАЦИЯ URL
            URLValidationResult validation = validateURL(url);
            if (validation != URL_VALID) {
                String errorMsg = getValidationErrorMessage(validation);
                request->send(400, "text/plain", "Invalid URL: " + errorMsg);
                return;
            }
            
            for (auto& station : stations) {
                if (station.name == originalName) {
                    station.name = name;
                    station.url = url;
                    break;
                }
            }
            sendSaveStationsCommand();  // ✅ Через FreeRTOS Queue
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    // --- API пульта управления ---
    server.on("/api/player/next", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        if (sendNextStationCommand()) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(503, "text/plain", "Queue full");
        }
    });

    server.on("/api/player/previous", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        if (sendPrevStationCommand()) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(503, "text/plain", "Queue full");
        }
    });

    server.on("/api/player/volume", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        if (request->hasParam("volume", true)) {
            float vol = request->getParam("volume", true)->value().toFloat();
            if (sendVolumeCommand(vol)) {
                request->send(200, "text/plain", "OK");
            } else {
                request->send(503, "text/plain", "Queue full");
            }
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    // --- API дисплея ---
    server.on("/api/display/rotation", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        String json = jsonField("rotation", displayRotation);
        request->send(200, "application/json; charset=utf-8", json);
    });

    server.on("/api/display/rotation", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        if (request->hasParam("rotation", true)) {
            uint8_t rotation = request->getParam("rotation", true)->value().toInt();
            if (rotation == 0 || rotation == 2) {
                set_display_rotation(rotation);
                save_state(); // Сохраняем настройку
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Invalid rotation value");
            }
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    // --- API визуализатора ---
    // GET - получить текущий стиль
    server.on("/api/visualizer/style", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        String json = formatString("{\"style\":%d,\"name\":\"%s\"}", 
                                   (int)visualizerStyle, 
                                   visualizerManager.getCurrentStyleName());
        request->send(200, "application/json; charset=utf-8", json);
    });

    // POST - изменить стиль
    server.on("/api/visualizer/style", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        if (request->hasParam("style", true)) {
            int style = request->getParam("style", true)->value().toInt();
            if (style >= 0 && style < VISUALIZER_STYLE_COUNT) {
                visualizerStyle = (VisualizerStyle)style;
                visualizerManager.setStyle(visualizerStyle);
                save_state();
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Invalid style");
            }
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    // GET - список всех стилей
    server.on("/api/visualizer/styles", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        String json = "[";
        for (int i = 0; i < VISUALIZER_STYLE_COUNT; i++) {
            if (i > 0) json += ",";
            json += "{\"id\":" + String(i) + ",\"name\":\"" + VisualizerManager::getStyleName((VisualizerStyle)i) + "\"}";
        }
        json += "]";
        request->send(200, "application/json; charset=utf-8", json);
    });

    // --- API системы ---
    server.on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        if (sendRebootCommand()) {
            request->send(200, "text/plain", "Rebooting...");
        } else {
            request->send(503, "text/plain", "Queue full");
        }
    });

    server.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        
        if (request->hasParam("password", true)) {
            String password = request->getParam("password", true)->value();
            
            // Проверяем пароль
            if (verify_credentials(webCredentials.username, password)) {
                // Удаляем все конфигурационные файлы
                LittleFS.remove("/credentials.json");
                LittleFS.remove("/wifi.json");
                LittleFS.remove("/stations.json");
                LittleFS.remove("/state.json");
                LittleFS.remove("/log.txt");
                
                log_message("❗ FACTORY RESET выполнен! Перезагрузка...");
                
                request->send(200, "text/plain", "Factory reset complete");
                delay(500);
                ESP.restart();
            } else {
                request->send(401, "text/plain", "Неверный пароль");
            }
        } else {
            request->send(400, "text/plain", "Bad Request");
        }
    });

    // --- API логов ---
    server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        request->send(200, "text/plain", get_logs());
    });

    // --- API управления станциями (расширенное) ---
    server.on("/api/stations/export", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!isAuthenticated) return request->send(401);
        request->send(LittleFS, "/stations.json", "application/json; charset=utf-8", true);
    });

    AsyncCallbackJsonWebHandler* orderHandler = new AsyncCallbackJsonWebHandler("/api/stations/order", [](AsyncWebServerRequest *request, JsonVariant &json) {
        if (!isAuthenticated) return request->send(401);
        JsonArray newOrder = json.as<JsonArray>();
        std::vector<RadioStation> ordered_stations;
        
        // 🛡️ ЗАЩИТА ОТ RACE CONDITION: читаем stations под мьютексом
        STATIONS_LOCK();
        for (JsonVariant v : newOrder) {
            String name = v.as<String>();
            for (const auto& s : stations) {
                if (s.name == name) {
                    ordered_stations.push_back(s);
                    break;
                }
            }
        }
        
        // 🛡️ Перезаписываем stations только если все станции валидны
        if (ordered_stations.size() == stations.size()) {
            stations = ordered_stations;
            STATIONS_UNLOCK();
            sendSaveStationsCommand();  // ✅ Через FreeRTOS Queue
            request->send(200, "text/plain", "Order saved");
        } else {
            STATIONS_UNLOCK();
            request->send(400, "text/plain", "Invalid station order data");
        }
    });
    server.addHandler(orderHandler);

    // Обработка загрузки файла для импорта
    server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if (!isAuthenticated) return request->send(401);
        if (request->url() == "/api/stations/import") {
            if (!index) {
                // Открываем файл для записи
                request->_tempFile = LittleFS.open("/stations.json", "w");
            }
            if (len) {
                // Пишем данные в файл
                request->_tempFile.write(data, len);
            }
            if (final) {
                // Закрываем файл и перезагружаем конфиг
                request->_tempFile.close();
                load_stations_config();
                request->send(200, "text/plain", "Import successful");
            }
        }
    });

    // Статические файлы РЕГИСТРИРУЕМ В КОНЦЕ!
    // КЕШИРОВАНИЕ для ускорения (1 час)
    server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("max-age=3600");

    server.onNotFound([](AsyncWebServerRequest *request){
        // Если не зарегистрирован - на регистрацию
        if (!isRegistered) {
            request->redirect("/register");
        }
        // Если не авторизован - на логин
        else if (!isAuthenticated) {
            request->redirect("/login");
        }
        // Авторизован - 404
        else {
            request->send(404, "text/plain", "Not found");
        }
    });

    server.begin();
    Serial.println("Web-сервер запущен в режиме STA.");
}