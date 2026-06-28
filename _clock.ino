#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

// Hardware Pins
const int buttonPin = 1;       // Mode switch / lock-in button
const int buzzerPin = A0;      // Alarm and chime audio output
const int potPin = A1;         // Potentiometer configuration input

const int digitPins[] = {12, 10, 9, 7}; 
const int segmentPins[] = {13, 8, 5, 3, 2, 11, 6, 4}; 

// 7-segment byte patterns for numbers 0-9 (Common Cathode)
const byte numPatterns[] = {
  0b11111100, 0b01100000, 0b11011010, 0b11110010, 0b01100110,
  0b10110110, 0b10111110, 0b11100000, 0b11111110, 0b11110110
};

int displayBuffer[4] = {0, 0, 0, 0};
bool showColon = true;
unsigned long lastBlinkTime = 0;

// System States
enum Modes { BOOT_SET_HOURS, BOOT_SET_MINUTES, CLOCK_MODE, TIMER_SET, TIMER_COUNTDOWN, ALARM_TRIGGER };
Modes currentMode; 

// Time Setting Temp Variables
int setupHours = 12;
int setupMinutes = 0;

// Timer Variables
unsigned long timerRemainingSeconds = 0;
unsigned long lastTimerUpdateTime = 0;
int setMinutes = 0;

// Button Debounce & Press States
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // Initialize display pins
  for (int i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], HIGH); 
  }
  for (int i = 0; i < 8; i++) {
    pinMode(segmentPins[i], OUTPUT);
    digitalWrite(segmentPins[i], LOW);
  }

  // --- NASA STARTUP CHIME ---
  // Ascending space frequencies (Hz) played using delay intervals
  playTone(880, 150);  // Note 1: A5
  delay(50);
  playTone(1319, 150); // Note 2: E6
  delay(50);
  playTone(1760, 300); // Note 3: A6 (Systems Online!)

  if (!rtc.begin()) {
    while (1); 
  }
  
  // --- SMART BOOT CHECK ---
  if (rtc.lostPower()) {
    currentMode = BOOT_SET_HOURS;
  } else {
    currentMode = CLOCK_MODE;
  }
}

void loop() {
  checkButton();

  switch (currentMode) {
    case BOOT_SET_HOURS:
      handleBootSetHours();
      break;

    case BOOT_SET_MINUTES:
      handleBootSetMinutes();
      break;

    case CLOCK_MODE:
      handleClockMode();
      break;

    case TIMER_SET:
      handleTimerSetMode();
      break;

    case TIMER_COUNTDOWN:
      handleTimerCountdownMode();
      break;

    case ALARM_TRIGGER:
      handleAlarmMode();
      break;
  }

  refreshDisplay();
}

// --- AUDIO HELPER FUNCTION ---
// Generates a square wave frequency for passive/active buzzers safely
void playTone(int frequency, int durationMs) {
  long periodMicroseconds = 1000000L / frequency;
  long elapsedMicroseconds = 0;
  
  while (elapsedMicroseconds < durationMs * 1000L) {
    digitalWrite(buzzerPin, HIGH);
    delayMicroseconds(periodMicroseconds / 2);
    digitalWrite(buzzerPin, LOW);
    delayMicroseconds(periodMicroseconds / 2);
    elapsedMicroseconds += periodMicroseconds;
  }
}

// --- BOOT SETUP HANDLERS ---

void handleBootSetHours() {
  int potValue = analogRead(potPin);
  setupHours = map(potValue, 0, 1023, 0, 23); 

  displayBuffer[0] = setupHours / 10;
  displayBuffer[1] = setupHours % 10;
  displayBuffer[2] = setupMinutes / 10;
  displayBuffer[3] = setupMinutes % 10;

  if (millis() - lastBlinkTime >= 250) {
    showColon = !showColon;
    lastBlinkTime = millis();
  }
  if (!showColon) {
    displayBuffer[0] = 10; 
    displayBuffer[1] = 10;
  }
}

void handleBootSetMinutes() {
  int potValue = analogRead(potPin);
  setupMinutes = map(potValue, 0, 1023, 0, 59); 

  displayBuffer[0] = setupHours / 10;
  displayBuffer[1] = setupHours % 10;
  displayBuffer[2] = setupMinutes / 10;
  displayBuffer[3] = setupMinutes % 10;

  if (millis() - lastBlinkTime >= 250) {
    showColon = !showColon;
    lastBlinkTime = millis();
  }
  if (!showColon) {
    displayBuffer[2] = 10; 
    displayBuffer[3] = 10;
  }
}

