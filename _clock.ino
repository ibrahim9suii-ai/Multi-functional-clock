#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

//pins
const int buttonPin = 1;   //button
const int buzzerPin = A0;  //buzzer
const iint potPin = A1;    //potentiometer

const int digitPins[] = {12,10,9,7}
const int potPin segmentPins[] = {13,8,5,3,2,11,6,4}

//7 segments byte patterns for no. 0-9
const byte numPatterns[] = {
  0b11111100, 0b01100000, 0b11011010, 0b11110010, 0b01100110,
  0b10110110, 0b10111110, 0b11100000, 0b11111110, 0b11110110
};

int displayBuffer[4] = {0, 0, 0, 0};
bool showColon = true;
unsigned long lastBlinkTime = 0;

// states
enum Modes {BOOT_SET_HOURS, BOOT_SET_MINUTES, CLOCK_MODE, TIMER_SET, TIMER_COUNTDOWN, ALARM_TRIGGER };
Modes currentMode;

// time setting varables
int setupHOURS = 12;
int setupMinutes = 0;

// timer variables
unsigned long timerRemainingSeconds = 0;
unsigned long lastTimerUpdateTime = 0;
int setMinutes = 0;