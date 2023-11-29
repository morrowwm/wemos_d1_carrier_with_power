#include <Arduino.h>
#include <Hash.h>
#include <OneWire.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

#include "secrets.h"
#include "page_html.h"
const char* PARAM_SET = "target";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

AsyncWebServer server(80);

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

time_t now;

#define ONE_WIRE_BUS D1
OneWire  oneWire(ONE_WIRE_BUS);  // on pin D1 (a 4.7K resistor is necessary)

#define FAN      D8

const unsigned long intervalNTP = 3600UL; // Update the time every hour
const unsigned long dataInterval = 5UL;   // Do a temperature measurement every N seconds

unsigned long nextSearchTime[2] = {0UL, 0UL};
unsigned long nextReadTime[2] = {0UL, 0UL};
unsigned long nextPublishTime = 0UL;

int fanSpeed = 128;

float deltaTemperature;
float PID_error = 0.0;
float previous_error = 0.0;
unsigned long elapsedTime, nowTime = 0, prevTime = 0;
float PID_value = 0.0;

//PID constants
float kp = 1000.0; float ki = 0.1; float kd = 100.0; 
float PID_proportional = 0.0; float PID_integral = 0.0; float PID_differential = 0.0;
float performanceMetric, oldMetric;

static const char PROGMEM JSONtemplate[] = R"({"TIME":%ld,"dT":%4.2f,"TEin":%4.2f,"TEout":%4.2f,"Fan":%d,"PM":%4.2f})";
static const char PROGMEM INFOtemplate[] = R"( Setting target temperature to: %f)";
static const char PROGMEM SYSINFOtemplate[] = R"( IP address: %s)";

char dataMessage[160];

void wifi_setup(void) {
  WiFi.mode(WIFI_STA);

  WiFi.hostname("heater");
  WiFi.begin(WLAN_SSID1, WLAN_PASS1);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println('\n');
  Serial.print("Connected to AP ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer
}

void ap_setup(void) {
  WiFi.disconnect(true);
  delay(2000);
  // IPAddress local_IP(192,168,4,22);
  // IPAddress gateway(192,168,4,9);
  // IPAddress subnet(255,255,0,0);

  //Serial.print("Setting soft-AP configuration ... ");
  //Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  WiFi.mode(WIFI_AP);
    
  if(WiFi.softAP("heater")){
    Serial.println("AP is true");
  }
  else{
    Serial.println("AP failed!");
}
  Serial.print("WAP is "); Serial.println(AP_SSID);
  Serial.println(WiFi.softAPIP());
}

void ota_setup(void){
  Serial.println("Setting up OTA...");
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 1);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  Serial.println(messageTemp);
  if (String(topic) == "ha/heater/cmd") {
    char info[64];

    if(messageTemp.substring(0,8) == "SETTEMP:"){
      deltaTemperature = messageTemp.substring(8).toFloat();
      snprintf_P(info, sizeof(info), INFOtemplate, deltaTemperature);
    }
    else if(messageTemp.substring(0,8) == "INFO"){
      snprintf_P(info, sizeof(info), SYSINFOtemplate, WiFi.localIP().toString().c_str());
    }
    else if(messageTemp.substring(0,5) == "CLEAR"){
      PID_proportional = 0.0; PID_integral = 0.0; PID_differential = 0.0;
      strcpy(info, "Cleared PID factors");

    }
    mqttClient.publish("ha/heater/info", info);
  }
}

void mqtt_setup() {
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(mdnsName, MQTT_USERNAME, MQTT_PASSWD)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("env", "Connect from MOSQ_USER");
      mqttClient.subscribe("ha/heater/cmd");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}

void web_setup(){
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", INDEX_HTML);
    });

    // Send a GET request to <IP>/data
    server.on("/data", HTTP_GET, [] (AsyncWebServerRequest *request) {
        request->send(200, "text/plain", dataMessage);
        Serial.println("Sent data message on request");
    });

    server.on("/setfan", HTTP_POST, [](AsyncWebServerRequest *request){
      Serial.print("****** Got sent: ");
      int params = request->params();
      for(int i=0; i<params; i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isFile()){ //p->isPost() is also true
          Serial.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
        } else if(p->isPost()){
          Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
          fanSpeed = 0+((256-32)*std::stoi(p->value().c_str())/100); // fan doesn't move below PWM of 32
        } else {
          Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      request -> send(200);
    });

    server.on("/code/highcharts.js", HTTP_GET, [](AsyncWebServerRequest *request){
      Serial.println("Serving highcharts.js");
      request->send(LittleFS, "/code/highcharts.js", "text/javascript");
    });

    server.onNotFound(notFound);

    server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println(F("    wi-fi..."));
  wifi_setup();
  Serial.println(F("    OTA..."));
  ota_setup();
  Serial.println(F("    AP..."));
  //ap_setup();
  Serial.println(F("    MQTT..."));
  mqtt_setup();
  Serial.println(F("    WWW..."));
  web_setup();

  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  // Initialize an NTPClient to get time
  timeClient.begin();

  // Set offset time in seconds to adjust for your timezone, for example:
  //timeClient.setTimeOffset(-14400);

  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  Serial.print("Time from NTP: "); Serial.println(now);

  pinMode(FAN, OUTPUT);

  //ROM = 28 1B 64 67 3 0 0 41
  //ROM = 28 92 47 E1 2 0 0 F0

}

