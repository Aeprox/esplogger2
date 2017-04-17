// defines for network ssid and password
#define SSID "Your ssid"
#define PASSWORD "Your network password"

// defines for DHT type and pin
#define DHTPIN 13
#define DHTTYPE DHT22

// defines for TSL2561 pins
#define SDAPIN 14
#define SCLPIN 12

// defines for thingspeak MQTT connection
#define MQTTSERVER "mqtt.thingspeak.com"
#define CHANNELID "your channel ID"
#define APIKEY  "your api key"

// defines MQTT admin connection
#define MQTTADMINSERVER "192.168.1.252"
#define MQTTADMINSERVERPORT 1883
#define MQTTADMINTOPIC "templogger/admin"

//update & measurement inverval (in seconds)
#define DEFAULTMEASUREMENTINTERVAL 60
//# of measurementintervals before update
#define DEFAULTNUMMEASUREMENTS 5

//uncomment to enable deepsleep
//#define SLEEP