// --- OPERATIONAL RUN HANDLERS ---

void handleClockMode() {
  DateTime now = rtc.now();
  displayBuffer[0] = now.hour() / 10;
  displayBuffer[1] = now.hour() % 10;
  displayBuffer[2] = now.minute() / 10;
  displayBuffer[3] = now.minute() % 10;

  if (millis() - lastBlinkTime >= 500) {
    showColon = !showColon;
    lastBlinkTime = millis();
  }
}

void handleTimerSetMode() {
  showColon = true; 
  int potValue = analogRead(potPin);
  setMinutes = map(potValue, 0, 1023, 0, 60);

  displayBuffer[0] = setMinutes / 10;
  displayBuffer[1] = setMinutes % 10;
  displayBuffer[2] = 0; 
  displayBuffer[3] = 0;
}

void handleTimerCountdownMode() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastTimerUpdateTime >= 1000) {
    lastTimerUpdateTime = currentMillis;
    if (timerRemainingSeconds > 0) {
      timerRemainingSeconds--;
    } else {
      currentMode = ALARM_TRIGGER;
    }
    showColon = !showColon;
  }

  int displayMin = timerRemainingSeconds / 60;
  int displaySec = timerRemainingSeconds % 60;

  displayBuffer[0] = displayMin / 10;
  displayBuffer[1] = displayMin % 10;
  displayBuffer[2] = displaySec / 10;
  displayBuffer[3] = displaySec % 10;
}

void handleAlarmMode() {
  displayBuffer[0] = 0; displayBuffer[1] = 0; displayBuffer[2] = 0; displayBuffer[3] = 0;
  
  // High pitched alarm signal sequence
  if ((millis() / 250) % 2 == 0) {
    playTone(2000, 100); 
    showColon = true;
  } else {
    digitalWrite(buzzerPin, LOW);
    showColon = false;
  }
}

// --- HARDWARE INTERRUPT & PROCESSING INTERFACE ---

void checkButton() {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      digitalWrite(buzzerPin, LOW); 
      
      // Provide a fast click confirmation sound on button press
      playTone(1500, 30);

      if (currentMode == BOOT_SET_HOURS) {
        currentMode = BOOT_SET_MINUTES;
        showColon = true;
      } 
      else if (currentMode == BOOT_SET_MINUTES) {
        rtc.adjust(DateTime(2026, 1, 1, setupHours, setupMinutes, 0));
        currentMode = CLOCK_MODE;
      } 
      else if (currentMode == CLOCK_MODE) {
        currentMode = TIMER_SET;
      } 
      else if (currentMode == TIMER_SET) {
        if (setMinutes > 0) {
          timerRemainingSeconds = setMinutes * 60UL;
          lastTimerUpdateTime = millis();
          currentMode = TIMER_COUNTDOWN;
        } else {
          currentMode = CLOCK_MODE;
        }
      } 
      else if (currentMode == TIMER_COUNTDOWN || currentMode == ALARM_TRIGGER) {
        currentMode = CLOCK_MODE;
      }
    }
  }
  lastButtonState = reading;
}

void refreshDisplay() {
  for (int digit = 0; digit < 4; digit++) {
    for (int seg = 0; seg < 8; seg++) {
      digitalWrite(segmentPins[seg], LOW);
    }

    if (displayBuffer[digit] == 10) {
      continue; 
    }

    digitalWrite(digitPins[digit], LOW);

    byte pattern = numPatterns[displayBuffer[digit]];
    for (int seg = 0; seg < 7; seg++) {
      digitalWrite(segmentPins[seg], (pattern >> (7 - seg)) & 0x01);
    }

    if (digit == 1) {
      if (currentMode == BOOT_SET_HOURS || currentMode == BOOT_SET_MINUTES) {
        digitalWrite(segmentPins[7], HIGH); 
      } else if (showColon) {
        digitalWrite(segmentPins[7], HIGH); 
      }
    }

    delayMicroseconds(2000); 
    digitalWrite(digitPins[digit], HIGH);
  }
}#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

