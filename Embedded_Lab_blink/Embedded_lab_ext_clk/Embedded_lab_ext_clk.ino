#include "Arduino.h"
#include "Wire.h" // lib to work with I2C
#include "uRTCLib.h" // lib to work with DS1307

const int segmentPins[] = {2, 3, 4, 5, 6, 7, 8}; 
const int BUTTON_PIN = 9;
const int LED_PIN = 10;

// creation object for RTC with address 0x68 (slave)
uRTCLib rtc(0x68); 

// pattern for 7-segment display 5161AS with common cathode
const byte digitPatterns[11] = {
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111, // 9
  0b00000000  // Пусто
};

int currentCounter = 0; // seconds
const int targetValue = 5; 
bool isPlaying = false;
int previousRTCSecond = -1;
unsigned long lastTickTime = 0; // milliseconds

void setup() {
  Serial.begin(9600);
  
  // display pin setup
  for (int i = 0; i < 7; i++) {
    pinMode(segmentPins[i], OUTPUT);
    digitalWrite(segmentPins[i], LOW);
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  // from now on arduino - master
  URTCLIB_WIRE.begin();
  
  // setting up time, doing it once, then we comment the line
  // rtc.set(second, minute, hour, dayOfWeek, dayOfMonth, month, year);
  // rtc.set(0, 0, 12, 1, 1, 1, 04); 

  displayNumber(0);
  Serial.println("System is Ready. Press button to START.");
}

void loop() {
  // fetching data from RTC using I2C
  rtc.refresh();
  int currentSecond = rtc.second();

  if (!isPlaying) {
    // listening
    // when button is pressed - latch to ground, return LOW
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(200); // for debouncing 
      isPlaying = true;
      currentCounter = 0;
      previousRTCSecond = currentSecond;
      displayNumber(currentCounter);
      Serial.println("GAME STARTED! Catch the 5.");
    }
  } 
  else {
    // game state - incrementing using RTC signal
    if (currentSecond != previousRTCSecond) {
      previousRTCSecond = currentSecond;
      currentCounter++;
      if (currentCounter > 9) currentCounter = 0;
      
      displayNumber(currentCounter);
      lastTickTime = millis(); // Capture the start of a second
      Serial.print("Digit: "); Serial.println(currentCounter);
    }

    // Checking button press
    if (digitalRead(BUTTON_PIN) == LOW) {
      handleGameEnd();
      delay(500); 
    }
  }
}

// Output to 7-segm Display => Parallel Communication
void displayNumber(int num) {
  byte pattern = digitPatterns[num];
  // bitwise right shift
  for (int i = 0; i < 7; i++) {
    bool isOn = (pattern >> i) & 1;
    digitalWrite(segmentPins[i], isOn ? HIGH : LOW);
  }
}

void handleGameEnd() {
  unsigned long pressTime = millis();
  long reactionTime = pressTime - lastTickTime;
  
  Serial.print("Pressed on: "); Serial.println(currentCounter);
  Serial.print("Reaction Time: "); Serial.print(reactionTime); Serial.println(" ms");

  if (currentCounter == targetValue && reactionTime <= 500) {
    
    Serial.println(">>> SUCCESS! <<<");
    // led blinking when success
    for(int i=0; i<10; i++) {
      digitalWrite(LED_PIN, HIGH); delay(50);
      digitalWrite(LED_PIN, LOW); delay(50);
    }
    
  } 
  else {
    // when user has failed to meet timing requirements, led stays off
    Serial.println(">>> FAILED <<<");
    digitalWrite(LED_PIN, LOW); 
  }
  
  // game reset
  isPlaying = false;
  currentCounter = 0;
  displayNumber(0);
}