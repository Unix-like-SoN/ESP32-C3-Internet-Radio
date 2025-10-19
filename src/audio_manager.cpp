#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include "config.h"
#include "audio_manager.h"
#include "display_manager.h"
#include "log_manager.h"
#include "wifi_manager.h"
#include "url_validator.h"
#include "string_utils.h"
#include <WiFi.h>

// --- РЕАЛИЗАЦИЯ ВИЗУАЛИЗАТОРА ЧЕРЕЗ ConsumeSample ---

void process_audio_data_for_visualizer(const int16_t *data, int len);

class AudioOutputWithVisualizer : public AudioOutputI2S {
private:
    int16_t sample_buffer[VISUALIZER_SAMPLE_BUFFER];
    int buffer_pos = 0;
    
    // 🛡️ Счетчик переполнений для диагностики
    static unsigned long overflow_count;
    static unsigned long last_overflow_warning;

public:
  AudioOutputWithVisualizer() : AudioOutputI2S() {}

  virtual bool ConsumeSample(int16_t sample[2]) override {
    // 🛡️ Защита от переполнения буфера
    if (buffer_pos < VISUALIZER_SAMPLE_BUFFER) {
        sample_buffer[buffer_pos++] = sample[0];
    } else {
        // Буфер переполнен - логируем раз в WARNING_LOG_INTERVAL
        overflow_count++;
        if (millis() - last_overflow_warning > WARNING_LOG_INTERVAL) {
            Serial.printf("⚠️ Visualizer buffer overflow: %lu times\n", overflow_count);
            last_overflow_warning = millis();
            overflow_count = 0;
        }
    }

    if (buffer_pos >= VISUALIZER_SAMPLE_BUFFER) {
        // 🛡️ Безопасная обработка с проверкой стека внутри
        process_audio_data_for_visualizer(sample_buffer, buffer_pos * sizeof(int16_t));
        buffer_pos = 0;
    }
    
    return AudioOutputI2S::ConsumeSample(sample);
  }
};

// Инициализация статических переменных
unsigned long AudioOutputWithVisualizer::overflow_count = 0;
unsigned long AudioOutputWithVisualizer::last_overflow_warning = 0;

// --- КОНЕЦ РЕАЛИЗАЦИИ ---

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceHTTPStream *file = nullptr;
AudioFileSourceBuffer *buff = nullptr;
AudioOutputWithVisualizer *out_with_visualizer = nullptr;

int currentStation = 0;
float volume = VOLUME_DEFAULT;

AudioState audioState = AUDIO_IDLE;
unsigned long audioStateTime = 0;

// 🎯 Флаг приоритета аудио над display
std::atomic<bool> audio_decoding_active(false);

// 🚫 Защита от повторных вызовов next_station во время переключения
static bool isChangingStation = false;

// ⚠️ Статус инициализации I2S
static bool i2sInitialized = false;
static unsigned long lastI2SInitAttempt = 0;

void cleanup_audio();
bool init_audio_non_blocking();
int find_available_station(int direction);
void mark_station_as_unavailable(int stationIndex);
void try_reinit_i2s();

void setup_audio() {
    // 🛡️ ЗАЩИТА ОТ УТЕЧКИ ПАМЯТИ: удаляем старый объект перед созданием нового
    if (out_with_visualizer) {
        delete out_with_visualizer;
        out_with_visualizer = nullptr;
        log_message("🔄 Предыдущий AudioOutput удален перед реинициализацией");
    }
    
    out_with_visualizer = new AudioOutputWithVisualizer();
    
    // ⚠️ Проверка инициализации I2S
    if (!out_with_visualizer->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT)) {
        log_message("⚠️ I2S не инициализирован! Повторные попытки каждые 5 секунд...");
        i2sInitialized = false;
        lastI2SInitAttempt = millis();
        return;
    }
    
    i2sInitialized = true;
    out_with_visualizer->SetGain(volume);
    log_message("✅ I2S инициализирован успешно (BCLK=GPIO4, LRC=GPIO5, DIN=GPIO6)");
}