// Hardware Pins
const int buttonPin = 1;       // Mode switch / lock-in button
const int buzzerPin = A0;      // Alarm and chime audio output
const int potPin = A1;         // Potentiometer configuration input

const int digitPins[] = {12, 10, 9, 7}; 
const int segmentPins[] = {13, 8, 5, 3, 2, 11, 6, 4}; 

// 7-segment byte patterns for numbers 0-9 (Common Cathode)
const byte numPatterns[] = {
  0b11111100, 0b01100000, 0b11011010, 0b11110010, 0b01100110,
  0b10110110, 0b10111110, 0b11100000, 0b11111110, 0b11110110
};

int displayBuffer[4] = {0, 0, 0, 0};
bool showColon = true;
unsigned long lastBlinkTime = 0;

// System States
enum Modes { BOOT_SET_HOURS, BOOT_SET_MINUTES, CLOCK_MODE, TIMER_SET, TIMER_COUNTDOWN, ALARM_TRIGGER };
Modes currentMode; 

// Time Setting Temp Variables
int setupHours = 12;
int setupMinutes = 0;

// Timer Variables
unsigned long timerRemainingSeconds = 0;
unsigned long lastTimerUpdateTime = 0;
int setMinutes = 0;

// Button Debounce & Press States
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // Initialize display pins
  for (int i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], HIGH); 
  }
  for (int i = 0; i < 8; i++) {
    pinMode(segmentPins[i], OUTPUT);
    digitalWrite(segmentPins[i], LOW);
  }

  // --- NASA STARTUP CHIME ---
  // Ascending space frequencies (Hz) played using delay intervals
  playTone(880, 150);  // Note 1: A5
  delay(50);
  playTone(1319, 150); // Note 2: E6
  delay(50);
  playTone(1760, 300); // Note 3: A6 (Systems Online!)

  if (!rtc.begin()) {
    while (1); 
  }
  
  // --- SMART BOOT CHECK ---
  if (rtc.lostPower()) {
    currentMode = BOOT_SET_HOURS;
  } else {
    currentMode = CLOCK_MODE;
  }
}

void loop() {
  checkButton();

  switch (currentMode) {
    case BOOT_SET_HOURS:
      handleBootSetHours();
      break;

    case BOOT_SET_MINUTES:
      handleBootSetMinutes();
      break;

    case CLOCK_MODE:
      handleClockMode();
      break;

    case TIMER_SET:
      handleTimerSetMode();
      break;

    case TIMER_COUNTDOWN:
      handleTimerCountdownMode();
      break;

    case ALARM_TRIGGER:
      handleAlarmMode();
      break;
  }

  refreshDisplay();
}

// --- AUDIO HELPER FUNCTION ---
// Generates a square wave frequency for passive/active buzzers safely
void playTone(int frequency, int durationMs) {
  long periodMicroseconds = 1000000L / frequency;
  long elapsedMicroseconds = 0;
  
  while (elapsedMicroseconds < durationMs * 1000L) {
    digitalWrite(buzzerPin, HIGH);
    delayMicroseconds(periodMicroseconds / 2);
    digitalWrite(buzzerPin, LOW);
    delayMicroseconds(periodMicroseconds / 2);
    elapsedMicroseconds += periodMicroseconds;
  }
}

// --- BOOT SETUP HANDLERS ---

void handleBootSetHours() {
  int potValue = analogRead(potPin);
  setupHours = map(potValue, 0, 1023, 0, 23); 

  displayBuffer[0] = setupHours / 10;
  displayBuffer[1] = setupHours % 10;
  displayBuffer[2] = setupMinutes / 10;
  displayBuffer[3] = setupMinutes % 10;

  if (millis() - lastBlinkTime >= 250) {
    showColon = !showColon;
    lastBlinkTime = millis();
  }
  if (!showColon) {
    displayBuffer[0] = 10; 
    displayBuffer[1] = 10;
  }
}

void handleBootSetMinutes() {
  int potValue = analogRead(potPin);
  setupMinutes = map(potValue, 0, 1023, 0, 59); 

  displayBuffer[0] = setupHours / 10;
  displayBuffer[1] = setupHours % 10;
  displayBuffer[2] = setupMinutes / 10;
  displayBuffer[3] = setupMinutes % 10;

  if (millis() - lastBlinkTime >= 250) {
    showColon = !showColon;
    lastBlinkTime = millis();
  }
  if (!showColon) {
    displayBuffer[2] = 10; 
    displayBuffer[3] = 10;
  }
}

