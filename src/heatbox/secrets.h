const char AP_SSID[] = "heatbox";
const char AP_PASSWORD[] = "itshot";

#define WLAN_SSID1       "XXX"
#define WLAN_PASS1       "YYY"


#define MQTT_TOPIC      "ha/heatbox"
#define MQTT_USERNAME    "heater"
#define MQTT_PASSWD      "warmenoughforya"

IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(47, 55, 55, 55);

const char* mqtt_server = "192.168.0.12";
const char *OTAName = "heater";         // A name and a password for the OTA service
const char *OTAPassword = "warmenoughforya";
const char* mdnsName = "HeatESP8266";    // Domain name for the mDNS responder
