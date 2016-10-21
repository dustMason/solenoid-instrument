byte directionPin = 12;
byte stepPin = 11;
byte resetPin = A1;

unsigned long waitTime = 10;

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(2);
  pinMode(directionPin, OUTPUT);
  pinMode(stepPin, OUTPUT);
  pinMode(resetPin, OUTPUT);
  digitalWrite(directionPin, HIGH);
}

void loop() {
  int t = Serial.parseInt();
  if (t > 0) {
    waitTime = 1000/t;
  }
  digitalWrite(resetPin, HIGH);
  digitalWrite(stepPin, HIGH);
  delay(waitTime);
  digitalWrite(stepPin, LOW);
  digitalWrite(resetPin, LOW);
}



