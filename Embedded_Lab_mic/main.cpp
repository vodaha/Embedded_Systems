#include <Arduino.h>
#include <LiquidCrystal.h>
#include <math.h>

// LCD: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(12, 11, 5, 4, 3, 6);

// Pins
const byte LED_PIN = 8;
const byte MIC_AO  = A0;   // analog output
const byte MIC_DO  = 2;    // digital output -> interrupt pin

// dB settings
// This is NOT absolute dB SPL.
// It is relative dB referenced to 2 mV RMS.
const float ADC_REF_VOLTAGE = 5.0;  // 0 = 0V, 1023 = 5V for 10-bit ADC
const float DB_REFERENCE_V  = 0.02;   // 2 mV RMS reference - should be tuned after testing with actual sound levels
float DB_THRESHOLD = 15.0;            // tune this after testing


//// TIMING (non-blocking) ////

// in 50 ms we collect enough samples for a stable RMS calculation and dB reading
const unsigned long WINDOW_MS = 50;
// We update the LCD every 200 ms to avoid flicker and give a stable reading
const unsigned long LCD_MS    = 200;
// We send serial data every 100 ms for real-time monitoring on the PC
const unsigned long SERIAL_MS = 100;
// We keep the LED on for 300 ms after a sound event is detected
const unsigned long LED_ON_MS = 300;

//// FOR RMS CALCULATION OVER A WINDOW ////

unsigned long sampleCount = 0; // number of samples collected in the current window
double sumSamples = 0.0;       // sum of samples for mean calculation
double sumSquares = 0.0;       // sum of squares for mean square calculation

float vrms = 0.0;              // RMS voltage in volts
float dbLevel = 0.0;           // calculated dB level relative to DB_REFERENCE_V
bool aboveThreshold = false; 

//// INTERRUPT AND LED STATE ////

volatile bool soundInterruptFlag = false;   // set in ISR when sound event is detected
volatile unsigned long interruptCount = 0;  // count of sound events detected (for display and monitoring)

//// TIMERS ////

unsigned long lastWindowStart = 0;
unsigned long lastLcdUpdate   = 0;
unsigned long lastSerialSend  = 0;
unsigned long ledStartTime    = 0;

// LED state
bool ledOn = false;

//// Interrupt Service Routine - called when MIC_DO detects a sound event (FALLING edge) ////
void soundISR() {
  soundInterruptFlag = true;
  interruptCount++; // when the interrupt triggers, we set the flag and increment the event count.
  // The main loop will handle the rest (turning on the LED, etc).
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(MIC_DO, INPUT);

  digitalWrite(LED_PIN, LOW);

  // Initialize LCD and print startup message
  lcd.begin(16, 2);             // initialize LCD with 16 columns and 2 rows
  lcd.setCursor(0, 0);
  lcd.print("Sound dB Meter");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  Serial.begin(9600);

  // Attach interrupt to MIC_DO pin for FALLING edge (sound event)
  // when Falling edge is detected on MIC_DO, soundISR will be called
  attachInterrupt(digitalPinToInterrupt(MIC_DO), soundISR, FALLING);

  // remember the start time for the first window and LCD/Serial updates
  lastWindowStart = millis();
  lastLcdUpdate   = millis();
  lastSerialSend  = millis();
}

void loop() {
  unsigned long now = millis();

  // 1) Read analog sample - we do this as fast as possible to get a good number 
  //    of samples for the RMS calculation within the WINDOW_MS time frame
  int sample = analogRead(MIC_AO);

  // Accumulate for RMS calculation
  sampleCount++;
  sumSamples += sample;
  sumSquares += (double)sample * (double)sample;

  // 2) Every WINDOW_MS, compute Vrms and dB (50 ms)
  if (now - lastWindowStart >= WINDOW_MS) {
    if (sampleCount > 0) {
      // Mean and mean square in ADC counts
      double mean = sumSamples / sampleCount;
      double meanSquare = sumSquares / sampleCount;

      // AC RMS in ADC counts:
      // sqrt(E[x^2] - (E[x])^2)
      double variancePart = meanSquare - (mean * mean);
      if (variancePart < 0) variancePart = 0;   // safety
      double rmsCounts = sqrt(variancePart);

      // Convert ADC counts RMS to voltage RMS
      vrms = (float)(rmsCounts * ADC_REF_VOLTAGE / 1023.0);

      // Convert to relative dB
      if (vrms < 0.001) {
        vrms = 0.001;   // avoid log(0), floor at 1 mV
      }

      dbLevel = 20.0 * log10(vrms / DB_REFERENCE_V);

      // Check if above threshold
      aboveThreshold = (dbLevel >= DB_THRESHOLD);
    }

    // Reset window accumulators
    sampleCount = 0;
    sumSamples = 0.0;
    sumSquares = 0.0;
    lastWindowStart = now;
  }

  // 3) If interrupt happened, start LED pulse
  if (soundInterruptFlag) {
    noInterrupts();
    soundInterruptFlag = false;
    interrupts();

    ledOn = true;
    ledStartTime = now;
    digitalWrite(LED_PIN, HIGH);
  }

  // 4) Turn LED off after LED_ON_MS
  if (ledOn && (now - ledStartTime >= LED_ON_MS)) {
    ledOn = false;
    digitalWrite(LED_PIN, LOW);
  }

  // 5) Update LCD periodically
  if (now - lastLcdUpdate >= LCD_MS) {
    lastLcdUpdate = now;

    lcd.setCursor(0, 0);
    lcd.print("dB:");
    lcd.print(dbLevel, 1); // print dB with 1 decimal place
    lcd.print("    ");

    lcd.setCursor(10, 0);
    lcd.print("T:");
    lcd.print(DB_THRESHOLD, 0);
    lcd.print(" ");

    lcd.setCursor(0, 1);
    lcd.print("Evt:");
    lcd.print(interruptCount);
    lcd.print("   ");

    lcd.setCursor(10, 1);
    if (aboveThreshold) {
      lcd.print("LOUD ");
    } else {
      lcd.print("quiet");
    }
  }

  // 6) Send UART data
  // Format: millis,dbLevel,aboveThreshold,eventCount
  if (now - lastSerialSend >= SERIAL_MS) {
    lastSerialSend = now;

    // send the current time (for PC timestamping), dB level, whether it's above threshold, and event count to the serial port for real-time monitoring on the PC
    Serial.print(now);
    Serial.print(",");
    Serial.print(dbLevel, 1);
    Serial.print(",");
    Serial.print(aboveThreshold ? 1 : 0);
    Serial.print(",");
    Serial.println(interruptCount);
  }
}