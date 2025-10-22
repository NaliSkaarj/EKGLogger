#include "helper.h"
#include "credentials.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <LittleFS.h>

#define CUSTOM_TIME_EPOCH 1761994800  // Date and time (GMT): Saturday, November 1, 2025 11:00:00 AM

ESP8266WebServer server(80);
bool initialized = false;
bool wifiConnected = false;
bool webServerRunning = false;

// Simple HTML page with a list of files
static void handleRoot() {
  String html = R"rawliteral(
  <html>
  <head>
    <meta charset="utf-8">
    <title>ESP8266 SPIFFS File Browser</title>
    <style>
      body { font-family: Arial; margin: 20px; background: #fafafa; }
      h2 { color: #333; }
      ul { list-style-type: none; padding: 0; }
      li { margin: 6px 0; padding: 6px; background: #fff; border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); display: flex; justify-content: space-between; align-items: center; }
      a.file { text-decoration: none; color: #0077cc; font-weight: bold; }
      a.delete { color: #cc0000; text-decoration: none; font-size: 18px; padding-left: 10px; }
      a.delete:hover { color: #ff3333; }
    </style>
    <script>
      function confirmDelete(file) {
        if (confirm('Delete ' + file + '?')) {
          window.location = '/delete?file=' + encodeURIComponent(file);
        }
      }
    </script>
  </head>
  <body>
    <h2>ESP8266 SPIFFS File Browser</h2>
    <ul>
  )rawliteral";

  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    html += "<li><span><a class='file' href='/file?name=" + fileName + "'>" + fileName + "</a> (" + String(fileSize) + " bytes)</span>";
    html += "<a class='delete' href='#' onclick='confirmDelete(\"" + fileName + "\")'>&#128465;</a></li>";  // 🗑️ ikona kosza (Unicode)
  }

  html += R"rawliteral(
    </ul>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// Download a specific file
static void handleFileDownload() {
  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing 'name' parameter");
    return;
  }

  String path = server.arg("name");
  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  File file = LittleFS.open(path, "r");
  server.streamFile(file, "application/octet-stream");
  file.close();
}

static void handleFileDelete() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing 'file' parameter");
    return;
  }

  String path = server.arg("file");
  if (!path.startsWith("/")) path = "/" + path;

  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  if (LittleFS.remove(path)) {
    Serial.println("Deleted: " + path);

    String html =
      "<html><head>"
      "<meta charset='utf-8'>"
      "<meta http-equiv='refresh' content='1;url=/' />"
      "<style>"
      "body { font-family: Arial; background:#fafafa; color:#333; margin:40px; }"
      "a { color:#0077cc; text-decoration:none; font-weight:bold; }"
      "a:hover { text-decoration:underline; }"
      "</style>"
      "</head><body>"
      "<h3>Deleted: " + path + "</h3>"
      "<p>Returning to file list...</p>"
      "<p><a href='/'>Back to Home</a></p>"
      "</body></html>";

    server.send(200, "text/html", html);
  } else {
    server.send(500, "text/plain", "Delete failed");
  }
}

static void webServerStart() {
  server.on("/", handleRoot);
  server.on("/file", handleFileDownload);
  server.on("/delete", handleFileDelete);
  server.begin();
  Serial.println("HTTP server started");
  webServerRunning = true;
}

static void webServerStop() {
  server.close();   // close connections
  server.stop();
  Serial.println("HTTP server stopped");
  webServerRunning = false;
}

static bool wifiConnect() {
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi already connected");
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_SSID, wifi_PASS);
  Serial.print("Connecting to WiFi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(300);
    if (millis() - start > 5000) { // timeout 5s
      Serial.println();
      Serial.println("WiFi connection timeout");
      return false;
    }
  }
  Serial.println();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

// Set time manually
static void setManualTime() {
  struct timeval tv;
  tv.tv_sec = CUSTOM_TIME_EPOCH;  // time in seconds from 1970-01-01
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  Serial.println("Manual time set.");
}

static bool setNTPTime() {
  if( wifiConnected ) {
    Serial.println("Synchronizing time via NTP...");
    configTime(7200, 0, "pool.ntp.org", "time.nist.gov");  // 7200s = +2h
    // wait for time synchronization — 'time(nullptr)' will be > 8*3600 when synchronized
    time_t now = time(nullptr);
    uint32_t start = millis();
    while (now < 1000000000UL) { // iftime less than ~2001-09-09 => no synchronization yet
      delay(200);
      now = time(nullptr);
      if (millis() - start > 10000) { // timeout 10s
        Serial.println("NTP sync timeout, continuing with manual time set");
        return false;
      }
    }
    return true;
  }

  return false;
}

void HELPER_init() {
  if(wifiConnect()) {
    wifiConnected = true;
  } else {
    wifiConnected = false;
  }

  if(setNTPTime()) {
    Serial.println("Time synchronized via NTP.");
  } else {
    setManualTime();
  }

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS!");
  }

  if( wifiConnected ) {
    webServerStart();
  }

  initialized = true;
}

void HELPER_showTime() {
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

String HELPER_getDateTime() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &timeinfo);

  return String(buf);
}

bool HELPER_saveToFile( const char * fileName, const uint8_t * data, uint32_t len ) {
  if( !LittleFS.begin() ) {
    Serial.println( "[LittleFS] File system mount error" );
    return false;
  }

  // free space check
  FSInfo fs_info;
  LittleFS.info( fs_info );

  uint32_t freeBytes = fs_info.totalBytes - fs_info.usedBytes;

  if( len > freeBytes ) {
    Serial.printf( "[LittleFS] No free space! Need: %u B, free: %u B\n", (unsigned)len, (unsigned)freeBytes );
    return false;
  }

  // open file for write (mode "a" — appending data at the end of file, create file if doesn't exist)
  File file = LittleFS.open( fileName, "a" );
  if( !file ) {
    Serial.printf( "[LittleFS] Can't open file for write: %s\n", fileName );
    return false;
  }

  // save data
  size_t written = file.write( data, len );
  file.close();

  if( written != len ) {
    Serial.printf( "[LittleFS] Write error! Wrote %u from %u B\n", (unsigned)written, (unsigned)len );
    return false;
  }

  Serial.printf( "[LittleFS] File saved: %s (%u B)\n", fileName, (unsigned)len );
  return true;
}
