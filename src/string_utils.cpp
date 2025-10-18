#include "string_utils.h"
#include <cstdarg>

// Максимальный размер буфера для форматированных строк
#define FORMAT_BUFFER_SIZE 256

String formatString(const char* format, ...) {
    char buffer[FORMAT_BUFFER_SIZE];
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, FORMAT_BUFFER_SIZE, format, args);
    va_end(args);
    
    // Создаем String с точным размером, минимизируя фрагментацию
    String result;
    result.reserve(strlen(buffer) + 1);
    result = buffer;
    
    return result;
}