// ⚠️ Попытка переинициализации I2S
void try_reinit_i2s() {
    if (i2sInitialized) return; // Уже инициализирован
    
    unsigned long now = millis();
    if (now - lastI2SInitAttempt < AUDIO_I2S_RETRY_INTERVAL) return; // Еще не прошло AUDIO_I2S_RETRY_INTERVAL
    
    lastI2SInitAttempt = now;
    log_message("🔄 Попытка переинициализации I2S...");
    
    // 🛡️ GRACEFUL CLEANUP: очищаем аудио перед реинициализацией
    // Предотвращаем конфликты с активным воспроизведением
    if (audioState != AUDIO_IDLE) {
        cleanup_audio();
        audioState = AUDIO_IDLE;
    }
    
    if (out_with_visualizer && out_with_visualizer->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT)) {
        i2sInitialized = true;
        out_with_visualizer->SetGain(volume);
        log_message("✅ I2S инициализирован после повторной попытки!");
    }
}

void IRAM_ATTR loop_audio() {
    // ⚠️ Проверка инициализации и попытка восстановления I2S
    try_reinit_i2s();
    if (!i2sInitialized) return; // I2S не готов, пропускаем аудио
    
    // 🛡️ Защита от race condition: проверяем stations.empty() под мьютексом
    STATIONS_LOCK();
    bool stationsEmpty = stations.empty();
    STATIONS_UNLOCK();
    
    if (stationsEmpty) {
        if (audioState != AUDIO_IDLE) {
            cleanup_audio();
            audioState = AUDIO_IDLE;
        }
        return;
    }

    if (audioState != AUDIO_PLAYING) {
        init_audio_non_blocking();
    }

    if (audioState == AUDIO_PLAYING && mp3 && mp3->isRunning()) {
        // 🎯 Устанавливаем приоритет на время декодирования
        audio_decoding_active.store(true, std::memory_order_relaxed);
        
        if (!mp3->loop()) {
            log_message("Поток завершен");
            audioState = AUDIO_IDLE;
        }
        
        audio_decoding_active.store(false, std::memory_order_relaxed);
    } else {
        // Не декодируем - сбрасываем флаг
        audio_decoding_active.store(false, std::memory_order_relaxed);
    }
    
    if (audioState == AUDIO_CONNECTING || audioState == AUDIO_STARTING) {
        if (millis() - audioStateTime > AUDIO_CONNECTION_TIMEOUT) {
            log_message("Таймаут подключения к станции. Принудительный сброс.");
            audioState = AUDIO_ERROR;
        }
    }
}

void IRAM_ATTR next_station() {
    if (stations.empty() || isChangingStation) return;
    
    isChangingStation = true;  // 🚫 Защита от повторных вызовов
    
    audioState = AUDIO_IDLE;
    cleanup_audio();
    
    int oldStation = currentStation;
    currentStation = find_available_station(1);
    
    log_message(formatString("Next: %s -> %s", stations[oldStation].name.c_str(), stations[currentStation].name.c_str()));
    
    reset_inactivity_timer();
    audioState = AUDIO_IDLE;
    
    isChangingStation = false;  // Снимаем блокировку
}

void IRAM_ATTR previous_station() {
    if (stations.empty() || isChangingStation) return;
    
    isChangingStation = true;  // 🚫 Защита от повторных вызовов
    
    audioState = AUDIO_IDLE;
    cleanup_audio();
    
    int oldStation = currentStation;
    currentStation = find_available_station(-1);
    
    log_message(formatString("Prev: %s -> %s", stations[oldStation].name.c_str(), stations[currentStation].name.c_str()));
    
    reset_inactivity_timer();
    audioState = AUDIO_IDLE;
    
    isChangingStation = false;  // Снимаем блокировку
}

void IRAM_ATTR set_volume(float new_volume) {
    volume = new_volume;
    if (out_with_visualizer) {
        out_with_visualizer->SetGain(volume);
    }
    reset_inactivity_timer();
}

void force_audio_reset() {
    audioState = AUDIO_IDLE;
    cleanup_audio();
}

void cleanup_audio() {
    if (mp3) {
        mp3->stop();
        delete mp3;
        mp3 = nullptr;
    }
    if (buff) {
        delete buff;
        buff = nullptr;
    }
    if (file) {
        delete file;
        file = nullptr;
    }
}

