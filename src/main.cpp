#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <Adafruit_NeoPixel.h>


// WIFI credentials
#define WIFI_SSID "test"
#define WIFI_PASSWORD "test"

// MQTT broker address
#define MQTT_HOST IPAddress(192, 168, 1, 201) // URL or IPAddress(10, 0, 0, 100)
#define MQTT_PORT 1883

// #define MQTT_AUTH // Uncoment if you want use login/pass
#define MQTT_USER "test"
#define MQTT_PASS "test"

// Your amount of LEDs to be wrote, and their configuration for NeoPixel
#define NUM_LEDS    16
#define DATA_PIN    2

const String client_id = String(ESP.getChipId(), HEX);

// Topics
const String prefix = "/home/desk-lamp/";
const String light_set_topic = prefix + client_id + "/setRGB";
const String light_get_topic = prefix + client_id + "/getRGB";
const String light_set_topic_all = prefix + "all/setRGB";

const String rainbow_topic = prefix + client_id + "/rainbow";
const String rainbow_topic_all = prefix + "all/rainbow";

const String brightness_set_topic = prefix + client_id + "/setBrightness";
const String brightness_get_topic = prefix + client_id + "/getBrightness";
const String brightness_set_topic_all = prefix + "all/setBrightness";


// Code

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

Ticker fader;

Adafruit_NeoPixel pixels(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// Init globals
float current [NUM_LEDS][3];
float delta [NUM_LEDS][3];
int target [NUM_LEDS][3];
byte brightness = 50;
int fadeCounter = 0;
long rainbowCounter = 0;
byte pre_state = 0; 
byte state = 0; // 0 => off,  1 => fade,  2 => rainbow

char buff[50];

void setColor() {
  for(int i=0; i<NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(
      int(current[i][0]), int(current[i][1]), int(current[i][2])
    ));
  }
  pixels.show();
}

void setColor(byte R, byte G, byte B) {
  for(int i=0; i<NUM_LEDS; i++) {
    pixels.setPixelColor(i, pixels.Color(R, G, B));
  }
  pixels.show();
}

void calculateDelta() {
  for(int i = 0; i < NUM_LEDS; i++) {
    for(byte j=0; j < 3; j++){
      float foo = target[i][j] - current[i][j];
      if (foo) { foo = foo/30; }
      delta[i][j] = foo;
    }
  }
}

void calculateVal() {
  for(int i = 0; i < NUM_LEDS; i++) {
    for(byte j=0; j < 3; j++) {
      float foo = current[i][j] + delta[i][j];
      if (foo > 255)    { foo = 255; }
      else if (foo < 0) { foo = 0;   }
      current[i][j] = foo;
    }
  }
}

void handleFade() {
  byte s;
  if (pre_state) { 
    if (fadeCounter == 30) {
      pre_state = 0;
      s = state;
    }
    s = pre_state;
  }
  else { s = state; }

  switch (s) {
  case 0:
    setColor(0,0,0);
    fader.detach();
    break;
  
  case 1:
    if(fadeCounter < 30) {
      fadeCounter++;
      calculateVal();
      setColor();
    } else {
      fader.detach();
      fadeCounter = 0;
    }
    break;
  case 2:
    if (rainbowCounter == 65535) {
      rainbowCounter = 0;
    }
    for(int i=0; i<pixels.numPixels(); i++) { 
      int pixelHue = rainbowCounter + (i * 65536L / pixels.numPixels());
      uint32_t c = pixels.gamma32(pixels.ColorHSV(pixelHue));

      current[i][0] = (uint8_t)(c >> 16),
      current[i][1] = (uint8_t)(c >>  8),
      current[i][2] = (uint8_t)c;

      setColor();
    }
    rainbowCounter+= 256;
    break;
  }
  
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
  mqttClient.subscribe(brightness_set_topic.c_str(), 2);
  mqttClient.subscribe(rainbow_topic.c_str(), 2);

  mqttClient.subscribe(light_set_topic_all.c_str(), 2);
  mqttClient.subscribe(brightness_set_topic_all.c_str(), 2);
  mqttClient.subscribe(rainbow_topic_all.c_str(), 2);

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
    
    for(int i = 0; i < NUM_LEDS; i++) {
      target[i][0] = red;
      target[i][1] = green;
      target[i][2] = blue;
    }
    fader.detach();

    calculateDelta();
    state = 1;
    fadeCounter = 0;
    fader.attach(0.02, handleFade);
  }

  if (strcmp(topic, rainbow_topic.c_str())==0 || strcmp(topic, rainbow_topic_all.c_str())==0) {
    fader.detach();

    for(int i=0; i<pixels.numPixels(); i++) { 
      uint32_t c = pixels.gamma32(pixels.ColorHSV(i * 65536L / pixels.numPixels()));

      target[i][0] = (uint8_t)(c >> 16),
      target[i][1] = (uint8_t)(c >>  8),
      target[i][2] = (uint8_t)c;
    }
    
    pre_state = 1;
    state = 2;
    fader.attach(0.02, handleFade);
  }


  if (strcmp(topic, brightness_set_topic.c_str())==0 || strcmp(topic, brightness_set_topic_all.c_str())==0) {
    msg = (byte)msg.toInt();
    if ((byte)msg.toInt() < 256 && (byte)msg.toInt() > -1) {
      brightness = (byte)msg.toInt();

      pixels.setBrightness(brightness);
      setColor();
        
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
 
  for(int i = 0; i < NUM_LEDS; i++) {
    current[i][0] = 0;
    current[i][1] = 0;
    current[i][2] = 0;
    delta[i][0] = 0;
    delta[i][1] = 0;
    delta[i][2] = 0;
    target[i][0] = 0;
    target[i][1] = 0;
    target[i][2] = 0;
  }

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