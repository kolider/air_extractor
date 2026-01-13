#include "Sensor.h"
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include "index.h"

#include "AsyncJson.h"
#include <ArduinoJson.h>

#define WIFI_CONNECTION_TIMEOUT_MS 2000
#define SSID_STA "WiFi-SSID"; //Station Point SSID
#define PASSWD_STA NULL; //passwd to remote Station
#define SSID_AP "BathFAN"; //SoftAP SSID
#define PASSWD_AP NULL;  //passwd to local softAP

#define INTERVAL_UPDATE_MS 500
#define COUNT_MEASURES 120 // for EMA calculating INTERVAL_CHECK_MS * COUNT_MEASURES... equivalent St​=α⋅Xt​+(1−α)⋅St−1​
#define HUMIDITY_THRESOLD_ON 2 //in %
#define HUMIDITY_THRESOLD_OFF 1 //in %
#define MAX_FAN_RUNNING 12 * 60 * 60 * 1000UL // MAX continuous run time 

#define RELAY_PIN 14 //This D5 or GPIO14

#define RELAY_ON false
#define RELAY_OFF true

#define SENSOR_ERROR 0
#define FAN_ON_HUMIDITY 1
#define FAN_ON_TIMER 2
#define FAN_OFF 3

unsigned long duration_off_ms = 0;
int Fan_mode;


unsigned long wifi_t_ms=0, sensor_upd_ms=0;

const char* ssid = SSID_STA; //Station Point SSID
const char* passwd = PASSWD_STA; //Station Point SSID

const char* ap_ssid = SSID_AP; //Access Point SSID
const char* ap_passwd = PASSWD_AP;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

//Si7020 Temp & Hum sensor initial
float temperature;
float humidity;
float ema_humidity;
float start_humidity;
unsigned long start_fan_ms;
bool heating = false;
bool apEnabled = false;

bool is_notified = false;

Sensor THSensor;

void doRelayOn(bool state = RELAY_ON){ 
  digitalWrite(RELAY_PIN, state);
  if (state){
    pinMode(RELAY_PIN, INPUT);
  }else{
    pinMode(RELAY_PIN, OUTPUT);
  }  
}

void setup() {
  Serial.begin(115200);
  doRelayOn(RELAY_OFF);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(ap_ssid);
  WiFi.begin(ssid, passwd);
  wifi_t_ms=millis();
  while(WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, passwd);
    if (millis() - wifi_t_ms >= WIFI_CONNECTION_TIMEOUT_MS){
      startSoftAP();
      break;
    }
  }
  
  if (!THSensor.begin()) {
    while (1)
    {
        doMode(SENSOR_ERROR);
        delay(5000);
    }
    
  }
  THSensor.update();
  start_humidity = ema_humidity = THSensor.getHumidity();
  doMode(FAN_OFF);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  initWebSocket();
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html; charset=utf-8", index_html, processor);
    response->addHeader("Cache-Control","no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache"); 
    response->addHeader("Expires", "-1");
    request->send(response);
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
}

void loop(){
  ws.cleanupClients();
  
  if (millis() - sensor_upd_ms > INTERVAL_UPDATE_MS){
      sensor_upd_ms = millis();
      processing(); //FAN actions
  }
  
  if (millis() - wifi_t_ms > WIFI_CONNECTION_TIMEOUT_MS){
    wifi_t_ms = millis();
    if (WiFi.status() == WL_CONNECTED){
      // STA connected → stopping SoftAP
      digitalWrite(LED_BUILTIN, LOW);
      if (apEnabled) {
        stopSoftAP();
      }
    }else{
      if (!apEnabled) {
        //STA disconnected → starting SoftAP
        digitalWrite(LED_BUILTIN, HIGH);
        startSoftAP();
      }
    }
  }
  if (!is_notified){
    notifyClients();
    is_notified = true;
  }
}

