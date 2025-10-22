#ifndef HELPER_H
#define HELPER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

extern ESP8266WebServer server;

void HELPER_init();
void HELPER_showTime();
String HELPER_getDateTime();
bool HELPER_saveToFile( const char *, const uint8_t *, uint32_t );
void HELPER_radioOn();
void HELPER_radioOff();
bool HELPER_isWebServerRunning();

#endif  // HELPER_H