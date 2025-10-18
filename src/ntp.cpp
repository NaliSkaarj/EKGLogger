#include "ntp.h"
#include "credentials.h"
#include <ESP8266WiFi.h>
#include <time.h>

void NTP_init() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_SSID, wifi_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(300);
  }
  Serial.println();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());

  configTime(7200, 0, "pool.ntp.org", "time.nist.gov");  // 7200s = +2h

  Serial.println("Waiting for NTP time...");
  // wait for time synchronization — 'time(nullptr)' will be > 8*3600 when synchronized
  time_t now = time(nullptr);
  uint32_t start = millis();
  while (now < 1000000000UL) { // iftime less than ~2001-09-09 => no synchronization yet
    delay(200);
    now = time(nullptr);
    if (millis() - start > 10000) { // timeout 10s
      Serial.println("NTP sync timeout, continuing with whatever time is available");
      break;
    }
  }
}

void NTP_showTime() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
  Serial.printf("Local time: %s\n", buf);

  struct tm utc;
  gmtime_r(&now, &utc);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &utc);
  Serial.printf("UTC time:   %s\n", buf);
}

String NTP_getFileTimestamp() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &timeinfo);

  return String(buf);
}