// --- OPERATIONAL RUN HANDLERS ---

void handleClockMode() {
  DateTime now = rtc.now();
  displayBuffer[0] = now.hour() / 10;
  displayBuffer[1] = now.hour() % 10;
  displayBuffer[2] = now.minute() / 10;
  displayBuffer[3] = now.minute() % 10;

  if (millis() - lastBlinkTime >= 500) {
    showColon = !showColon;
    lastBlinkTime = millis();
  }
}

void handleTimerSetMode() {
  showColon = true; 
  int potValue = analogRead(potPin);
  setMinutes = map(potValue, 0, 1023, 0, 60);

  displayBuffer[0] = setMinutes / 10;
  displayBuffer[1] = setMinutes % 10;
  displayBuffer[2] = 0; 
  displayBuffer[3] = 0;
}

void handleTimerCountdownMode() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastTimerUpdateTime >= 1000) {
    lastTimerUpdateTime = currentMillis;
    if (timerRemainingSeconds > 0) {
      timerRemainingSeconds--;
    } else {
      currentMode = ALARM_TRIGGER;
    }
    showColon = !showColon;
  }

  int displayMin = timerRemainingSeconds / 60;
  int displaySec = timerRemainingSeconds % 60;

  displayBuffer[0] = displayMin / 10;
  displayBuffer[1] = displayMin % 10;
  displayBuffer[2] = displaySec / 10;
  displayBuffer[3] = displaySec % 10;
}

void handleAlarmMode() {
  displayBuffer[0] = 0; displayBuffer[1] = 0; displayBuffer[2] = 0; displayBuffer[3] = 0;
  
  // High pitched alarm signal sequence
  if ((millis() / 250) % 2 == 0) {
    playTone(2000, 100); 
    showColon = true;
  } else {
    digitalWrite(buzzerPin, LOW);
    showColon = false;
  }
}

// --- HARDWARE INTERRUPT & PROCESSING INTERFACE ---

void checkButton() {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      digitalWrite(buzzerPin, LOW); 
      
      // Provide a fast click confirmation sound on button press
      playTone(1500, 30);

      if (currentMode == BOOT_SET_HOURS) {
        currentMode = BOOT_SET_MINUTES;
        showColon = true;
      } 
      else if (currentMode == BOOT_SET_MINUTES) {
        rtc.adjust(DateTime(2026, 1, 1, setupHours, setupMinutes, 0));
        currentMode = CLOCK_MODE;
      } 
      else if (currentMode == CLOCK_MODE) {
        currentMode = TIMER_SET;
      } 
      else if (currentMode == TIMER_SET) {
        if (setMinutes > 0) {
          timerRemainingSeconds = setMinutes * 60UL;
          lastTimerUpdateTime = millis();
          currentMode = TIMER_COUNTDOWN;
        } else {
          currentMode = CLOCK_MODE;
        }
      } 
      else if (currentMode == TIMER_COUNTDOWN || currentMode == ALARM_TRIGGER) {
        currentMode = CLOCK_MODE;
      }
    }
  }
  lastButtonState = reading;
}

void refreshDisplay() {
  for (int digit = 0; digit < 4; digit++) {
    for (int seg = 0; seg < 8; seg++) {
      digitalWrite(segmentPins[seg], LOW);
    }

    if (displayBuffer[digit] == 10) {
      continue; 
    }

    digitalWrite(digitPins[digit], LOW);

    byte pattern = numPatterns[displayBuffer[digit]];
    for (int seg = 0; seg < 7; seg++) {
      digitalWrite(segmentPins[seg], (pattern >> (7 - seg)) & 0x01);
    }

    if (digit == 1) {
      if (currentMode == BOOT_SET_HOURS || currentMode == BOOT_SET_MINUTES) {
        digitalWrite(segmentPins[7], HIGH); 
      } else if (showColon) {
        digitalWrite(segmentPins[7], HIGH); 
      }
    }

    delayMicroseconds(2000); 
    digitalWrite(digitPins[digit], HIGH);
  }
}#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

