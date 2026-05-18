#include <Arduino.h>
#include <Servo.h>

// PINS
const byte BTN1_PIN   = 2;
const byte BTN2_PIN   = 3;
const byte BUZZER_PIN = 4;
const byte SERVO_PIN  = 5;

const byte IN1_PIN = 8;
const byte IN2_PIN = 9;
const byte IN3_PIN = 10;
const byte IN4_PIN = 11;

// CONFIG
const bool USE_PASSIVE_BUZZER = false;

const int SERVO_NEUTRAL  = 90;
const int SERVO_P1_ANGLE = 0;
const int SERVO_P2_ANGLE = 180;

const int WIN_SCORE = 3;

const unsigned long DEBOUNCE_MS     = 25;
const unsigned long INTER_ROUND_MS  = 1200; // pause between rounds
const unsigned long ACTIVE_TIMEOUT_MS = 3000; // max reaction time after GO

// 28BYJ-48 with ULN2003, half-step mode
const int STEPS_PER_REV = 4096;           // one full turn
const int TUG_STEP_STEPS = STEPS_PER_REV / 12; // about 30° per win

unsigned int stepDelayMicros = 1500; // step delay; lower is faster but may skip steps

// GAME STATE
enum GameState {
  IDLE,         // waiting to start
  WAIT_RANDOM,  // waiting before GO
  ACTIVE,       // players can react
  INTER_ROUND,  // pause after round
  MATCH_OVER    // waiting for reset
};

GameState state = IDLE; // current state

// PLAYERS
String player1 = "P1";
String player2 = "P2";
int score1 = 0;
int score2 = 0;

// TIMING
unsigned long waitStartMs = 0;        // start of random wait
unsigned long randomDelayMs = 0;      // random wait before GO
unsigned long interRoundStartMs = 0;  // start of round pause
unsigned long buzzStartMs = 0;        // GO time in ms
unsigned long buzzTimeUs = 0;         // GO time in us

// BUZZER
bool buzzerOn = false;
unsigned long buzzerOffAtMs = 0;

// SERVO
Servo winnerServo;  // points to winner

// STEPPER
// stepper pins and sequence
const byte stepPins[4] = {IN1_PIN, IN2_PIN, IN3_PIN, IN4_PIN};

// half-step sequence
const byte halfStepSeq[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

int stepIndex = 0;
long currentPositionSteps = 0;   // position from home: +P1, -P2

// REACTION TIMES
// -1 means no press yet; otherwise time in us
long reaction1Us = -1;
long reaction2Us = -1;

// BUTTON DEBOUNCE
struct ButtonDebounce {
  byte pin;
  bool lastReading;
  bool stableState;
  unsigned long lastDebounceTime;
};

ButtonDebounce btn1 = {BTN1_PIN, HIGH, HIGH, 0};
ButtonDebounce btn2 = {BTN2_PIN, HIGH, HIGH, 0};

// HELPERS
bool readButtonPressed(ButtonDebounce &btn) {
  bool reading = digitalRead(btn.pin);
  bool pressed = false;

  if (reading != btn.lastReading) {
    btn.lastDebounceTime = millis();
  }

  if ((millis() - btn.lastDebounceTime) > DEBOUNCE_MS) {
    if (reading != btn.stableState) {
      btn.stableState = reading;
      if (btn.stableState == LOW) { // pressed = LOW
        pressed = true;
      }
    }
  }

  btn.lastReading = reading;
  return pressed;
}

void startGoSignal() {
  if (USE_PASSIVE_BUZZER) {
    tone(BUZZER_PIN, 2000, 60);
  } else {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerOn = true;
    buzzerOffAtMs = millis() + 60;
  }
}

void updateBuzzer() {
  if (!USE_PASSIVE_BUZZER && buzzerOn && millis() >= buzzerOffAtMs) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
  }
}

void setStepperOutput(const byte pattern[4]) {
  for (byte i = 0; i < 4; i++) {
    digitalWrite(stepPins[i], pattern[i]);
  }
}

void releaseStepper() {
  for (byte i = 0; i < 4; i++) {
    digitalWrite(stepPins[i], LOW);
  }
}

void stepMotor(int direction) {
  stepIndex += direction;

  if (stepIndex > 7) stepIndex = 0;
  if (stepIndex < 0) stepIndex = 7;

  setStepperOutput(halfStepSeq[stepIndex]);
  delayMicroseconds(stepDelayMicros);
}

void moveStepperSteps(int steps, int direction) {
  for (int i = 0; i < steps; i++) {
    stepMotor(direction);
  }
  releaseStepper();
}

void moveTugTowardWinner(int winner) {
  if (winner == 1) {
    moveStepperSteps(TUG_STEP_STEPS, +1);
    currentPositionSteps += TUG_STEP_STEPS;
  } else if (winner == 2) {
    moveStepperSteps(TUG_STEP_STEPS, -1);
    currentPositionSteps -= TUG_STEP_STEPS;
  }
}

void centerTug() {
  if (currentPositionSteps > 0) {
    moveStepperSteps(currentPositionSteps, -1);
  } else if (currentPositionSteps < 0) {
    moveStepperSteps(-currentPositionSteps, +1);
  }

  currentPositionSteps = 0;
  releaseStepper();
}

void victorySpinAndReturnHome() {
  // spin once
  moveStepperSteps(STEPS_PER_REV, +1);

  // return home
  centerTug();
}

void setServoToWinner(int winner) {
  if (winner == 1) {
    winnerServo.write(SERVO_P1_ANGLE);
  } else if (winner == 2) {
    winnerServo.write(SERVO_P2_ANGLE);
  } else {
    winnerServo.write(SERVO_NEUTRAL);
  }
}

