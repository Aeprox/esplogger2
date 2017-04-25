# Esplogger2

Rewrite of https://github.com/Aeprox/ESPLogger, using arduino frameworkfor esp8266 (with [platformio](http://platformio.org/)). Still using the DHT22 and TSL2561 sensors to measure temperature, humidity and light intensity and periodically send this data to a server.

Changes in rewrite:

* Use MQTT to upload to Thingspeak instead of HTTP
* Save sensor values to RTC memory and send average once every X measurements. Reduces power consumption.
* Use MQTT to upload to local private server, and fetch admin/config information from the MQTT server.

## Configuration and building/uploading

Configuring is done in [config.h](https://github.com/Aeprox/esplogger2/blob/master/src/config.h)

``` c++
#define SSID "Your ssid"
#define PASSWORD "Your network password"
```

Enter your network name and password. 

``` c++
// defines for thingspeak MQTT connection
#define MQTTSERVER "mqtt.thingspeak.com"
#define CHANNELID "your channel ID"
#define APIKEY  "your api key"
```
This is where you enter your channel ID and API write key, which you find in your Thingspeak channel settings.

``` c++
// defines MQTT admin connection
#define MQTTADMINSERVER "192.168.1.252"
#define MQTTADMINSERVERPORT 1883
#define MQTTADMINTOPIC "templogger/admin"
```
Private server to send data to. Data is published to templogger/output topic.

Also subscribes to the configured topic. The module will look for messages published in this topic (and subtopics) with topic `templogger/admin/num` and `templogger/admin/int`and parse their values to update the measurement interval, see below.

``` c++
//update & measurement inverval (in seconds)
#define DEFAULTMEASUREMENTINTERVAL 60
//# of measurementintervals before update
#define DEFAULTNUMMEASUREMENTS 5
```

The important bit. The module will take measurements every DEFAULTMEASUREMENTINTERVAL seconds, and upload the average after DEFAULTNUMMEASUREMENTS measurements. This means the module will update to the remote server once every (DEFAULTMEASUREMENTINTERVAL * DEFAULTNUMMEASUREMENTS) seconds.


**Building** is quite simple when you're using platformio to build this project, as it will automatically fetch the needed libraries from GitHub. Never tested it using any other IDE, but you'll need to manualy add these libraries:

  * https://github.com/adafruit/Adafruit_Sensor.git
  * https://github.com/adafruit/DHT-sensor-library.git#1.3.0
  * https://github.com/knolleary/pubsubclient.git#2.6
  * https://github.com/Aeprox/TSL2561-Arduino-Library.git (using my own fork of the adafruit library to allow configuration of the pins used by the TSL2561 sensor)
