#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <Adafruit_NeoPixel.h>


// WIFI credentials
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "KEY"

// MQTT broker address
#define MQTT_HOST "broker-address.here" // URL or IPAddress(10, 0, 0, 100)
#define MQTT_PORT 1883

// #define MQTT_AUTH // Uncoment if you want use login/pass
#define MQTT_USER "test"
#define MQTT_PASS "test"

// Your amount of LEDs to be wrote, and their configuration for NeoPixel
#define NUM_LEDS    16
#define DATA_PIN    2

const String client_id = String(ESP.getChipId(), HEX);

// Topics

const String light_set_topic = "/home/desk-lamp/" + client_id + "/setRGB";
const String light_get_topic = "/home/desk-lamp/" + client_id + "/getRGB";
const String light_set_topic_all = "/home/desk-lamp/all/setRGB";

const String brightness_set_topic = "/home/desk-lamp/" + client_id + "/setBrightness";
const String brightness_get_topic = "/home/desk-lamp/" + client_id + "/getBrightness";
const String brightness_set_topic_all = "/home/desk-lamp/all/setBrightness";


// Code

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

Ticker fader;

Adafruit_NeoPixel pixels(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// Init globals
float current[3] = {0,0,0};
float delta[3] = {0,0,0};
float step[3] = {0,0,0};
int target[3] = {0,0,0};
byte brightness = 50;
int fadeCounter = 0;

char buff[50];
  

void setColor(byte red, byte green, byte blue) {
  for(int i=0; i<NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(red, green, blue));
  }
  pixels.show();
}

float calculateDelta(int prevValue, int endValue) {
    float delta = endValue - prevValue;
    if (delta) {                      
        delta = delta/30;            
    }
    Serial.println(delta);
    return delta;
}

float calculateVal(float delta, float val) {
    float foo;
    foo = val + (float) delta;
    
    if (foo > 255) {
        foo = 255;
    }
    else if (foo < 0) {
        foo = 0;
    }
    Serial.print(foo);
    return foo;
}

void handleFade() {
  if(fadeCounter < 30 + 1) {
    fadeCounter++;
    current[0] = calculateVal(delta[0], current[0]);
    current[1] = calculateVal(delta[1], current[1]);
    current[2] = calculateVal(delta[2], current[2]);

    setColor(int(current[0]),int(current[1]),int(current[2]));
  } else {
    fader.detach();
    fadeCounter = 1;
  }
}

void startFade(byte red, byte green, byte blue) {
  fader.detach();
  target[0] = red;
  target[1] = green;
  target[2] = blue;

  delta[0] = calculateDelta(current[0], target[0]);
  delta[1] = calculateDelta(current[1], target[1]);
  delta[2] = calculateDelta(current[2], target[2]);
  
  fadeCounter = 1;
  fader.attach(0.02, handleFade);
}


// AsyncMqttClient 
void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  setColor(0,0,0);
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach();
  wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT as " + client_id);
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  
  mqttClient.subscribe(light_set_topic.c_str(), 2);
  mqttClient.subscribe(light_set_topic_all.c_str(), 2);
  mqttClient.subscribe(brightness_set_topic.c_str(), 2);
  mqttClient.subscribe(brightness_set_topic_all.c_str(), 2);

  sprintf(buff,"#%02X%02X%02X", (int)current[0], (int)current[1], (int)current[2]);
  mqttClient.publish(light_get_topic.c_str(), 2, true, buff);
  memset(buff, '\0', sizeof(buff));

  mqttClient.publish(brightness_get_topic.c_str(), 2, true, String(brightness).c_str());
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);


  char message[len + 1];
  for (int i = 0; i < (int)len; i++) {
    message[i] = (char)payload[i];
  }
  message[len] = '\0';
  Serial.println(message);
  String msg = String(message);
  
  if (strcmp(topic, light_set_topic.c_str())==0 || strcmp(topic, light_set_topic_all.c_str())==0) {
    mqttClient.publish(light_get_topic.c_str(), 2, true, msg.c_str());
    msg.replace('#', '\0');
  
    long number = (long) strtol( &msg[1], NULL, 16);
    byte red = number >> 16;
    byte green = number >> 8 & 0xFF;
    byte blue = number & 0xFF;
    
    startFade(red, green, blue);
  }

  if (strcmp(topic, brightness_set_topic.c_str())==0 || strcmp(topic, brightness_set_topic_all.c_str())==0) {
    msg = (byte)msg.toInt();
    if ((byte)msg.toInt() < 256 && (byte)msg.toInt() > -1) {
      brightness = (byte)msg.toInt();

      pixels.setBrightness(brightness);
      setColor(current[0], current[1], current[2]);
        
      mqttClient.publish(brightness_get_topic.c_str(), 2, true, msg.c_str());
    }
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  pixels.begin();
  pixels.setBrightness(brightness);
  pixels.clear();
  setColor(0,0,0);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
  mqttClient.setClientId(client_id.c_str());
  #ifdef MQTT_AUTH
    mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
  #endif
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();
}

void loop() {}