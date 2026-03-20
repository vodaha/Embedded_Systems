// Lab Task 2 - Joystick (A0/A1) -> 4 directions -> 4 LEDs + Serial Monitor
// Arduino Uno: VRx=A0, VRy=A1
// LEDs: D8=UP, D9=DOWN, D10=LEFT, D11=RIGHT (you can change mapping)

const int JOY_X = A0;
const int JOY_Y = A1;

// LED pins (change if you wired differently)
const int LED_UP    = 8;
const int LED_DOWN  = 9;
const int LED_LEFT  = 10;
const int LED_RIGHT = 11;

// Thresholds & dead-zone (tune if needed)
const int CENTER = 512;          // expected center for 10-bit ADC
const int DEADZONE = 60;         // +/- around center treated as neutral

// Derived thresholds
const int LOW_TH  = DEADZONE;  // below this -> negative direction
const int HIGH_TH = CENTER + DEADZONE;  // above this -> positive direction

enum Direction { NEUTRAL, UP, DOWN, LEFT, RIGHT };
Direction lastDir = NEUTRAL;

void allLedsOff() {
  digitalWrite(LED_UP, LOW);
  digitalWrite(LED_DOWN, LOW);
  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, LOW);
}

void setDirLed(Direction dir) {
  allLedsOff();
  switch (dir) {
    case UP:    digitalWrite(LED_UP, HIGH); break;
    case DOWN:  digitalWrite(LED_DOWN, HIGH); break;
    case LEFT:  digitalWrite(LED_LEFT, HIGH); break;
    case RIGHT: digitalWrite(LED_RIGHT, HIGH); break;
    default: /* NEUTRAL */ break;
  }
}

const char* dirToText(Direction dir) {
  switch (dir) {
    case UP: return "UP";
    case DOWN: return "DOWN";
    case LEFT: return "LEFT";
    case RIGHT: return "RIGHT";
    default: return "NEUTRAL";
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(LED_UP, OUTPUT);
  pinMode(LED_DOWN, OUTPUT);
  pinMode(LED_LEFT, OUTPUT);
  pinMode(LED_RIGHT, OUTPUT);

  allLedsOff();
}

void loop() {
  int x = analogRead(JOY_X);
  int y = analogRead(JOY_Y);

  Direction dir = NEUTRAL;

  // Decide direction:
  // Priority choice: LEFT/RIGHT first, else UP/DOWN.
  // (You can swap priority if you prefer.)
  if (x < LOW_TH) dir = LEFT;
  else if (x > HIGH_TH) dir = RIGHT;
  else if (y < LOW_TH) dir = DOWN;
  else if (y > HIGH_TH) dir = UP;
  else dir = NEUTRAL;

  // Update outputs only if direction changed
  if (dir != lastDir) {
    setDirLed(dir);
    Serial.println(dirToText(dir));
    lastDir = dir;
  }

  // Small delay to reduce flicker/spam (still real-time)
  delay(20);
}
