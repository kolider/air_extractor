Smart air extractor control FAN using humidity sensor in "auto" mode. 
Project tested on NodeMCU Amica & used Arduino IDE.
Firmware have:
- WIFI client & Access Point modes
- OTA update firmware
- MQTT remote control

v0.1:
- humidity sensor based Si7020/21 (SHU21/HTU21);
- hardware ESP8266(ESP-12E);
- 5v relay support (low current);
- set humidity trigger with gisteresis via MQTT for "auto mode";
- send value of temperature & humidity via MQTT;
- save last hum.value control & mode to EEPROM (flash)

v0.2:
- fix save EEPROM for low value humidity
- MQTT update sensor value by constant interval

v1.0
- Most of the code has been rewritten
- Convenient web interface running on web sockets:
    - 4 button with different timer for manual contol FAN
    - animation of working
- softAP is disabled if the station is available
- EMA calculation is used to detect sudden humidification from a hot shower
- EEPROM is not used
- Relay clicking on sensor error
- Optimized use of the sensor's internal heater to dry the sensor surface
- OTA-update support by address http://BathFAN.local/upload || http://192.168.4.1/upload for softAp
- MQTT is not supported yet
- Hot connect to WIFI-Station requested from softAP
