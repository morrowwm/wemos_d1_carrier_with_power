const char AP_SSID[] = "proofer";
const char AP_PASSWORD[] = "riseup";

#define WLAN_SSID1       "XXX"
#define WLAN_PASS1       "YYY"


#define MQTT_TOPIC      "ha/proofer"
#define MQTT_USERNAME    "proofer"
#define MQTT_PASSWD      "riseupriseup"

IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(47, 55, 55, 55);

const char* mqtt_server = "192.168.0.12";
const char *OTAName = "proofer";         // A name and a password for the OTA service
const char *OTAPassword = "warmenoughforya";
const char* mdnsName = "ProofESP8266";    // Domain name for the mDNS responder