void processing(){
  THSensor.update();
  
  float _temperature = THSensor.getTemperature();
  float _humidity = THSensor.getHumidity();

  //for web-interface only BEGIN
  if (THSensor.isDrying() != heating){
    heating = THSensor.isDrying();
    is_notified = false;
  }
  if (THSensor.isReady()){
    ema_humidity = (ema_humidity * COUNT_MEASURES + _humidity)/(COUNT_MEASURES+1);
  }
  if (
    (_humidity - humidity >= 1 || _humidity - humidity <= -1) || 
    (_temperature - temperature >= 0.1 || _temperature - temperature <= -0.1 )
    ){
    humidity = _humidity;
    temperature = _temperature;
    is_notified = false;
  }
  //for web-interface only end

  switch (Fan_mode)
  {
  case FAN_OFF:
    start_humidity = ema_humidity;
    //sensitivity to sudden humidity
    if (THSensor.isReady() && _humidity - ema_humidity > HUMIDITY_THRESOLD_ON){
      doMode(FAN_ON_HUMIDITY);
    }
    break;
  case FAN_ON_HUMIDITY:
    if (THSensor.isReady()){ //when inner not heating do dehumiding
      if (_humidity - start_humidity <= HUMIDITY_THRESOLD_OFF){ 
        doMode(FAN_OFF);
      }
    }
    // when working too long
    if (millis() - start_fan_ms >= duration_off_ms){
      doMode(FAN_OFF);
      start_humidity = _humidity;
    }
    break;
  case FAN_ON_TIMER:
    if (millis() - start_fan_ms >= duration_off_ms){
      doMode(FAN_OFF);
    }
    break;
  default:
    break;
  }
}

void doMode(int mode){
  // when websocket request fan_on_timer with 0 sec that need off
  if (mode == FAN_ON_TIMER && duration_off_ms == 0){
    mode = FAN_OFF;
    ema_humidity = humidity;
  }
  if (mode != Fan_mode){
    is_notified = false;
    Fan_mode = mode;
  }
  switch (mode)
  {
  case FAN_OFF:
    doRelayOn(RELAY_OFF);
    break;
  case FAN_ON_HUMIDITY:
    duration_off_ms = MAX_FAN_RUNNING;
  case FAN_ON_TIMER:
    //fan on timer set when request from websocket
    doRelayOn(RELAY_ON);
    start_fan_ms = millis();
    break;
  case SENSOR_ERROR:
    for (int i=0; i<5; i++){
      doRelayOn();
      delay(400);
      doRelayOn(RELAY_OFF);
      delay(400);
    }
    break;
  default:
    break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void notifyClients() {
//  ws.textAll(String(temp_average(Tc_buff)));
  sendDataWs();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  //processing input massages
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0; //set EOL
    //here we get data from client
    // Target_temp = (float)get_digit(strstr((char*)data, "target_temp"));

    int off_timer_sec = get_digit(strstr((char*)data, "off_timer"));
    duration_off_ms = off_timer_sec * 1000UL;
    doMode(FAN_ON_TIMER);
    notifyClients(); 
  }
}

//parse input message through websocket
int get_digit(char* str) {
  char *p = str;
  long val;
  while(*p){
    if ( isdigit(*p) || ( (*p=='-'||*p=='+'||*p=='.'||*p==',') && isdigit(*(p+1))) ){
      val = strtol(p, &p, 10);
      break;

    }else{
      p++;
    }
  }
  return (int)val;
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
//      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      notifyClients();
      break;
    case WS_EVT_DISCONNECT:
//      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

unsigned long getTimerMs(){
  if (Fan_mode == FAN_OFF) return 0;
  return duration_off_ms - (millis() - start_fan_ms);
}

String processor(const String& var){
  if (var == "TEMP"){
    return String(temperature);
  }
  if (var == "HUM"){
    return String(humidity);
  }
  if (var == "TARGET_HUM"){
    return String(start_humidity);
  }
  if (var == "STATE"){
    return String(Fan_mode);
  }
  if (var == "TIMER"){
    return String(getTimerMs()/1000);
  }
  if (var == "HEATER"){
    return String(heating);
  }
  return String();
}

void sendDataWs(){
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["temp"] = String(temperature);
    root["hum"] = String(humidity);
    root["target_hum"] = String(start_humidity);
    root["state"] = String(Fan_mode);
    root["off_timer"] = String(getTimerMs()/1000);
    root["heating"] = String(heating);
    size_t len = root.measureLength();
    AsyncWebSocketMessageBuffer *buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
    if (buffer) {
        root.printTo((char *)buffer->get(), len + 1);
        ws.textAll(buffer);
    }
}

void stopSoftAP() {
  WiFi.softAPdisconnect(true);
  apEnabled = false;

  WiFi.mode(WIFI_STA); //SoftAP stopped
}

void startSoftAP() {
  WiFi.mode(WIFI_AP_STA); // одночасно AP + STA

  if (WiFi.softAP(ap_ssid, ap_passwd)) {
    apEnabled = true;
  }
}
