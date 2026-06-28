#include <Wire.h>

// --- PIN CONFIGURATION ---
const int buttonPin = 1;        // Mode switch / lock-in button
const int buzzerPin = A0;       // Alarm and chime audio output
const int potPin = A1;          // Potentiometer configuration input

const int digitPins[] = {12, 10, 9, 7};
const int segmentPins[] = {13, 8, 5, 3, 2, 11, 6, 4};

// --- DISPLAY SEGMENT PATTERNS (Common Cathode) ---
const byte numPatterns[] = {
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111  // 9
};

int displayBuffer[4] = {0, 0, 0, 0};
bool showColon = true;
unsigned long lastBlinkTime = 0;

// --- STATE MACHINE ---
enum Modes { BOOT_SET_HOURS, BOOT_SET_MINUTES, CLOCK_MODE, TIMER_SET, TIMER_COUNTDOWN, ALARM_TRIGGER };
Modes currentMode;

// --- TIME & CONFIGURATION VARIABLES ---
int setupHours = 12;
int setupMinutes = 0;

unsigned long timerRemainingSeconds = 0;
unsigned long lastTimerUpdateTime = 0;
int setMinutes = 0;

// --- DEBOUNCE SYSTEM ---
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  Wire.begin();
  
  // Initialize Pins
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  pinMode(potPin, INPUT);
  
  for (int i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], HIGH); // Turn off digits (Common Cathode control)
  }
  
  for (int i = 0; i < 8; i++) {
    pinMode(segmentPins[i], OUTPUT);
    digitalWrite(segmentPins[i], LOW);
  }

  // Boot chime sequence
  currentMode = BOOT_SET_HOURS;
  playTone(440, 150);
  delay(100);
  playTone(554, 150);
  delay(100);
  playTone(659, 300);
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

void playTone(int frequency, int durationMs) {
  long period = 1000000L / frequency;
  long pulse = period / 2;
  long totalCycles = ((long)durationMs * 1000L) / period;
  for (long i = 0; i < totalCycles; i++) {
    digitalWrite(buzzerPin, HIGH);
    delayMicroseconds(pulse);
    digitalWrite(buzzerPin, LOW);
    delayMicroseconds(pulse);
  }
}

void handleBootSetHours() {
  int potVal = analogRead(potPin);
  setupHours = map(potVal, 0, 1023, 0, 23);
  
  displayBuffer[0] = setupHours / 10;
  displayBuffer[1] = setupHours % 10;
  displayBuffer[2] = 10; // Clear digit
  displayBuffer[3] = 10; // Clear digit
  showColon = false;
}

void handleBootSetMinutes() {
  int potVal = analogRead(potPin);
  setupMinutes = map(potVal, 0, 1023, 0, 59);
  
  displayBuffer[0] = 10; // Clear digit
  displayBuffer[1] = 10; // Clear digit
  displayBuffer[2] = setupMinutes / 10;
  displayBuffer[3] = setupMinutes % 10;
  showColon = false;
}

void handleClockMode() {
  // Pull live data from DS3231 RTC via I2C bus
  Wire.beginTransmission(0x68);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(0x68, 3);
  
  byte bcdSeconds = Wire.read();
  byte bcdMinutes = Wire.read();
  byte bcdHours = Wire.read();
  
  int currentHour = ((bcdHours & 0x20) ? 20 : 0) + ((bcdHours & 0x10) ? 10 : 0) + (bcdHours & 0x0F);
  int currentMinute = ((bcdMinutes & 0x70) >> 4) * 10 + (bcdMinutes & 0x0F);
  
  displayBuffer[0] = currentHour / 10;
  displayBuffer[1] = currentHour % 10;
  displayBuffer[2] = currentMinute / 10;
  displayBuffer[3] = currentMinute % 10;
  
  if (millis() - lastBlinkTime >= 500) {
    showColon = !showColon;
    lastBlinkTime = millis();
  }
}

