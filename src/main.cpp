#include <Arduino.h>
#define LO_M D0         // Right arm
#define LO_P D1         // Left arm

void setup() {
  Serial.begin(115200);
}

void loop() {
  if( digitalRead(LO_M) == HIGH ) {
    Serial.println( "Right Arm electrode error!" );
  } else if( digitalRead(LO_P) == HIGH ) {
    Serial.println( "Left Arm electrode error!" );
  } else {
    Serial.println( analogRead(A0) );
  }
  delay(10);
}