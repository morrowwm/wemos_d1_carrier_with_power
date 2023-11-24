#include <Arduino.h>
#include <Hash.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <AHT10.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>

#include "secrets.h"
#include "page_html.h"
const char* PARAM_SET = "target";

AHT10 aht20(AHT10_ADDRESS_0X38, AHT20_SENSOR);

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

AsyncWebServer server(80);

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

time_t now;

#define HEATLAMP      D8

const unsigned long intervalNTP = 3600UL; // Update the time every hour
const unsigned long dataInterval = 2UL;   // Do a temperature measurement every N seconds

unsigned long nextReadTime = 0UL, nextPublishTime = 0UL;
int heating = 255;

float targetTemperature;
float PID_error = 0.0;
float previous_error = 0.0;
unsigned long elapsedTime, nowTime = 0, prevTime = 0;
float PID_value = 0.0;

//PID constants
float kp = 1000.0; float ki = 0.1; float kd = 100.0; 
float PID_proportional = 0; float PID_integral = 0; float PID_differential = 0;

static const char PROGMEM JSONtemplate[] = R"({"TIME":%ld,"HU":%4.2f,"TGT":%4.2f,"TE":%4.2f, "E":%f, "P":%f, "D":%f, "I":%f, "PID":%f, "HEAT":%d})";
static const char PROGMEM INFOtemplate[] = R"( Setting target temperature to: %f)";
static const char PROGMEM SYSINFOtemplate[] = R"( IP address: %s)";

char dataMessage[160];


void wifi_setup(void) {
  WiFi.mode(WIFI_STA);

  WiFi.hostname("proofer");
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

  WiFi.mode(WIFI_AP);
    
  if(WiFi.softAP("proofer")){
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
  if (String(topic) == "ha/proofer/cmd") {
    char info[64];

    if(messageTemp.substring(0,8) == "SETTEMP:"){
      targetTemperature = messageTemp.substring(8).toFloat();
      snprintf_P(info, sizeof(info), INFOtemplate, targetTemperature);
    }
    else if(messageTemp.substring(0,8) == "INFO"){
      snprintf_P(info, sizeof(info), SYSINFOtemplate, WiFi.localIP().toString().c_str());
    }
    else if(messageTemp.substring(0,5) == "CLEAR"){
      PID_proportional = 0.0; PID_integral = 0.0; PID_differential = 0.0;
      strcpy(info, "Cleared PID factors");

    }
    mqttClient.publish("ha/proofer/info", info);
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
      mqttClient.subscribe("ha/proofer/cmd");
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
    });

    server.on("/set", HTTP_POST, [](AsyncWebServerRequest *request){
      String message;
      if (request->hasParam(PARAM_SET, true)) {
          message = request->getParam(PARAM_SET, true)->value();
          targetTemperature = message.toFloat();
      } else {
          message = "No message sent";
      }
      //request->send(200, "text/html", INDEX_HTML);
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

  pinMode(HEATLAMP, OUTPUT);

  if(aht20.begin() == true){
    Serial.println(F("AHT2x connected")); 
  } 
  else{
    Serial.println(F("AHT2x not connected or fail to load calibration coefficient")); 
  }
  targetTemperature = aht20.readTemperature(); 

  Serial.print("\nAll set up, initial target temperature: "); Serial.println(targetTemperature);
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

void loop() {
  char topic[32];

  unsigned long now = timeClient.getEpochTime();
  nowTime = millis();

  if(now > nextReadTime){
    nextReadTime = now + dataInterval;

    float temperature = aht20.readTemperature(); //read 6-bytes via I2C, takes 80 milliseconds
    float humidity = aht20.readHumidity();

    PID_error = targetTemperature - temperature;
    PID_proportional = kp * PID_error;
    PID_integral = PID_integral + (ki * PID_error);
    PID_differential = kd * (PID_error - previous_error)/(0.001*(nowTime - prevTime)); // don't forget conversion from ms to seconds
    PID_value = PID_proportional + PID_integral + PID_differential;
    
    previous_error = PID_error;
    prevTime = nowTime;

    heating = int(PID_value);
    if(heating < 0){ heating = 0;}
    if(heating > 255){ heating = 255;}
    
    analogWrite(HEATLAMP, heating); // MOSFET fires on high level

    size_t len = snprintf(topic, sizeof(topic), MQTT_TOPIC);
    if (len >= sizeof(topic)) {
      Serial.println("\n\n*** MQTT topic truncated ***\n");
    }

    len = snprintf_P(dataMessage, sizeof(dataMessage), JSONtemplate, now, humidity, targetTemperature, temperature, PID_error,
      PID_proportional, PID_differential, PID_integral, PID_value, heating);
  
    Serial.print("Publish : "); Serial.print(topic);
    Serial.print(": "); Serial.println(dataMessage);
    if(!mqttClient.publish(topic, dataMessage)) {
      Serial.println("\n\n*** mqtt publish failed ***\n");
    }
  }

  ArduinoOTA.handle();
  mqtt_loop();
}
