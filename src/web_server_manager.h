#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <atomic>

void start_web_server_sta();
void start_web_server_ap();

// Флаг: веб-сервер загружает большой файл (приостановить audio)
// ATOMIC: используется между async веб-сервером и main loop
extern std::atomic<bool> web_loading_html;

#endif // WEB_SERVER_MANAGER_H