bool init_audio_non_blocking() {
    // 🛡️ ЗАЩИТА ОТ RACE CONDITION: проверяем stations.empty() под мьютексом
    STATIONS_LOCK();
    bool stationsEmpty = stations.empty();
    STATIONS_UNLOCK();
    
    if (stationsEmpty) return false;
    
    // Блокируем новые подключения во время WiFi recovery
    if (wifiRecoveryState != WIFI_OK) {
        if (audioState != AUDIO_IDLE) {
            cleanup_audio();
            audioState = AUDIO_IDLE;
        }
        return false;
    }

    switch (audioState) {
        case AUDIO_IDLE: {
            if (WiFi.status() != WL_CONNECTED) return false;
            cleanup_audio();
            
            // 🛡️ Читаем название станции под мьютексом
            STATIONS_LOCK();
            String stationName = stations[currentStation].name;
            STATIONS_UNLOCK();
            
            show_message("Connecting to", stationName);
            log_message(formatString("Подключение: %s", stationName.c_str()));
            audioState = AUDIO_CONNECTING;
            audioStateTime = millis();
            return false;
        }
            
        case AUDIO_CONNECTING: {
            if (millis() - audioStateTime > AUDIO_STATE_DELAY) {
                // 🛡️ Читаем URL и название под мьютексом
                STATIONS_LOCK();
                String url = stations[currentStation].url;
                String stationName = stations[currentStation].name;
                STATIONS_UNLOCK();
                
                // 🛡️ ВАЛИДАЦИЯ URL (защита от уязвимостей)
                URLValidationResult validation = validateURL(url);
                
                if (validation != URL_VALID) {
                    // URL невалиден - логируем ошибку и помечаем станцию недоступной
                    String errorMsg = getValidationErrorMessage(validation);
                    String safeURL = sanitizeURLForLog(url);
                    
                    log_message(formatString("⚠️ Невалидный URL: %s", safeURL.c_str()));
                    log_message(formatString("⚠️ Ошибка: %s", errorMsg.c_str()));
                    
                    show_message("Invalid URL", stationName);
                    mark_station_as_unavailable(currentStation);
                    audioState = AUDIO_ERROR;
                    return false;
                }
                
                // URL валиден - используем санитизированную версию для логов
                String safeURL = sanitizeURLForLog(url);
                log_message(formatString("URL: %s", safeURL.c_str()));
                
                file = new AudioFileSourceHTTPStream(url.c_str());
                if (file) {
                    buff = new AudioFileSourceBuffer(file, AUDIO_BUFFER_SIZE);
                    if (buff) {
                        mp3 = new AudioGeneratorMP3();
                        if (mp3) {
                            audioState = AUDIO_STARTING;
                            audioStateTime = millis();
                        }
                    }
                }
                if (audioState != AUDIO_STARTING) {
                    audioState = AUDIO_ERROR;
                }
            }
            return false;
        }
            
        case AUDIO_STARTING: {
            if (mp3 && mp3->begin(buff, out_with_visualizer)) {
                out_with_visualizer->SetGain(volume);
                audioState = AUDIO_BUFFERING;  // Переходим к буферизации
                audioStateTime = millis();
                log_message("Декодер готов, заполняем буфер...");
                return false;
            }
            if (millis() - audioStateTime > AUDIO_START_TIMEOUT) {
                log_message("Таймаут запуска потока");
                audioState = AUDIO_ERROR;
            }
            return false;
        }
            
        case AUDIO_BUFFERING: {
            // Ждём заполнения буфера перед стартом
            if (buff && buff->getFillLevel() > AUDIO_BUFFER_SIZE * AUDIO_BUFFER_LOW_THRESHOLD / 100) {
                // Буфер заполнен на 20% - можно начинать
                audioState = AUDIO_PLAYING;
                stations[currentStation].isAvailable = true;
                log_message(formatString("Буфер заполнен (%d / %d), старт!", buff->getFillLevel(), AUDIO_BUFFER_SIZE));
                reset_inactivity_timer();
                return true;
            }
            
            // Таймаут буферизации
            if (millis() - audioStateTime > AUDIO_PREBUFFER_TIME) {
                log_message("Таймаут буферизации, запуск несмотря на низкий уровень");
                audioState = AUDIO_PLAYING;
                stations[currentStation].isAvailable = true;
                reset_inactivity_timer();
                return true;
            }
            return false;
        }
            
        case AUDIO_ERROR: {
            log_message("❌ Ошибка подключения к станции. Переключаюсь на следующую...");
            mark_station_as_unavailable(currentStation);
            next_station(); // Автоматически переключаем на следующую
            return false;
        }
            
        case AUDIO_PLAYING: {
            return true;
        }
    }
    return false;
}

