// defines for network ssid and password
#define SSID "Your ssid"
#define PASSWORD "Your network password"

// defines for DHT type and pin
#define DHTPIN 13
#define DHTTYPE DHT22

// defines for TSL2561 pins
#define SDAPIN 14
#define SCLPIN 12

// defines for thingspeak connection
#define MQTTSERVER "mqtt.thingspeak.com"
#define CHANNELID "your channel ID"
#define APIKEY  "our api key"

//update & measurement inverval (in seconds)
#define MEASUREMENTINTERVAL 10
//# of measurementintervals before update
#define NUMMEASUREMENTS 3


//uncomment to enable deepsleep
//#define SLEEP
