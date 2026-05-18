// Flow:
// 1. Enter 4 keypad digits and press #
// 2. System locks
// 3. Enter the same code using the IR remote
// 4. System unlocks
// 5. RFID scanning is enabled
// 6. UID is printed as: TAG,XXXXXXXX

#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <IRremote.hpp>
#include <string.h>

// IR LED feedback uses pin 13, which conflicts with RC522 SCK, so it is disabled.
#ifndef DISABLE_LED_FEEDBACK
#define DISABLE_LED_FEEDBACK false
#endif

// -------------------- Pins --------------------
#define IR_RECEIVE_PIN 2

#define RFID_SS_PIN   10
#define RFID_RST_PIN  9

#define GREEN_LED A2
#define RED_LED   A3

// -------------------- IR Codes --------------------
#define IR_KEY_0 0x16
#define IR_KEY_1 0x0C
#define IR_KEY_2 0x18
#define IR_KEY_3 0x5E
#define IR_KEY_4 0x08
#define IR_KEY_5 0x1C
#define IR_KEY_6 0x5A
#define IR_KEY_7 0x42
#define IR_KEY_8 0x52
#define IR_KEY_9 0x4A

// -------------------- Keypad --------------------
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {3, 4, 5, 6};
byte colPins[COLS] = {7, 8, A0, A1};

// Create keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// -------------------- RFID --------------------
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN); // RC522 reader object
bool rfidOk = false; // RFID init status

// -------------------- State --------------------
enum SystemState {
  WAITING_FOR_CODE,
  LOCKED,
  UNLOCKED
};

SystemState currentState = WAITING_FOR_CODE;

// -------------------- Code Buffers --------------------
const byte CODE_LENGTH = 4;

// Extra byte is for the string terminator
char lockCode[CODE_LENGTH + 1] = "";      // Code entered from keypad
char keypadBuffer[CODE_LENGTH + 1] = "";  // Stores keypad digits before #
char irBuffer[CODE_LENGTH + 1] = "";      // Stores IR digits for comparison

byte keypadIndex = 0;    // Current keypad digit count
byte irIndex = 0;        // Current IR digit count

// -------------------- Buffer Helpers --------------------
void clearKeypadBuffer() {
  keypadIndex = 0;
  keypadBuffer[0] = '\0';
}

void clearIrBuffer() {
  irIndex = 0;
  irBuffer[0] = '\0';
}

bool isDigitKey(char key) {
  return key >= '0' && key <= '9';
}

// -------------------- IR Mapping --------------------
int irCommandToDigit(uint8_t command) {
  switch (command) {
    case IR_KEY_0: return 0;
    case IR_KEY_1: return 1;
    case IR_KEY_2: return 2;
    case IR_KEY_3: return 3;
    case IR_KEY_4: return 4;
    case IR_KEY_5: return 5;
    case IR_KEY_6: return 6;
    case IR_KEY_7: return 7;
    case IR_KEY_8: return 8;
    case IR_KEY_9: return 9;
    default: return -1;
  }
}

// -------------------- LEDs --------------------
void setStateLEDs() {
  if (currentState == LOCKED) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  }
  else if (currentState == UNLOCKED) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  }
}

void updateWaitingLEDs() {
  static unsigned long lastBlink = 0;
  static bool state = false;

  if (currentState != WAITING_FOR_CODE) {
    return;
  }

  // Blink both LEDs while waiting for the code
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    state = !state;

    digitalWrite(GREEN_LED, state ? HIGH : LOW);
    digitalWrite(RED_LED, state ? HIGH : LOW);
  }
}

void flashSuccess() {
  for (byte i = 0; i < 3; i++) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, HIGH);
    delay(120);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    delay(120);
  }

  setStateLEDs();
}

void flashWrongCode() {
  for (byte i = 0; i < 3; i++) {
    digitalWrite(RED_LED, HIGH);
    delay(150);
    digitalWrite(RED_LED, LOW);
    delay(150);
  }

  setStateLEDs();
}

// -------------------- State Change --------------------
void changeState(SystemState newState) {
  currentState = newState;

  clearKeypadBuffer();
  clearIrBuffer();

  if (currentState == WAITING_FOR_CODE) {
    Serial.println(F("STATE,WAITING_FOR_CODE"));
    Serial.println(F("INFO,Enter 4 digits on keypad, then press #"));
  }
  else if (currentState == LOCKED) {
    Serial.println(F("STATE,LOCKED"));
    Serial.println(F("INFO,RFID disabled. Use IR remote to unlock."));
    setStateLEDs();
  }
  else if (currentState == UNLOCKED) {
    Serial.println(F("STATE,UNLOCKED"));
    Serial.println(F("INFO,RFID active. Scan card now."));
    setStateLEDs();
  }
}

