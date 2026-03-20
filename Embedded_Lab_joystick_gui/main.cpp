#include <Arduino.h>

const int JOY_X = A0;
const int JOY_Y = A1;
const int ledPins[4] = {8, 9, 10, 11}; // UP, DOWN, LEFT, RIGHT
const char* names[5] = {"NEUTRAL", "UP", "DOWN", "LEFT", "RIGHT"};

const int CENTER = 512;
const int DEADZONE = 60;

void setLeds(int dir) {
  // turn off all LED first, then turn on the one corresponding to the direction
  for (int i = 0; i < 4; i++) digitalWrite(ledPins[i], LOW);
  if (dir != 0) digitalWrite(ledPins[dir - 1], HIGH);
}

void setup() {
  // baud rate 115200 for faster data transfer to Python GUI,
  // if needed we can reduce it to 9600 for better stability
  Serial.begin(115200); 
  // setting all led pins as OUTPUT and turn them off
  for (int i = 0; i < 4; i++) pinMode(ledPins[i], OUTPUT);
  setLeds(0);
}

void loop() {
  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);

  int low  = CENTER - DEADZONE;
  int high = CENTER + DEADZONE;
  int dir = 0; 

  if      (x < low)  dir = 3;   // LEFT
  else if (x > high) dir = 4;   // RIGHT
  else if (y < low)  dir = 2;   // DOWN
  else if (y > high) dir = 1;   // UP

  setLeds(dir);

  // serial output format: x,y,direction
  Serial.print(x);
  Serial.print(",");
  Serial.print(y);
  Serial.print(",");
  Serial.println(names[dir]);

  delay(10); // Частота опроса ~100Hz
}