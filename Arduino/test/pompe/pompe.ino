#include <Servo.h>

Servo pump;

#define PUMPPIN 9
#define WAITTIME 2000

void setup() {
  // put your setup code here, to run once:
  pump.attach(PUMPPIN);
}

void loop() {
  // put your main code here, to run repeatedly:
  pump.write(0);
  delay(WAITTIME);
  pump.write(90);
  delay(WAITTIME);
}