// Hardware Pins
const int buttonPin = 1;       // Mode switch / lock-in button
const int buzzerPin = A0;      // Alarm and chime audio output
const int potPin = A1;         // Potentiometer configuration input

const int digitPins[] = {12, 10, 9, 7}; 
const int segmentPins[] = {13, 8, 5, 3, 2, 11, 6, 4}; 

// 7-segment byte patterns for numbers 0-9 (Common Cathode)
const byte numPatterns[] = {
  0b11111100, 0b01100000, 0b11011010, 0b11110010, 0b01100110,
  0b10110110, 0b10111110, 0b11100000, 0b11111110, 0b11110110
};

int displayBuffer[4] = {0, 0, 0, 0};
bool showColon = true;
unsigned long lastBlinkTime = 0;

// System States
enum Modes { BOOT_SET_HOURS, BOOT_SET_MINUTES, CLOCK_MODE, TIMER_SET, TIMER_COUNTDOWN, ALARM_TRIGGER };
Modes currentMode; 

// Time Setting Temp Variables
int setupHours = 12;
int setupMinutes = 0;

// Timer Variables
unsigned long timerRemainingSeconds = 0;
unsigned long lastTimerUpdateTime = 0;
int setMinutes = 0;

// Button Debounce & Press States
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // Initialize display pins
  for (int i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], HIGH); 
  }
  for (int i = 0; i < 8; i++) {
    pinMode(segmentPins[i], OUTPUT);
    digitalWrite(segmentPins[i], LOW);
  }

  // --- NASA STARTUP CHIME ---
  // Ascending space frequencies (Hz) played using delay intervals
  playTone(880, 150);  // Note 1: A5
  delay(50);
  playTone(1319, 150); // Note 2: E6
  delay(50);
  playTone(1760, 300); // Note 3: A6 (Systems Online!)

  if (!rtc.begin()) {
    while (1); 
  }
  
  // --- SMART BOOT CHECK ---
  if (rtc.lostPower()) {
    currentMode = BOOT_SET_HOURS;
  } else {
    currentMode = CLOCK_MODE;
  }
}

void loop() {
  checkButton();

  switch (currentMode) {
    case BOOT_SET_HOURS:
      handleBootSetHours();
      break;

    case BOOT_SET_MINUTES:
      handleBootSetMinutes();
      break;

    case CLOCK_MODE:
      handleClockMode();
      break;

    case TIMER_SET:
      handleTimerSetMode();
      break;

    case TIMER_COUNTDOWN:
      handleTimerCountdownMode();
      break;

    case ALARM_TRIGGER:
      handleAlarmMode();
      break;
  }

  refreshDisplay();
}

// --- AUDIO HELPER FUNCTION ---
// Generates a square wave frequency for passive/active buzzers safely
void playTone(int frequency, int durationMs) {
  long periodMicroseconds = 1000000L / frequency;
  long elapsedMicroseconds = 0;
  
  while (elapsedMicroseconds < durationMs * 1000L) {
    digitalWrite(buzzerPin, HIGH);
    delayMicroseconds(periodMicroseconds / 2);
    digitalWrite(buzzerPin, LOW);
    delayMicroseconds(periodMicroseconds / 2);
    elapsedMicroseconds += periodMicroseconds;
  }
}

// --- BOOT SETUP HANDLERS ---

void handleBootSetHours() {
  int potValue = analogRead(potPin);
  setupHours = map(potValue, 0, 1023, 0, 23); 

  displayBuffer[0] = setupHours / 10;
  displayBuffer[1] = setupHours % 10;
  displayBuffer[2] = setupMinutes / 10;
  displayBuffer[3] = setupMinutes % 10;

  if (millis() - lastBlinkTime >= 250) {
    showColon = !showColon;
    lastBlinkTime = millis();
  }
  if (!showColon) {
    displayBuffer[0] = 10; 
    displayBuffer[1] = 10;
  }
}

void handleBootSetMinutes() {
  int potValue = analogRead(potPin);
  setupMinutes = map(potValue, 0, 1023, 0, 59); 

  displayBuffer[0] = setupHours / 10;
  displayBuffer[1] = setupHours % 10;
  displayBuffer[2] = setupMinutes / 10;
  displayBuffer[3] = setupMinutes % 10;

  if (millis() - lastBlinkTime >= 250) {
    showColon = !showColon;
    lastBlinkTime = millis();
  }
  if (!showColon) {
    displayBuffer[2] = 10; 
    displayBuffer[3] = 10;
  }
}