void resetRoundData() {
  // no reaction yet
  reaction1Us = -1;
  reaction2Us = -1;
}

void startRound() {
  resetRoundData();
  randomDelayMs = random(1000, 20001); // 1 to 20 seconds
  waitStartMs = millis();
  state = WAIT_RANDOM;
  Serial.println("ROUND_WAIT");
}

void resetMatch() {
  score1 = 0;
  score2 = 0;
  state = IDLE;
  resetRoundData();
  winnerServo.write(SERVO_NEUTRAL);
  centerTug();
}

void sendRoundResult(int winner, bool falseStart) {
  long r1ms = (reaction1Us < 0) ? -1 : reaction1Us / 1000;
  long r2ms = (reaction2Us < 0) ? -1 : reaction2Us / 1000;

  Serial.print("ROUND,");
  Serial.print(winner);
  Serial.print(",");
  Serial.print(r1ms);
  Serial.print(",");
  Serial.print(r2ms);
  Serial.print(",");
  Serial.print(falseStart ? 1 : 0);
  Serial.print(",");
  Serial.print(score1);
  Serial.print(",");
  Serial.println(score2);
}

// send match winner, or NONE for tie
void sendMatchResult(int winner) {
  Serial.print("MATCH,");
  if (winner == 1) Serial.println(player1);
  else if (winner == 2) Serial.println(player2);
  else Serial.println("NONE");
}

void finishRound(int winner, bool falseStart) {
  if (winner == 1) {
    score1++;
    setServoToWinner(1);
    moveTugTowardWinner(1);
  } else if (winner == 2) {
    score2++;
    setServoToWinner(2);
    moveTugTowardWinner(2);
  } else {
    setServoToWinner(0);
  }

  sendRoundResult(winner, falseStart);

  if (score1 >= WIN_SCORE) {
    victorySpinAndReturnHome();
    sendMatchResult(1);
    state = MATCH_OVER;
  } else if (score2 >= WIN_SCORE) {
    victorySpinAndReturnHome();
    sendMatchResult(2);
    state = MATCH_OVER;
  } else {
    interRoundStartMs = millis();
    state = INTER_ROUND;
  }
}

// process one serial command line
// run command and send response
void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "PING") {
    Serial.println("PONG");
    return;
  }

  if (line == "RESET") {
    resetMatch();
    Serial.println("RESET_OK");
    return;
  }

  // Format: START,Player1Name,Player2Name
  if (line.startsWith("START,")) {
    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);

    if (secondComma == -1) {
      Serial.println("ERR,BAD_START");
      return;
    }

    player1 = line.substring(firstComma + 1, secondComma);
    player2 = line.substring(secondComma + 1);

    // clean player names
    player1.trim();
    player2.trim();

    // use defaults if names are empty
    if (player1.length() == 0) player1 = "P1";
    if (player2.length() == 0) player2 = "P2";

    score1 = 0;
    score2 = 0;
    winnerServo.write(SERVO_NEUTRAL);
    centerTug(); // center tug at match start

    Serial.println("START_OK");
    startRound();
    return;
  }

  Serial.println("ERR,UNKNOWN_CMD");
}

// read serial commands
// call handleCommand() for each complete line
void updateSerial() {
  static String line = "";

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (line.length() > 0) {
        handleCommand(line);
        line = "";
      }
    } else {
      line += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  // A0 is left floating to read noise for
  // random seed, so delays change each match
  randomSeed(analogRead(A0)); // seed random timing

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // setup stepper pins
  for (byte i = 0; i < 4; i++) {
    pinMode(stepPins[i], OUTPUT);
    digitalWrite(stepPins[i], LOW);
  }

  winnerServo.attach(SERVO_PIN); // attach servo before using it
  winnerServo.write(SERVO_NEUTRAL); // neutral position

  Serial.println("READY");
}

void loop() {
  updateSerial(); // handle serial commands
  updateBuzzer(); // stop buzzer when time is up

  // read buttons and update state
  bool p1Pressed = readButtonPressed(btn1);
  bool p2Pressed = readButtonPressed(btn2);

  switch (state) {
    case IDLE:
      break;

    case WAIT_RANDOM:
      if (p1Pressed) {
        finishRound(2, true); // P1 false start
      } else if (p2Pressed) {
        finishRound(1, true); // P2 false start
      } else if (millis() - waitStartMs >= randomDelayMs) {
        buzzStartMs = millis();
        buzzTimeUs = micros();
        startGoSignal();
        Serial.println("GO");
        state = ACTIVE;
      }
      break;

    case ACTIVE:
      if (p1Pressed && reaction1Us < 0) {
        reaction1Us = (long)(micros() - buzzTimeUs);
      }

      if (p2Pressed && reaction2Us < 0) {
        reaction2Us = (long)(micros() - buzzTimeUs);
      }

      if (reaction1Us >= 0 && reaction2Us >= 0) {
        if (reaction1Us < reaction2Us) finishRound(1, false);
        else if (reaction2Us < reaction1Us) finishRound(2, false);
        else finishRound(0, false);
      } else if (millis() - buzzStartMs >= ACTIVE_TIMEOUT_MS) {
        if (reaction1Us >= 0 && reaction2Us < 0) finishRound(1, false);
        else if (reaction2Us >= 0 && reaction1Us < 0) finishRound(2, false);
        else finishRound(0, false);
      }
      break;

    case INTER_ROUND:
      if (millis() - interRoundStartMs >= INTER_ROUND_MS) {
        startRound();
      }
      break;

    case MATCH_OVER:
      break;
  }
}