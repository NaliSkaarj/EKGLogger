#include <Arduino.h>
extern "C" {
  #include "user_interface.h"
}

void setup() {
  Serial.begin( 115200 );
}

void loop() {
  uint16_t adc_value = system_adc_read();
  Serial.println(adc_value);

  delay(5);
}