#ifndef HELPER_H
#define HELPER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

extern ESP8266WebServer server;

void HELPER_init();
void HELPER_showTime();
String HELPER_getFileTimestamp();
bool HELPER_saveToFile( const char *, const uint8_t *, uint32_t );

#endif  // HELPER_H