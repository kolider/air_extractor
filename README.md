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
