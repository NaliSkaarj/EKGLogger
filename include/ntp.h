#ifndef NTP_H
#define NTP_H

#include <Arduino.h>

void NTP_init();
void NTP_showTime();
String NTP_getFileTimestamp();

#endif  // NTP_H