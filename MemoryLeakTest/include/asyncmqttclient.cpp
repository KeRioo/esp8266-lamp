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
  connectToMqtt();
  setColor();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
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

    colorFader.detach();
    calculateDelta();
    state = 1;
    fadeCounter = 0;
    colorFader.attach_ms(FRAMES_PER_SECONDS, handleFade);
  }

  if (strcmp(topic, rainbow_topic.c_str())==0 || strcmp(topic, rainbow_topic_all.c_str())==0) {
    colorFader.detach();
    if (state != 2) {
      for(int i=0; i<NUM_LEDS; i++) { 
      uint32_t c = pixels.gamma32(pixels.ColorHSV(rainbowCounter + (i * 65536L / pixels.numPixels())));
      
      target[i][0] = (uint8_t)(c >> 16),
      target[i][1] = (uint8_t)(c >>  8),
      target[i][2] = (uint8_t)c;
      }

      calculateDelta();
      fadeCounter = 0;
      pre_state = 1;
      state = 2;
    }
    colorFader.attach_ms(FRAMES_PER_SECONDS, handleFade);
  }

  if (strcmp(topic, brightness_set_topic.c_str())==0 || strcmp(topic, brightness_set_topic_all.c_str())==0) {
    msg = (byte)msg.toInt();
    if ((byte)msg.toInt() < 256 && (byte)msg.toInt() > -1) {
      mqttClient.publish(brightness_get_topic.c_str(), 2, true, msg.c_str());
      
      targetBrightness = (byte)msg.toInt();
      
      float foo = targetBrightness - brightness;
      if (foo) { foo = foo/FRAMES_PER_SECONDS; }
      deltaBrightness = foo;

      brightnessCounter = 0;
      brightnessFader.attach_ms(FRAMES_PER_SECONDS, handleBrightnessFade);
    }
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}
