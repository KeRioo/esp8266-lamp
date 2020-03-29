#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <Adafruit_NeoPixel.h>

ADC_MODE(ADC_VCC);

// WIFI credentials
#define WIFI_SSID 
#define WIFI_PASSWORD 

// MQTT broker address
#define MQTT_HOST  // URL or IPAddress(10, 0, 0, 100)
#define MQTT_PORT 

#define MQTT_AUTH // Uncoment if you want use login/pass
#define MQTT_USER 
#define MQTT_PASS 

// Your amount of LEDs to be wrote, and their configuration for NeoPixel
#define NUM_LEDS    16
#define DATA_PIN    2

#define FRAMES_PER_SECONDS 30

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

Ticker colorFader;
Ticker brightnessFader;

Ticker freeMemory;

Adafruit_NeoPixel pixels(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// Init globals
float current [NUM_LEDS][3];
float delta [NUM_LEDS][3];
byte target [NUM_LEDS][3];

float brightness = 50;
float deltaBrightness = 0;
byte targetBrightness = 0;

byte fadeCounter = 0;
long rainbowCounter = 0;
byte brightnessCounter = 0;

byte pre_state = 0; 
byte state = 0; // 0 => off,  1 => fade,  2 => rainbow

char buff[50];

#include "fade.cpp"

#include "asyncmqttclient.cpp"

extern "C" {
#include "user_interface.h"
}

void freeMem() { 
  Serial.print("Mem: " + String(system_get_free_heap_size()) + " \t"); 
  Serial.print("PWR: " + String(ESP.getVcc()) + " \t");
  Serial.print("R: " + String(current[0][0]) + " \t");
  Serial.print("G: " + String(current[0][1]) + " \t");
  Serial.println("B: " + String(current[0][2]));



  mqttClient.publish("/home/desk-lamp/dev/logs", 2, true, String("Mem: " + String(system_get_free_heap_size()) + " \t" 
  + "PWR: " + String(ESP.getVcc()) + " \t" + "R: " + String(current[0][0]) + " \n").c_str());
};


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
  Serial.println();
  freeMemory.attach(1, freeMem);

  pixels.begin();
  pixels.setBrightness(brightness);
  pixels.clear();
  setColor();

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
  mqttClient.setClientId(client_id.c_str());
  mqttClient.onConnect(onMqttConnect);
  #ifdef MQTT_AUTH
    mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
  #endif
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();
}

void loop() {}