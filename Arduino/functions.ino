//call repeatedly in loop, only updates after a certain time interval
//returns true if update happened
bool getTemperature() {
  if ((millis() - lastTempUpdate) > TEMP_READ_DELAY) {
    temperature = thermocouple.readCelsius();
    lastTempUpdate = millis();
    return true;
  }
  return false;
}

void updateTimer() {
  if ((millis() - lastTimerUpdate) > 1000) {
    if (timerMode > 0) {
      timerMode = timerMode - 1;
      lastTimerUpdate = millis();
      Serial.println(timerMode);
    }
  }
}

// Status LEDs
void updateLEDs() {
  if (temperature > 40 && temperature < 80) {
    ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, 0);  // stop flashing SAFE LED
    ledcWrite(PIN_LED_WARN, 255);                        // set WARN LED to 100% duty cycle
  } else if (temperature > 80) {
    ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, 0);  // stop flashing WARN LED
    ledcWrite(PIN_LED_WARN, 128);                        // set SAFE LED to 100% duty cycle
  } else {
    ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);  // stop flashing WARN LED
    ledcWrite(PIN_LED_SAFE, 255);                        // set SAFE LED to 100% duty cycle
  }
}

void disableSSR() {
  setTemp = 0;
  ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_7, 0);
}

void updateSSR(double PWM) {
  ledcWrite(PIN_SSR, PWM);
}

void processModes() {
  // Idle
  if (mode == 0) {  // Idle mode
    disableSSR();
    timerMode = 0;
    modeDescription = "Idle";
  }

  // Preheat to soak
  if (mode == 1) {
    modeDescription = "Preheat";
    setTemp = temp_preheat;

    myPID.run();

    updateSSR(outputVal);

    if (myPID.atSetPoint(2)) {
      timerMode = 90;  // start countdown timer 90s
      mode = 2;        // Soak mode
    }
  }

  // Soak
  if (mode == 2) {
    modeDescription = "Soak";

    myPID.run();

    updateSSR(outputVal);

    if (myPID.atSetPoint(2)) {
      boolAtSetPoint = true;  // only start the timer if we reached the setpoint
    }

    if (boolAtSetPoint) {
      updateTimer();
    }

    // Soak countdown timer is complete, move to reflow
    if (timerMode == 0) {
      timerMode = 15;  // start
      mode = 3;        // Reflow mode
      boolAtSetPoint = false;
    }
  }

  // Reflow
  if (mode == 3) {
    modeDescription = "Reflow";
    setTemp = temp_reflow;

    myPID.run();

    updateSSR(outputVal);

    if (myPID.atSetPoint(2)) {
      boolAtSetPoint = true;  // only start the timer if we reached the setpoint
    }

    if (boolAtSetPoint) {
      updateTimer();
    }

    if (timerMode == 0) {  // reflow is finished, revert to idle mode
      mode = 0;
      screen = 0;
      boolAtSetPoint = false;
    }
  }
}

void processEncoder() {
  // Poll rotary encoder and pushbutton /////////////////////////////////////////
  unsigned char result = rotary.process();
  pushButton.poll();

  // process rotary encoder inputs only when in the temperature select screen ///
  if (screen == 1 && result == DIR_CW && (temp_reflow < 220)) {
    temp_reflow += 5;
    soundClick();
  }

  if (screen == 1 && result == DIR_CCW && (temp_reflow > 160)) {
    temp_reflow -= 5;
    soundClick();
  }

  // Process long press as appropriate
  if (pushButton.longPress()) {
    if (screen == 0) {
      screen = 1;
      soundConfirm();
    } else if (screen == 1) {
      screen = 2;
      mode = 1;
      soundConfirm();
    } else if (screen == 2) {
      screen = 0;
      mode = 0;
      soundCancel();
    }
  }
}

void updateUI() {
  if ((millis() - lastScreenUpdate) > SCREEN_DRAW_DELAY) {

    if (screen == 0) {
      ui_idle(temperature);
    }

    if (screen == 1) {
      ui_temp();
    }

    if (screen == 2) {
      ui_heating(modeDescription, setTemp, temperature, timerMode);
    }

    lastScreenUpdate = millis();
  }
}

void soundBoot() {
  // Play startup sound
  ledcWriteTone(PIN_BUZZER, 1000);
  delay(250);
  ledcWriteTone(PIN_BUZZER, 1800);
  delay(180);
  ledcWriteTone(PIN_BUZZER, 0);
}

void soundCancel() {
  // Play cancel sound
  ledcWriteTone(PIN_BUZZER, 1800);
  delay(300);
  ledcWriteTone(PIN_BUZZER, 0);
  delay(50);
  ledcWriteTone(PIN_BUZZER, 1400);
  delay(300);
  ledcWriteTone(PIN_BUZZER, 0);
  delay(50);
  ledcWriteTone(PIN_BUZZER, 1000);
  delay(100);
  ledcWriteTone(PIN_BUZZER, 0);
}

void soundConfirm() {
  ledcWriteTone(PIN_BUZZER, 1800);
  delay(75);
  ledcWriteTone(PIN_BUZZER, 0);
  delay(50);
  ledcWriteTone(PIN_BUZZER, 1800);
  delay(75);
  ledcWriteTone(PIN_BUZZER, 0);
}

void soundClick() {
  ledcWriteTone(PIN_BUZZER, 1800);
  delay(3);
  ledcWriteTone(PIN_BUZZER, 0);
}

void ui_boot() {
  // Render boot screen and play "on" tone from piezo
  u8g2.begin();
  u8g2.enableUTF8Print();

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  u8g2.drawXBMP(12, 0, 37, 64, icon_logo);

  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(54, 26, "Wattle Labs");
  u8g2.drawStr(57, 41, "YAORP32");

  u8g2.sendBuffer();
  soundBoot();

  delay(2000);
}

void ui_idle(int temp) {
  // Render idle screen showing current plate temperature
  // screen = 1

  char buffer[4] = "";

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(55, 10, "Idle");

  u8g2.drawXBMP(34, 24, 16, 16, icon_temperature);

  itoa(temp, buffer, 10);  // convert int to string

  u8g2.setFont(u8g2_font_profont22_tr);
  u8g2.drawStr(60, 39, buffer);

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(14, 64, "Hold Select to Start");

  u8g2.sendBuffer();
}

void ui_temp() {
  // Render temperature selection screen
  // screen = 2
  char buffer[4] = "";

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(13, 8, "Select Reflow Temp:");

  u8g2.drawXBMP(34, 24, 16, 16, icon_temperature);

  itoa(temp_reflow, buffer, 10);  // convert int to string

  u8g2.setFont(u8g2_font_profont22_tr);
  u8g2.drawStr(60, 39, buffer);

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(14, 64, "Hold Select to Start");

  u8g2.sendBuffer();
}

void ui_heating(char *phase, int temp_target, int temp_current, int timer) {
  // Render heating screen showing current phase
  // screen = 3
  char buffer[4] = "";

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(12, 64, "Hold Select to Cancel");

  u8g2.drawXBMP(34, 24, 16, 16, icon_temperature);

  itoa(temp_target, buffer, 10);  // convert int to string
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(100, 8, "T:");
  u8g2.drawStr(110, 8, buffer);

  if (timer > 0) { // only display a timer if one is active
    itoa(timer, buffer, 10);  // convert int to string
    u8g2.drawStr(58, 8, buffer);
  }

  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(0, 8, phase);

  itoa(temp_current, buffer, 10);  // convert int to string
  u8g2.setFont(u8g2_font_profont22_tr);
  u8g2.drawStr(60, 39, buffer);

  u8g2.sendBuffer();
}