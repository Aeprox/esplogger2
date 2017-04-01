#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "ESP8266HTTPClient.h"
#include "PubSubClient.h"
#include "Adafruit_Sensor.h"
#include "DHT.h"
#include "TSL2561.h"

// include the user configuration file
#include "config.h"

extern "C" {
#include "user_interface.h"

extern struct rst_info resetInfo;
}

struct {
  float ir;
  float full;
  float lux;
  float h;
  float t;
  uint16_t vdd;
  uint8_t count;
} rtc_mem;

// enable reading the Vcc voltage with the ADC
ADC_MODE(ADC_VCC);

// WiFi network and password
WiFiClient espClient;
void connectWifi();

// MQTT server & channel
PubSubClient mqttClient(espClient);
void reconnectMqtt();
void callback(char* topic, byte* payload, unsigned int length);

// DHT22 sensor pin & type declaration
DHT dht(DHTPIN, DHTTYPE);

// TSL2561 sensor pin declaration
TSL2561 tsl(TSL2561_ADDR_FLOAT, SDAPIN, SCLPIN);

unsigned long lastMsg = 0;
uint8_t measurements = 0;
void update();
void doMeasurements();
void resetRTC();

void setup(void) {
  Serial.begin(9600);

  dht.begin();

  if (tsl.begin()) {
   Serial.println("Found sensor");
  } else {
   Serial.println("No sensor?");
  }

  tsl.setGain(TSL2561_GAIN_0X);
  tsl.setTiming(TSL2561_INTEGRATIONTIME_13MS);

  mqttClient.setServer(MQTTSERVER, 1883);
  mqttClient.setCallback(callback);

  Serial.print(ESP.getResetReason());
  if (resetInfo.reason != REASON_DEEP_SLEEP_AWAKE){
    resetRTC();
  }
}

void loop() {
  #ifdef SLEEP
  doMeasurements();
  if(rtc_mem.count >= NUMMEASUREMENTS){
    if ((WiFi.status() != WL_CONNECTED)){
      connectWifi();
    }
    if (!mqttClient.connected()) {
      reconnectMqtt();
    }
    mqttClient.loop();

    update();
  }

  WiFi.disconnect(true);
  delay(50);

  if(rtc_mem.count >= NUMMEASUREMENTS-1){
    ESP.deepSleep(MEASUREMENTINTERVAL*1000000, WAKE_RF_DEFAULT);
  }
  else{
    ESP.deepSleep(MEASUREMENTINTERVAL*1000000, WAKE_RF_DISABLED);
  }
  #else
  if ((WiFi.status() != WL_CONNECTED)){
    connectWifi();
  }
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastMsg > MEASUREMENTINTERVAL*1000) {
    doMeasurements();
    lastMsg = millis();
    if(rtc_mem.count >= NUMMEASUREMENTS){
      update();
    }
  }
  #endif

  delay(50);
}

void doMeasurements(){
  tsl.enable();
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    h=0;
    t=0;
  }

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" *C ");

  //  Read 32 bits with top 16 bits IR, bottom 16 bits full spectrum
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir, full;
  ir = lum >> 16;
  full = lum & 0xFFFF;
  uint32_t lux = tsl.calculateLux(full, ir);

  //disable sensor
  tsl.disable();

  Serial.print("IR: "); Serial.print(ir);   Serial.print("\t\t");
  Serial.print("Full: "); Serial.print(full);   Serial.print("\t");
  Serial.print("Visible: "); Serial.print(full - ir);   Serial.print("\t");
  Serial.print("Lux: "); Serial.println(lux);

  // read Vdd voltage
  uint16_t vdd = ESP.getVcc();

  // get avg of previous measurements from RTC memory
  ESP.rtcUserMemoryRead(0, (uint32_t*) &rtc_mem, sizeof(rtc_mem));

  // re-calculate average
  rtc_mem.t = rtc_mem.t+(t/NUMMEASUREMENTS);
  rtc_mem.h = rtc_mem.h+(h/NUMMEASUREMENTS);
  rtc_mem.ir = rtc_mem.ir+((float)ir/NUMMEASUREMENTS);
  rtc_mem.full = rtc_mem.full+((float)full/NUMMEASUREMENTS);
  rtc_mem.lux = rtc_mem.lux+((float)lux/NUMMEASUREMENTS);
  rtc_mem.vdd = vdd;
  rtc_mem.count = rtc_mem.count + 1;

  Serial.println(rtc_mem.t);
  Serial.println(rtc_mem.h);
  Serial.println(rtc_mem.ir);
  Serial.println(rtc_mem.full);
  Serial.println(rtc_mem.lux);
  Serial.println(rtc_mem.vdd);
  Serial.println(rtc_mem.count);

  // write to RTC mem
  ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtc_mem, sizeof(rtc_mem));
}

void update(){
  // TODO: get free heap memory

  String data = String("field1=" + String(rtc_mem.t, 1) + "&field2=" + String(rtc_mem.h, 1) + "&field3=" + String(rtc_mem.ir, 1)+ "&field4=" + String(rtc_mem.full, 1)+ "&field5=" + String(rtc_mem.lux, 1)+ "&field6=" + String(rtc_mem.vdd, DEC));
  int length = data.length();
  char msgBuffer[length];
  data.toCharArray(msgBuffer,length+1);

  String channel = String("channels/" + String(CHANNELID) +"/publish/"+String(APIKEY));
  length = channel.length();
  char chnlBuffer[length];

  channel.toCharArray(chnlBuffer, length+1);
  mqttClient.publish(chnlBuffer,msgBuffer);

  Serial.print(data);
  delay(1000);

  // set rtc to 0 for all fields and write to memory
  resetRTC();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void resetRTC(){
  rtc_mem.t = 0.0;
  rtc_mem.h = 0.0;
  rtc_mem.ir = 0;
  rtc_mem.full = 0;
  rtc_mem.lux = 0;
  rtc_mem.vdd = 0;
  rtc_mem.count = 0;
  ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtc_mem, sizeof(rtc_mem));
}

void connectWifi() {
  WiFi.disconnect(true);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnectMqtt(){
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
