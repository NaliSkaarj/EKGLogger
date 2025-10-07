#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(10000);
  Serial.printf("Flash chip size (real): %u bytes\n", ESP.getFlashChipRealSize());
  Serial.printf("Flash chip size (IDE):  %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("Flash chip speed:       %u Hz\n", ESP.getFlashChipSpeed());
  Serial.printf("CPU chip speed:         %u MHz\n", ESP.getCpuFreqMHz());
  Serial.print( "Reset info:             ");
  Serial.println(ESP.getResetInfo());
  Serial.print( "Reset reason:           ");
  Serial.println(ESP.getResetReason());
  Serial.println(ESP.getFullVersion());
}

void loop() {
  delay(20000);
  Serial.println("*");
  Serial.print( "Free stack: ");
  Serial.println(ESP.getFreeContStack());
  Serial.print( "Free heap:  ");
  Serial.println(ESP.getFreeHeap());
  // Serial.println(ESP.getMaxFreeBlockSize());
  // Serial.println(ESP.getHeapFragmentation());
}