void handleTimerSetMode() {
  int potVal = analogRead(potPin);
  setMinutes = map(potVal, 0, 1023, 0, 60);
  
  displayBuffer[0] = 0;
  displayBuffer[1] = 0;
  displayBuffer[2] = setMinutes / 10;
  displayBuffer[3] = setMinutes % 10;
  showColon = true;
}

void handleTimerCountdownMode() {
  if (millis() - lastTimerUpdateTime >= 1000) {
    lastTimerUpdateTime = millis();
    if (timerRemainingSeconds > 0) {
      timerRemainingSeconds--;
    } else {
      currentMode = ALARM_TRIGGER;
    }
  }
  
  int displayMin = timerRemainingSeconds / 60;
  int displaySec = timerRemainingSeconds % 60;
  
  displayBuffer[0] = displayMin / 10;
  displayBuffer[1] = displayMin % 10;
  displayBuffer[2] = displaySec / 10;
  displayBuffer[3] = displaySec % 10;
  showColon = true;
}

void handleAlarmMode() {
  displayBuffer[0] = 0;
  displayBuffer[1] = 0;
  displayBuffer[2] = 0;
  displayBuffer[3] = 0;
  showColon = true;
  
  if ((millis() / 250) % 2 == 0) {
    playTone(880, 100);
  }
}

void checkButton() {
  bool reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      // Button was pressed, shift states
      if (currentMode == BOOT_SET_HOURS) {
        currentMode = BOOT_SET_MINUTES;
        playTone(554, 100);
      } else if (currentMode == BOOT_SET_MINUTES) {
        // Program the final values directly into the DS3231 RTC registers via I2C
        byte bcdH = ((setupHours / 10) << 4) | (setupHours % 10);
        byte bcdM = ((setupMinutes / 10) << 4) | (setupMinutes % 10);
        Wire.beginTransmission(0x68);
        Wire.write(0); // Start at register 00h (seconds)
        Wire.write(0); // Reset seconds to 00
        Wire.write(bcdM);
        Wire.write(bcdH);
        Wire.endTransmission();
        
        currentMode = CLOCK_MODE;
        playTone(659, 200);
      } else if (currentMode == CLOCK_MODE) {
        currentMode = TIMER_SET;
        playTone(440, 100);
      } else if (currentMode == TIMER_SET) {
        if (setMinutes > 0) {
          timerRemainingSeconds = (unsigned long)setMinutes * 60;
          lastTimerUpdateTime = millis();
          currentMode = TIMER_COUNTDOWN;
          playTone(880, 200);
        } else {
          currentMode = CLOCK_MODE;
          playTone(330, 150);
        }
      } else if (currentMode == TIMER_COUNTDOWN) {
        currentMode = CLOCK_MODE; // Cancel timer
        playTone(330, 150);
      } else if (currentMode == ALARM_TRIGGER) {
        currentMode = CLOCK_MODE; // Dismiss alarm
        playTone(440, 150);
      }
    }
  }
  lastButtonState = reading;
}

void refreshDisplay() {
  for (int d = 0; d < 4; d++) {
    // Clear all segment outputs
    for (int s = 0; s < 8; s++) {
      digitalWrite(segmentPins[s], LOW);
    }
    
    // Select the active common cathode digit
    digitalWrite(digitPins[d], LOW);
    
    int symbol = displayBuffer[d];
    if (symbol >= 0 && symbol <= 9) {
      byte pattern = numPatterns[symbol];
      for (int s = 0; s < 7; s++) {
        if (bitRead(pattern, s)) {
          digitalWrite(segmentPins[s], HIGH);
        }
      }
    }
    
    // Control the center colon marker / decimal point on Digit 2
    if (d == 1 && showColon) {
      digitalWrite(segmentPins[7], HIGH);
    }
    
    delayMicroseconds(2000); // High-frequency multiplex delay interval
    digitalWrite(digitPins[d], HIGH); // De-select digit
  }
}