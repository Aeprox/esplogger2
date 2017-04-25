# Esplogger2

Rewrite of https://github.com/Aeprox/ESPLogger, using arduino framework (with platformio). Still using ESP8266 module, DHT22 and TSL2561 sensor to measure temperature, humidity and light intensity and periodicaly send this data to a server.

Changes in rewrite:

* Use MQTT to upload to Thingspeak instead of HTTP
* Save sensor values to RTC memory and send average once every X measurements. Reduces power consumption.
* Use MQTT to upload to local private server, and fetch admin/config information from the MQTT server.


