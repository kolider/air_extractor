#include "Adafruit_Si7021.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

//Si7020 Temp & Hum sensor initial
float temperature;
float humidity;
bool enableHeater = false;
uint8_t loopCnt = 0;
unsigned long lastRefresh = 0;
uint8_t humi_fan_on = 65;
uint8_t humi_fan_off = 60;

Adafruit_Si7021 sensor = Adafruit_Si7021();

// Wifi connection, webServer and Update FW via OTA
#ifndef STASSID
#define STASSID "ssid_name"
#define STAPSK  "ssid_password"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// MQTT initial
#define AIO_SERVER      "address.to.mqttserver.com"
#define AIO_SERVERPORT  8883                   // use 8883 for SSL
#define AIO_USERNAME    "bathroom"
#define AIO_KEY         "pass"
#define PUBLISH_INTERVAL   60000 // now not implemented

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Publish temp_pub_mqtt = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");

Adafruit_MQTT_Publish humi_pub_mqtt = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");

Adafruit_MQTT_Subscribe fan_sub_mqtt = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/fanmode");

Adafruit_MQTT_Subscribe low_thr_hum_sub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/low_thr_hum");
Adafruit_MQTT_Publish low_thr_hum_pub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/low_thr_hum");

Adafruit_MQTT_Subscribe high_thr_hum_sub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/high_thr_hum");
Adafruit_MQTT_Publish high_thr_hum_pub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/high_thr_hum");

#define RELAY_PIN 14 //This D5 or GPIO14
#define RELAY_ON false
#define RELAY_OFF true
#define FAN_AUTO 1
#define FAN_OFF 2
#define FAN_ON 3
uint8_t Fan_mode = FAN_AUTO;

void setupOTA() {
  // set WiFi mode to station mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP", "0664041848");

  // set a device hostname
  // this is not required, but helps in recognizing the device on the network
  // MY_HOSTNAME is defined in config.h
  WiFi.hostname("bathroom-esp8266.local");

  // actually connect to wifi
  // again, WIFI_SSID and WIFI_PASSWORD are defined in config.h
  WiFi.begin(STASSID, STAPSK);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(STASSID, STAPSK);
  }

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // initialize the web server & the OTA updater
  httpUpdater.setup(&httpServer, "admin", "12345");
  httpServer.begin();
}

void rootHandle(){
  httpServer.send(200, "text/html", "hello worlds. <a href='/update'>Update firmware</a>");  
}

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         ESP.restart();
          return;
       }
  }
  Serial.println("MQTT Connected!");
}

void actionFAN(bool state){ 
  digitalWrite(RELAY_PIN, state);
  if (state){
    pinMode(RELAY_PIN, INPUT);
  }else{
    pinMode(RELAY_PIN, OUTPUT);
  }  
}

void setupFAN()
{    
    actionFAN(RELAY_OFF); //in func set pinMode

    rememberFanMode();
    rememberHumThreshold();
}

void updateSensor() {
  humidity = sensor.readHumidity();
  temperature = sensor.readTemperature();
}

void sensorCorrMetering(){
  // Toggle heater enabled state every 30 seconds
  // An ~1.8 degC temperature increase can be noted when heater is enabled
  if (++loopCnt == 6) {
    enableHeater = !enableHeater;
    if (enableHeater)
      updateSensor(); //Real temp & humidity metering before heater
    sensor.heater(enableHeater);
    Serial.print("Heater Enabled State: ");
    if (sensor.isHeaterEnabled())
      Serial.println("ENABLED");
    else
      Serial.println("DISABLED");
    loopCnt = 0;
  }
}

bool needRefresh(){
  unsigned long ms = millis();
  if ( ms < lastRefresh || ms - lastRefresh > 1000){
    lastRefresh = ms;
    return true;
  }
  return false;
}

void autoModeFAN(){
  if (humidity >= humi_fan_on){
    actionFAN(RELAY_ON);
  }else if(humidity <= humi_fan_off){
    actionFAN(RELAY_OFF);
  }
}
void FanModeHandler(){
  switch (Fan_mode){
    case FAN_AUTO:
      autoModeFAN();
      break;
    case FAN_OFF:
        actionFAN(RELAY_OFF);
      break;
    case FAN_ON:
        actionFAN(RELAY_ON);
      break;
    default:
      return;
  }
}

void saveFanMode(){
  //write Fan_mode to eeprom
  EEPROM.write(0, Fan_mode); //address 0
  EEPROM.commit();
}

void rememberFanMode(){
  Fan_mode = EEPROM.read(0); //address 0
}

void saveHumThreshold(){
  EEPROM.write(1, humi_fan_on); //address 1
  EEPROM.write(1, humi_fan_off); //address 2
  EEPROM.commit();
}

void rememberHumThreshold (){
  humi_fan_on = EEPROM.read(1); //address 1
  humi_fan_off = EEPROM.read(2); //address 2
}

void applyLowHumThreshold(uint8_t value){
   if(value){
     humi_fan_off = value;
     saveHumThreshold();
  }else{
    low_thr_hum_pub.publish(humi_fan_off);
  }
}

void applyHighHumThreshold(uint8_t value){
  if(value){
    humi_fan_on = value;
    saveHumThreshold(); 
  }else{
    high_thr_hum_pub.publish(humi_fan_on);
  }
}

void applyFanMode(uint8_t mode){
  switch (mode){
    case FAN_AUTO:
      Fan_mode = FAN_AUTO;
      break;
    case FAN_OFF:
      Fan_mode = FAN_OFF;
      actionFAN(RELAY_OFF); // for quick reaction
      break;
    case FAN_ON:
      Fan_mode = FAN_ON;
      actionFAN(RELAY_ON); // for quick reaction
      break;
    default:
    //error parse Fan_mode
    return;
   }
   saveFanMode();
}

void MQTT_Handler(){
  MQTT_connect();

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) { // need to issue. Stoping on this point
    if (subscription == &fan_sub_mqtt) {
      applyFanMode(atoi((char *)fan_sub_mqtt.lastread));
    }
    if (subscription == &high_thr_hum_sub) {
      applyHighHumThreshold(atoi((char *)high_thr_hum_sub.lastread));
    }
    if (subscription == &low_thr_hum_sub) {
      applyLowHumThreshold(atoi((char *)low_thr_hum_sub.lastread));
    }
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  setupOTA();
  httpServer.on("/", rootHandle);
  
  if (!sensor.begin()) {
    Serial.println("Did not find Si7021 sensor!");
  }
  
  updateSensor();
  setupFAN();
  mqtt.subscribe(&fan_sub_mqtt);
  mqtt.subscribe(&high_thr_hum_sub);
  mqtt.subscribe(&low_thr_hum_sub);
}

void loop() {  
  if(needRefresh()){
    sensorCorrMetering();
    if (! temp_pub_mqtt.publish(temperature)) {
      Serial.println(F("Failed"));
    }
    if (! humi_pub_mqtt.publish(humidity)) {
      Serial.println(F("Failed"));
    }
    Serial.print("Humidity:    ");
    Serial.print(humidity, 2);
    Serial.print("\tTemperature: ");
    Serial.println(temperature, 2);
  }

  FanModeHandler();
    
  httpServer.handleClient();

  MQTT_Handler();
  
}
