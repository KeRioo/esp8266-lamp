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
    pixels.setPixelColor(i, pixels.Color(R,G,B));
  }
  pixels.show();
}

void calculateDelta() {
  for(int i = 0; i < NUM_LEDS; i++) {
    for(byte j=0; j < 3; j++){
      float foo = target[i][j] - current[i][j];
      if (foo) { foo = foo/FRAMES_PER_SECONDS; }
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
  if (pre_state != 0) { 
    s = pre_state;
  }
  else { s = state; }

  switch (s) {
  case 0:
    setColor(0,0,0);
    colorFader.detach();
    break;
  
  case 1:
    if(fadeCounter++ < 30) {
      calculateVal();
      setColor();
    } else {
      fadeCounter = 0;
      if (pre_state != 0) pre_state = 0;
      else colorFader.detach();   
    }
    break;
  case 2:
    if (rainbowCounter == 65535) {
      rainbowCounter = 0;
    }
    for(int i=0; i<NUM_LEDS; i++) {     
      uint32_t c = pixels.gamma32(pixels.ColorHSV(rainbowCounter + (i * 65536L / pixels.numPixels())));
      current[i][0] = (uint8_t)(c >> 16),
      current[i][1] = (uint8_t)(c >>  8),
      current[i][2] = (uint8_t)c;

      setColor();
    }
    rainbowCounter += 256;
    break;
  }
}

void handleBrightnessFade() {
  if (brightnessCounter < 30) {
    float foo = brightness + deltaBrightness;
    if (foo > 255)    { foo = 255; }
    else if (foo < 0) { foo = 0;   }
    brightness = foo;
    pixels.setBrightness(brightness);
    setColor();
    brightnessCounter++;
  } else {
    brightnessCounter = 0;
    brightnessFader.detach();
  }
}