// --- OPERATIONAL RUN HANDLERS ---

void handleClockMode() {
  DateTime now = rtc.now();
  displayBuffer[0] = now.hour() / 10;
  displayBuffer[1] = now.hour() % 10;
  displayBuffer[2] = now.minute() / 10;
  displayBuffer[3] = now.minute() % 10;

  if (millis() - lastBlinkTime >= 500) {
    showColon = !showColon;
    lastBlinkTime = millis();
  }
}

void handleTimerSetMode() {
  showColon = true; 
  int potValue = analogRead(potPin);
  setMinutes = map(potValue, 0, 1023, 0, 60);

  displayBuffer[0] = setMinutes / 10;
  displayBuffer[1] = setMinutes % 10;
  displayBuffer[2] = 0; 
  displayBuffer[3] = 0;
}

void handleTimerCountdownMode() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastTimerUpdateTime >= 1000) {
    lastTimerUpdateTime = currentMillis;
    if (timerRemainingSeconds > 0) {
      timerRemainingSeconds--;
    } else {
      currentMode = ALARM_TRIGGER;
    }
    showColon = !showColon;
  }

  int displayMin = timerRemainingSeconds / 60;
  int displaySec = timerRemainingSeconds % 60;

  displayBuffer[0] = displayMin / 10;
  displayBuffer[1] = displayMin % 10;
  displayBuffer[2] = displaySec / 10;
  displayBuffer[3] = displaySec % 10;
}

void handleAlarmMode() {
  displayBuffer[0] = 0; displayBuffer[1] = 0; displayBuffer[2] = 0; displayBuffer[3] = 0;
  
  // High pitched alarm signal sequence
  if ((millis() / 250) % 2 == 0) {
    playTone(2000, 100); 
    showColon = true;
  } else {
    digitalWrite(buzzerPin, LOW);
    showColon = false;
  }
}

// --- HARDWARE INTERRUPT & PROCESSING INTERFACE ---

void checkButton() {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      digitalWrite(buzzerPin, LOW); 
      
      // Provide a fast click confirmation sound on button press
      playTone(1500, 30);

      if (currentMode == BOOT_SET_HOURS) {
        currentMode = BOOT_SET_MINUTES;
        showColon = true;
      } 
      else if (currentMode == BOOT_SET_MINUTES) {
        rtc.adjust(DateTime(2026, 1, 1, setupHours, setupMinutes, 0));
        currentMode = CLOCK_MODE;
      } 
      else if (currentMode == CLOCK_MODE) {
        currentMode = TIMER_SET;
      } 
      else if (currentMode == TIMER_SET) {
        if (setMinutes > 0) {
          timerRemainingSeconds = setMinutes * 60UL;
          lastTimerUpdateTime = millis();
          currentMode = TIMER_COUNTDOWN;
        } else {
          currentMode = CLOCK_MODE;
        }
      } 
      else if (currentMode == TIMER_COUNTDOWN || currentMode == ALARM_TRIGGER) {
        currentMode = CLOCK_MODE;
      }
    }
  }
  lastButtonState = reading;
}

void refreshDisplay() {
  for (int digit = 0; digit < 4; digit++) {
    for (int seg = 0; seg < 8; seg++) {
      digitalWrite(segmentPins[seg], LOW);
    }

    if (displayBuffer[digit] == 10) {
      continue; 
    }

    digitalWrite(digitPins[digit], LOW);

    byte pattern = numPatterns[displayBuffer[digit]];
    for (int seg = 0; seg < 7; seg++) {
      digitalWrite(segmentPins[seg], (pattern >> (7 - seg)) & 0x01);
    }

    if (digit == 1) {
      if (currentMode == BOOT_SET_HOURS || currentMode == BOOT_SET_MINUTES) {
        digitalWrite(segmentPins[7], HIGH); 
      } else if (showColon) {
        digitalWrite(segmentPins[7], HIGH); 
      }
    }

    delayMicroseconds(2000); 
    digitalWrite(digitPins[digit], HIGH);
  }
}