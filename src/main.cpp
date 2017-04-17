#include "Arduino.h"
#include "string.h"
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
  uint32_t mInt;
  uint32_t mNum;
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
WiFiClient espAdminClient;
void connectWifi();

// MQTT server & channel
PubSubClient mqttClient(espClient);
PubSubClient adminMqttClient(espAdminClient);
void reconnectMqtt(PubSubClient &client);
void callback(char* topic, byte* payload, unsigned int length);

// DHT22 sensor pin & type declaration
DHT dht(DHTPIN, DHTTYPE);

// TSL2561 sensor pin declaration
TSL2561 tsl(TSL2561_ADDR_FLOAT, SDAPIN, SCLPIN);

unsigned long lastMsg = 0;
uint8_t measurements = 0;
uint32_t measurementInterval = DEFAULTMEASUREMENTINTERVAL;
uint8_t numMeasurements = DEFAULTNUMMEASUREMENTS;

void update();
void doMeasurements();
void resetRTC();

unsigned long timet = 0;

void setup(void) {
  timet = millis();
  Serial.begin(9600);

  dht.begin();
  tsl.begin();

  tsl.setGain(TSL2561_GAIN_0X);
  tsl.setTiming(TSL2561_INTEGRATIONTIME_13MS);

  mqttClient.setServer(MQTTSERVER, 1883);
  mqttClient.setCallback(callback);
  adminMqttClient.setServer(MQTTADMINSERVER, MQTTADMINSERVERPORT);
  adminMqttClient.setCallback(callback);

  // when NOT waking from deep sleep, reset RTC memory to 0 and wait for sensors to initialise
  Serial.println(ESP.getResetReason());
  if (resetInfo.reason != REASON_DEEP_SLEEP_AWAKE){
    resetRTC();

    if ((WiFi.status() != WL_CONNECTED)){
      connectWifi();
    }
    if (!adminMqttClient.connected()) {
      reconnectMqtt(adminMqttClient);
    }

    //need to run pubsubclient.loop a couple of times to handle subscribes
    int i = 0;
    while(i<=25){
      adminMqttClient.loop();
      delay(100);
      i++;
    }

    WiFi.disconnect(true);
  }
  else{ //when waking from deep sleep, fetch config from RTC memory
    ESP.rtcUserMemoryRead(0, (uint32_t*) &rtc_mem, sizeof(rtc_mem));

    measurementInterval = rtc_mem.mInt;
    numMeasurements = rtc_mem.mNum;
  }
}

void loop() {
  #ifdef SLEEP
  doMeasurements();
  if(rtc_mem.count >= numMeasurements){
    if ((WiFi.status() != WL_CONNECTED)){
      connectWifi();
    }
    if (!mqttClient.connected()) {
      reconnectMqtt(mqttClient);
    }
    if (!adminMqttClient.connected()) {
      reconnectMqtt(adminMqttClient);
    }
    update();

    int i = 0;
    while(i<=10){
      adminMqttClient.loop();
      mqttClient.loop();
      delay(200);
      i++;
    }
    WiFi.disconnect(true);
  }

  Serial.print("Done. Took a total of ");
  Serial.print(millis()-timet);
  Serial.println(" milliseconds");

  if(rtc_mem.count >= numMeasurements-1){
    ESP.deepSleep(measurementInterval*1000000, WAKE_RFCAL);
  }
  else{
    ESP.deepSleep(measurementInterval*1000000, WAKE_RF_DISABLED);
  }

  #else

  if ((WiFi.status() != WL_CONNECTED)){
    connectWifi();
  }
  if (!mqttClient.connected()) {
    reconnectMqtt(mqttClient);
  }
  if (!adminMqttClient.connected()) {
    reconnectMqtt(adminMqttClient);
  }
  mqttClient.loop();
  adminMqttClient.loop();

  unsigned long now = millis();
  if (now - lastMsg > measurementInterval*1000) {
    doMeasurements();
    lastMsg = millis();
    if(rtc_mem.count >= numMeasurements){
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
  rtc_mem.t = rtc_mem.t+(t/numMeasurements);
  rtc_mem.h = rtc_mem.h+(h/numMeasurements);
  rtc_mem.ir = rtc_mem.ir+((float)ir/numMeasurements);
  rtc_mem.full = rtc_mem.full+((float)full/numMeasurements);
  rtc_mem.lux = rtc_mem.lux+((float)lux/numMeasurements);
  rtc_mem.vdd = vdd;
  rtc_mem.count = rtc_mem.count + 1;

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

  if(adminMqttClient.publish("templogger/output", msgBuffer)){
    Serial.println("Publish succeeded");
  }
  else{
    Serial.println("Publish failed");
  }

  Serial.print(data);

  // give network some time to send data before proceding
  delay(1000);

  // set rtc to 0 for all fields and write to memory
  resetRTC();
}

void callback(char* topic, byte* payload, unsigned int length) {
  std::string topicS = "";
  String payloadS = "";

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadS += (char) payload[i];
  }
  Serial.println();
  topicS = topic;

  if(topicS.compare("templogger/admin/num") == 0){
    int newNum = payloadS.toInt();
    Serial.printf("Setting new number of measurements to %d ", newNum);
    Serial.println();
    numMeasurements = newNum;
    rtc_mem.mNum = newNum;
  }
  else if (topicS.compare("templogger/admin/int") == 0){
    int newInt = payloadS.toInt();
    Serial.printf("Setting new measurement interval to %d ", newInt);
    Serial.println();
    measurementInterval = newInt;
    rtc_mem.mInt = newInt;
  }

  ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtc_mem, sizeof(rtc_mem));

}

void resetRTC(){
  rtc_mem.mInt = DEFAULTMEASUREMENTINTERVAL;
  rtc_mem.mNum = DEFAULTNUMMEASUREMENTS;
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

void reconnectMqtt(PubSubClient &client){
  uint8_t attempts = 0;

  while (!client.connected()) {
    if(attempts > 10){
      Serial.print("Could not connect to MQTT server. Restarting node.");
      delay(50);
      ESP.reset();
      break;
    }
    Serial.printf("(%d) Attempting MQTT connection...", attempts);
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if ((WiFi.status() != WL_CONNECTED)){
      connectWifi();
    }
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
    attempts++;
  }
  if(&client == &adminMqttClient){
    if(adminMqttClient.subscribe(MQTTADMINTOPIC)){
      Serial.println("Subscribe succeeded");
    }
    else{
      Serial.println("Subscribe failed");
    }
  }
}
