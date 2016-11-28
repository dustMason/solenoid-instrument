#include <Tone.h>

byte stepPins[] = { 2, 3, 4, 12 };
byte dirPins[] = { 5, 6, 7, 13 };

// atmega328 has only 2 extra HW timers, so we can only use two Tone objects
Tone tone1;
Tone tone2;

int currentNote = 0;
int currentChannel = 0;

void setup() {
  Serial.begin(115200);
  tone1.begin(stepPins[0]);
  tone2.begin(stepPins[1]);
  for (int i = 0; i < 4; i++) {
    pinMode(dirPins[i], OUTPUT);
    digitalWrite(dirPins[i], LOW);
  }
  // pin 8 is enable pin on CNC shield
  // LOW means all motors enabled
  pinMode(8, OUTPUT);
  digitalWrite(8, LOW);
}

void loop() {
  if (Serial.available() == 2) {
    currentNote = Serial.read();
    currentChannel = Serial.read();
    unsigned int freq = (float(440) * pow(2, float((currentNote - 57) / float(12))));
    // Serial.println(currentNote);
    // Serial.println(currentChannel);
    // Serial.println(freq);
    if (currentChannel == 0) {
      tone1.play(freq);
    } else if (currentChannel == 1) {
      tone2.play(freq);
    }
  }
}
