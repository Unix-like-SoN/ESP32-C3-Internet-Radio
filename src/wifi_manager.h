#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// Состояния восстановления WiFi при потере связи
enum WiFiRecoveryState {
    WIFI_OK,                    // WiFi работает нормально
    WIFI_AUTO_RECONNECTING,     // Ждем автоматического переподключения (0-40s)
    WIFI_MANUAL_RECONNECTING,   // Ручное переподключение через connect_to_wifi (40-90s)
    WIFI_FAILED_REBOOTING       // Переподключение не удалось, перезагрузка (5s countdown)
};

extern WiFiRecoveryState wifiRecoveryState;

// Основные функции
bool connect_to_wifi();
void setup_ap_mode();

// WiFi Recovery система (вызывается из main loop)
void handle_wifi_recovery();

#endif // WIFI_MANAGER_H