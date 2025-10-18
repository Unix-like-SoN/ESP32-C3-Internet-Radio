#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

enum SystemState {
    STATE_STA,      // Режим станции (подключен к Wi-Fi)
    STATE_AP,       // Режим точки доступа
    STATE_OFF       // Состояние "выключено"
};

extern SystemState systemState;

void check_power_stability();  // Проверка стабильности питания при старте
void check_reset_reason();     // Проверка причины сброса (вкл. brown-out)
void loop_system_tasks();
void print_system_status();
void shutdown_system();

#endif // SYSTEM_MANAGER_H
