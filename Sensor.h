#pragma once
#include <Arduino.h>
#include <Adafruit_Si7021.h>

class Sensor {
private:
    Adafruit_Si7021 _si7021;

    // Таймінги обслуговування
    const unsigned long DRYING_DURATION = 60 * 1000UL;           // 1 хв нагріву
    const unsigned long STABILIZATION_DURATION = 60 * 1000UL;    // 1 хв остигання
    const unsigned long DRYING_INTERVAL = 12 * 60 * 60 * 1000UL;  // 12 год (план)
    const unsigned long HIGH_HUM_COOLDOWN = 5 * 60 * 1000UL;     // 5 хв пауза між сушками > 90%

    unsigned long _lastDryingFinishedTime = 0;   
    unsigned long _dryingStartTime = 0;         
    unsigned long _lastHighHumTriggerTime = 0;  
    
    bool _isDrying = false;
    bool _isWaitingForStabilization = false;

    float _temperature = 0;
    float _humidity = 0;

public:
    Sensor() : _si7021() {}

    bool begin() {
        if (!_si7021.begin()) return false;
        _lastDryingFinishedTime = millis();
        return true;
    }

    void update() {
        unsigned long currentTime = millis();

        // 1. КОНТРОЛЬ НАГРІВАЧА (СУШКА)
        if (_isDrying && (currentTime - _dryingStartTime >= DRYING_DURATION)) {
            // Sensor: Heater OFF. Cooling down...
            _si7021.heater(false);
            _isDrying = false;
            _isWaitingForStabilization = true;
            _lastDryingFinishedTime = currentTime;
        }

        // 2. КОНТРОЛЬ ОСТИГАННЯ (СТАБІЛІЗАЦІЯ)
        if (_isWaitingForStabilization && (currentTime - _lastDryingFinishedTime >= STABILIZATION_DURATION)) {
            // Sensor: Stabilization finished. Readings resumed
            _isWaitingForStabilization = false;
        }

        // 3. РОБОЧИЙ РЕЖИМ
        if (!_isDrying && !_isWaitingForStabilization) {
            _temperature = _si7021.readTemperature();
            _humidity = _si7021.readHumidity();

            // 4. ПЕРЕВІРКА УМОВ ДЛЯ НОВОЇ СУШКИ
            bool timeForPlan = (currentTime - _lastDryingFinishedTime >= DRYING_INTERVAL);
            bool isCritical = (_humidity > 90.0);
            bool cooldownPassed = (currentTime - _lastHighHumTriggerTime >= HIGH_HUM_COOLDOWN);

            if (timeForPlan || (isCritical && cooldownPassed)) {
                //Sensor: Drying started..
                if (isCritical) _lastHighHumTriggerTime = currentTime;
                
                _si7021.heater(true);
                _isDrying = true;
                _dryingStartTime = currentTime;
            }
        }
    }

    // Гетери для отримання "сирих" значень
    float getTemperature() { return _temperature; }
    float getHumidity() { return _humidity; }
    
    // Статус сенсора
    bool isDrying() { return _isDrying; }
    bool isWaiting() { return _isWaitingForStabilization; }
    bool isReady() { return (!_isDrying && !_isWaitingForStabilization); }
};