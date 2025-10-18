#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>

void setup_logging();
void log_message(const String& message);
void clear_logs();
String get_logs();

#endif // LOG_MANAGER_H
