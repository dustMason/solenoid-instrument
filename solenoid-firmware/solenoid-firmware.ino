#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"

Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 

int currentNote = 0;
int currentChannel = 0;

void setup() {
  Serial.begin(115200);
  AFMS.begin(); // default freq is 1.6KHz
  /* AFMS.setPin(MOTOR1_B, LOW); */
}

void loop() {
  if (Serial.available() == 2) {
    currentNote = Serial.read();
    currentChannel = Serial.read();
    unsigned int freq = (float(440) * pow(2, float((currentNote - 57) / float(12))));

    Serial.println(currentNote);
    Serial.println(currentChannel);
    Serial.println(freq);

    /* AFMS.setPWM(MOTOR1_A, freq); */
    /* AFMS.setPWM(MOTOR1_B, freq); */
    AFMS.setPin(MOTOR1_A, HIGH);
    AFMS.setPin(MOTOR1_B, LOW);

    /* if (currentChannel == 0) { */
    /*   tone1.play(freq); */
    /* } else if (currentChannel == 1) { */
    /*   tone2.play(freq); */
    /* } */
  }
}