// -------------------- RFID Init --------------------
void initRFID() {
  // Keep SS inactive before SPI starts
  pinMode(RFID_SS_PIN, OUTPUT);
  digitalWrite(RFID_SS_PIN, HIGH);

  // Reset RC522 before setup
  pinMode(RFID_RST_PIN, OUTPUT);
  digitalWrite(RFID_RST_PIN, LOW);
  delay(50);
  digitalWrite(RFID_RST_PIN, HIGH);
  delay(100);

  SPI.begin();
  delay(50);

  // Start RC522 communication
  rfid.PCD_Init();
  delay(100);

  // Enable antenna before version check
  rfid.PCD_AntennaOn();

  // Read version register to check if the reader responds
  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);

  // This is the RC522 chip version, not the library version.
  // Normal values are 0x91, 0x92, or sometimes 0x88.
  // Values 0x00 or 0xFF usually mean communication failed.

  // If RFID init fails, the loop will retry later because some RC522 modules need another attempt.

  Serial.print(F("RFID_VERSION,0x"));
  Serial.println(version, HEX);

  if (version == 0x00 || version == 0xFF) {
    Serial.println(F("ERROR,RFID_NOT_RESPONDING"));
    rfidOk = false;
  } else {
    Serial.println(F("INFO,RFID_OK"));
    rfidOk = true;
  }
}

// -------------------- Keypad Handling --------------------
void handleKeypad() {
  char key = keypad.getKey();

  // Return if no key was pressed
  if (!key) {
    return;
  }

  Serial.print(F("KEYPAD,"));
  Serial.println(key);

  if (currentState == LOCKED) {
    Serial.println(F("INFO,Keypad ignored while locked"));
    return;
  }

  if (key == '*') {
    clearKeypadBuffer();
    Serial.println(F("INFO,Keypad cleared"));
    return;
  }

  if (key == '#') {
    if (keypadIndex == CODE_LENGTH) {
      strcpy(lockCode, keypadBuffer);

      Serial.println(F("INFO,Code saved"));
      changeState(LOCKED);
    } else {
      Serial.println(F("ERROR,Need 4 digits before #"));
    }

    return;
  }

  if (isDigitKey(key)) {
    if (keypadIndex < CODE_LENGTH) {
      keypadBuffer[keypadIndex] = key;
      keypadIndex++;
      keypadBuffer[keypadIndex] = '\0';

      Serial.print(F("INFO,Keypad digits="));
      Serial.println(keypadIndex);
    } else {
      Serial.println(F("ERROR,Keypad full. Use * or #"));
    }
  }
}

// -------------------- IR Handling --------------------
void handleIR() {
  // Return if no IR signal is available
  if (!IrReceiver.decode()) {
    return;
  }

  // Ignore repeat codes caused by holding a button
  if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
    IrReceiver.resume();
    return;
  }

  uint8_t command = IrReceiver.decodedIRData.command; // Button command code
  int digit = irCommandToDigit(command);

  Serial.print(F("IR,0x"));
  Serial.println(command, HEX);

  if (currentState != LOCKED) {
    IrReceiver.resume();
    return;
  }

  if (digit < 0) {
    Serial.println(F("ERROR,Unknown IR key"));
    IrReceiver.resume();
    return;
  }

  if (irIndex < CODE_LENGTH) {
    irBuffer[irIndex] = char('0' + digit);
    irIndex++;
    irBuffer[irIndex] = '\0';

    Serial.print(F("INFO,IR digits="));
    Serial.println(irIndex);
  }

  if (irIndex == CODE_LENGTH) {
    if (strcmp(irBuffer, lockCode) == 0) {
      Serial.println(F("RESULT,IR_CODE_CORRECT"));
      changeState(UNLOCKED);
      flashSuccess();
    } else {
      Serial.println(F("RESULT,IR_CODE_WRONG"));
      clearIrBuffer();
      flashWrongCode();
    }
  }

  IrReceiver.resume();
}

// -------------------- RFID Handling --------------------
void printUidAsTagLine(MFRC522::Uid *uid) {
  Serial.print(F("TAG,"));

  for (byte i = 0; i < uid->size; i++) {
    if (uid->uidByte[i] < 0x10) {
      Serial.print(F("0"));
    }

    Serial.print(uid->uidByte[i], HEX);
  }

  Serial.println();
}

void handleRFID() {
  // RFID works only after unlocking
  if (currentState != UNLOCKED) {
    return;
  }

  // If RFID setup failed, retry every 2 seconds while unlocked
  if (!rfidOk) {
    static unsigned long lastRetry = 0;

    if (millis() - lastRetry >= 2000) {
      lastRetry = millis();
      Serial.println(F("INFO,Retry RFID init"));
      initRFID();
    }

    return;
  }

  // Check if a new RFID card/tag is near the reader
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // Try to read the UID from the detected card
  if (!rfid.PICC_ReadCardSerial()) {
    Serial.println(F("ERROR,RFID_READ_FAIL"));
    rfid.PICC_HaltA();       // Stop communication with the card
    rfid.PCD_StopCrypto1();  // Stop RC522 crypto session
    return;
  }

  // Print UID after a successful read
  printUidAsTagLine(&rfid.uid);

  flashSuccess();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(700);
}

// -------------------- Setup / Loop --------------------
void setup() {
  Serial.begin(9600);
  delay(1000);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  Serial.println(F("LAB7_SECURITY_SYSTEM_START"));

  initRFID();

  // D13 is SPI SCK for RC522, so IR feedback LED stays disabled.
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  changeState(WAITING_FOR_CODE);
}

void loop() {
  updateWaitingLEDs();

  handleKeypad();
  handleIR();
  handleRFID();
}