// Универсальная функция поиска доступной станции
// direction: 1 = следующая, -1 = предыдущая
int find_available_station(int direction) {
    // 🛡️ ЗАЩИТА ОТ RACE CONDITION: захват мьютекса на всю функцию
    // Так как мы читаем/пишем stations в цикле, нужна атомарность
    STATIONS_LOCK();
    
    if (totalStations == 0) {
        STATIONS_UNLOCK();
        return 0;
    }
    if (totalStations == 1) {
        STATIONS_UNLOCK();
        return 0;  // Только одна станция
    }
    
    int attempts = 0;
    int station = currentStation;
    
    do {
        // Переходим к следующей/предыдущей станции
        if (direction > 0) {
            station = (station + 1) % totalStations;
        } else {
            station = (station - 1 + totalStations) % totalStations;
        }
        attempts++;
        
        // Если прошли полный круг - все станции недоступны
        if (attempts > totalStations) {
            log_message("⚠️ Все станции недоступны! Сбрасываю флаги.");
            
            // Сбрасываем все флаги isAvailable
            for (int i = 0; i < totalStations; i++) {
                stations[i].isAvailable = true;
            }
            
            STATIONS_UNLOCK();
            // ✅ ИСПРАВЛЕНИЕ БАГА: после сброса флагов возвращаем следующую/предыдущую станцию
            // А не последнюю из цикла (которая может совпадать с currentStation)
            if (direction > 0) {
                return (currentStation + 1) % totalStations;
            } else {
                return (currentStation - 1 + totalStations) % totalStations;
            }
        }
    } while (!stations[station].isAvailable);
    
    STATIONS_UNLOCK();
    return station;
}

void mark_station_as_unavailable(int stationIndex) {
    // 🛡️ ЗАЩИТА ОТ RACE CONDITION: захват мьютекса перед изменением stations
    STATIONS_LOCK();
    
    // 🛡️ Проверка границ массива (bounds check)
    if (stationIndex < 0 || stationIndex >= totalStations) {
        STATIONS_UNLOCK();
        log_message(formatString("⚠️ Попытка пометить несуществующую станцию: индекс %d", stationIndex));
        return;
    }
    
    stations[stationIndex].isAvailable = false;
    String stationName = stations[stationIndex].name; // Копия для лога
    
    STATIONS_UNLOCK();
    
    log_message(formatString("%s помечена как недоступная", stationName.c_str()));
}

// 🛡️ Защита от переполнения стека в визуализаторе
void process_audio_data_for_visualizer(const int16_t *data, int len) {
    // 1. Валидация входных параметров
    if (data == nullptr || len <= 0) return;
    
    // 2. Проверка свободного стека (min VISUALIZER_MIN_STACK байт)
    // Это критично т.к. функция вызывается из IRAM во время декодирования
    UBaseType_t freeStack = uxTaskGetStackHighWaterMark(NULL);
    
    if (freeStack < VISUALIZER_MIN_STACK) {
        // Недостаточно стека - пропускаем обработку
        static unsigned long lastWarning = 0;
        if (millis() - lastWarning > WARNING_LOG_INTERVAL) {
            Serial.printf("⚠️ Stack LOW: %u bytes (min %u)\n", freeStack, VISUALIZER_MIN_STACK);
            lastWarning = millis();
        }
        return; // Пропускаем визуализацию для безопасности
    }
    
    // 3. Валидация размера данных
    int sample_count = len / sizeof(int16_t);
    if (sample_count < VISUALIZER_BANDS) return; // Недостаточно данных для VISUALIZER_BANDS полос
    
    int band_size = sample_count / VISUALIZER_BANDS;
    if (band_size == 0) return;
    
    // 4. Проверка границ массива перед обработкой
    int max_index = (VISUALIZER_BANDS - 1) * band_size + (band_size - 1);
    if (max_index >= sample_count) {
        // Выход за границы - корректируем band_size
        band_size = sample_count / VISUALIZER_BANDS;
    }

    // 5. Безопасная обработка VISUALIZER_BANDS полос с проверкой границ
    for (int i = 0; i < VISUALIZER_BANDS; i++) {
        long total = 0;
        int start_idx = i * band_size;
        int end_idx = start_idx + band_size;
        
        // Дополнительная проверка границ для каждой полосы
        if (end_idx > sample_count) {
            end_idx = sample_count;
        }
        
        for (int j = start_idx; j < end_idx; j++) {
            total += abs(data[j]);
        }
        
        int actual_band_size = end_idx - start_idx;
        if (actual_band_size > 0) {
            long avg_amplitude = total / actual_band_size;
            // Ограничиваем значение для безопасности
            visualizerBands[i] = constrain(map(avg_amplitude, 0, VISUALIZER_MAX_AMPLITUDE, 0, SCREEN_HEIGHT), 0, SCREEN_HEIGHT);
        } else {
            visualizerBands[i] = 0;
        }
    }
}
