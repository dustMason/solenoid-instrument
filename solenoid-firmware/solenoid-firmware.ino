byte directionPin = 12;
byte stepPin = 11;
byte resetPin = A1;

unsigned long waitTime = 10;
int currentNote = 440;

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20);
  pinMode(directionPin, OUTPUT);
  pinMode(stepPin, OUTPUT);
  pinMode(resetPin, OUTPUT);
  digitalWrite(directionPin, HIGH);
  digitalWrite(resetPin, HIGH);
}

void loop() {
  int t = Serial.parseInt();
  if (currentNote != t && t > 50) {
    tone(stepPin, t);
    currentNote = t;
  }
}



