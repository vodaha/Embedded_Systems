int LED1 = 10;
int LED2 = 9;
int LED3 = 8;

void setup() {
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
}

void loop() {
  // one-by-one “chase” pattern
  digitalWrite(LED1, HIGH); 
  delay(50); 
  digitalWrite(LED1, LOW);
  delay(50); 

  digitalWrite(LED2, HIGH); 
  //delay(50);
  digitalWrite(LED2, LOW);
  //delay(50); 
  
  digitalWrite(LED3, HIGH); 
  delay(50); 
  digitalWrite(LED3, LOW);
  delay(50); 
}