long nextMsgTime = 0;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID1);

  WiFi.begin(WLAN_SSID1, WLAN_PASS1);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void mqtt_loop() {
  if (!mqttClient.connected()) {
    reconnect();
    delay(2000);
  }
  mqttClient.loop();
}

byte i;
byte present = 0;
byte type_s;
byte data[9];
 
boolean dataReady = false;
byte addr[][8]  = {{0x28, 0x1B, 0x64, 0x67, 0x03, 0x00, 0x00, 0x41},
                {0x28, 0x92, 0x47, 0xE1, 0x02, 0x00, 0x00, 0xF0}};

void loop() {
  char topic[32];
  float temperature[2];
  byte i;
  int sensor;

  unsigned long now = timeClient.getEpochTime();

  for(sensor = 0; sensor < 2; sensor++){
    if(now > nextSearchTime[sensor]){
      nextSearchTime[sensor] = now + dataInterval;
      nextReadTime[sensor] = now + 1;
      dataReady = false;
      oneWire.reset();
      oneWire.select(addr[sensor]);
      oneWire.write(0x44, 1);        // start conversion, with parasite power on at the end
    }

    if(now > nextReadTime[sensor]){
      nextReadTime[sensor] = now + 1;
      present = oneWire.reset();
      oneWire.select(addr[sensor]);    
      oneWire.write(0xBE);         // Read Scratchpad
      for ( i = 0; i < 9; i++) {           // we need 9 bytes
        data[i] = oneWire.read();
        // Serial.print(data[i], HEX);
        // Serial.print(" ");
      }

      int16_t raw = (data[1] << 8) | data[0];
      if (type_s) {
        raw = raw << 3; // 9 bit resolution default
        if (data[7] == 0x10) {
          // "count remain" gives full 12 bit resolution
          raw = (raw & 0xFFF0) + 12 - data[6];
        }
      } else {
        byte cfg = (data[4] & 0x60);
        // at lower res, the low bits are undefined, so let's zero them
        if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
        else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
        else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
        //// default is 12 bit resolution, 750 ms conversion time
      }
      temperature[sensor] = (float)raw / 16.0;
      if(sensor > 0){
        dataReady = true;
      }
    }
  }

  if(dataReady){
    dataReady = false;
    deltaTemperature = temperature[1]- temperature[0];

    // PID_error = deltaTemperature;
    // PID_proportional = kp * PID_error;
    // PID_integral = 0.0; //PID_integral + (ki * PID_error);
    // PID_differential = 0.0; //kd * (PID_error - previous_error)/(0.001*(nowTime - prevTime)); // don't forget conversion from ms to seconds
    // PID_value = PID_proportional + PID_integral + PID_differential;
    
    previous_error = PID_error;
    prevTime = nowTime;

    // fanSpeed = fanSpeed + 2; //int(PID_value);
    if(fanSpeed < 0){ fanSpeed = 0;}
    if(fanSpeed > 255){ fanSpeed = 2;}
    
    oldMetric = performanceMetric; // use for maximum power point tracking (MPTT)?
    performanceMetric = deltaTemperature * (float)fanSpeed;
    
    analogWrite(FAN, fanSpeed); // MOSFET fires on high level

    size_t len = snprintf(topic, sizeof(topic), MQTT_TOPIC);
    if (len >= sizeof(topic)) {
      Serial.println("\n\n*** MQTT topic truncated ***\n");
    }

    len = snprintf_P(dataMessage, sizeof(dataMessage), JSONtemplate, now, 
      deltaTemperature, temperature[0], temperature[1], fanSpeed, performanceMetric);
  
    Serial.print("Publish : "); Serial.print(topic);
    Serial.print(": "); Serial.println(dataMessage);
    if(!mqttClient.publish(topic, dataMessage)) {
      Serial.println("\n\n*** mqtt publish failed ***\n");
    }
  }

  ArduinoOTA.handle();
  mqtt_loop();
}