#include <Arduino.h>
extern "C" {
  #include "user_interface.h"
}
#include "LittleFS.h"
#include "helper.h"
#include <Ticker.h>

#define LO_M                      D0                    // Right arm electrode
#define LO_P                      D1                    // Left arm electrode
#define BUTTON_BOOT_PIN           0                     // GPIO_0 is connected to BOOT button on PCB

#define FS                        200                   // Sampling frequency in Hz
#define RISE_THRESHOLD            300
#define FALL_THRESHOLD            -500
#define REFRACTORY_PERIOD         (FS/4)                // 250ms refractory period
#define RISE_FALL_MAX_PERIOD      (FS/25)               // maximum period (in samples) between rise and fall, @200Hz => 8 samples = 40ms
#define DERIV_BUF_SIZE ((int)((FS * 15 + 999) / 1000))  // derivative buffer size to cover ~15ms duration
#define MAX_SAMPLES               4096                  // max samples to store in buffer
#define MAX_FILL_SAMPLES          20                    // max samples to "fill" when saving to FS takes too long
#define SAMPLE_INTERVAL_US        (1000000UL / FS)
#define DEBOUNCE_MS               50                    // button debounce time in milliseconds
#define LONG_PRESS_MS             1500                  // long press time in milliseconds

static volatile uint16_t last_bpm = 0;
static uint8_t rise_to_fall_period = 0;
static uint8_t refractory_counter = 0;
static uint16_t ecg_buffer[ MAX_SAMPLES ] = {0};
static uint16_t ecg_buffer_index = 0;
bool electrodeError = false;
bool btnClicked = false;
bool btnLongPress = false;
Ticker myTimer;
volatile bool wifiOffFlag = false;

static void turnWiFiOffISR() {
  wifiOffFlag = true;
}

static void handleButton() {
  static bool lastReading = HIGH;             // last raw reading
  static unsigned long lastChangeTime = 0;    // debounce timer
  unsigned long now = millis();
  bool reading = digitalRead(BUTTON_BOOT_PIN);

  // detect state change (for debounce)
  if (reading != lastReading) {
    lastChangeTime = now;
    lastReading = reading;
  }

  // check if state is stable long enough
  if ((now - lastChangeTime) > DEBOUNCE_MS) {
    static bool lastStableState = HIGH;         // last stable state
    static unsigned long pressStartTime = 0;    // time when button pressed
    static bool longPressTriggered = false;     // helper to avoid multiple triggers

    if (reading != lastStableState) {
      lastStableState = reading;

      if (lastStableState == LOW) {
        // button pressed
        pressStartTime = now;
        longPressTriggered = false;
      } else {
        // button released
        unsigned long pressDuration = now - pressStartTime;

        if (!longPressTriggered) {
          if (pressDuration >= LONG_PRESS_MS) {
            Serial.println("Button long press detected (released)");
            btnLongPress = true;
          } else {
            Serial.println("Button click detected");
            btnClicked = true;
          }
        }
      }
    }

    // check for long press while holding button
    if (lastStableState == LOW && !longPressTriggered) {
      if ((now - pressStartTime) >= LONG_PRESS_MS) {
        Serial.println("Button long press detected (holding)");
        btnLongPress = true;
        longPressTriggered = true;  // avoid multiple triggers
      }
    }
  }
}

/**
 * Call this function FS times per second (e.g. in loop with delay(1000/FS))
 * with adc_value being the latest ADC sample from ECG output.
 * It returns true if a heartbeat was detected.
 */
bool process_ecg_sample( uint16_t adc_value ) {
  static uint32_t current_time = 0;
  static int16_t deriv_buf[DERIV_BUF_SIZE] = {0};    // buffer size depends on FS (should be ~15ms in duration; @FS=200 => 3 samples, @FS=400 => 6 samples)
  static uint16_t prev_adc_value;
  static bool rise_detected = false;
  static bool fall_detected = false;
  bool heartbeat_detected = false;
  int16_t deriv = adc_value - prev_adc_value;

  prev_adc_value = adc_value;
  current_time++;

  // insert new sample at the beginning
  for( int i = DERIV_BUF_SIZE - 1; i > 0; i-- ) {
      deriv_buf[i] = deriv_buf[i - 1];
  }
  deriv_buf[0] = deriv;

  // refractory period check
  if( refractory_counter > 0 ) {
    refractory_counter--;
    return false;
  }

  int16_t deriv_sum = 0;
  // sum derivative buffer
  for( int i = 0; i < DERIV_BUF_SIZE; i++ ) {
    deriv_sum += deriv_buf[i];
  }

  if( !rise_detected && !fall_detected && deriv_sum > RISE_THRESHOLD ) {
    rise_detected = true;
    rise_to_fall_period = 0;
  }

  if( rise_detected && !fall_detected && deriv_sum < FALL_THRESHOLD ) {
    fall_detected = true;
  } else if( rise_detected && !fall_detected ) {
    rise_to_fall_period++;

    if( rise_to_fall_period > RISE_FALL_MAX_PERIOD ) {
      rise_detected = false;
      rise_to_fall_period = 0;
    }
  }

  if( rise_detected && fall_detected ) {
    heartbeat_detected = true;
    rise_detected = false;
    fall_detected = false;
    refractory_counter = REFRACTORY_PERIOD;

    // BPM calculation
    if( current_time > 0 ) {
      last_bpm = (uint16_t)( 6000 * FS / current_time );
    }
    current_time = 0;
  }

  return heartbeat_detected;
}

void setup() {
  Serial.begin( 115200 );
  pinMode( BUTTON_BOOT_PIN, INPUT_PULLUP );
  HELPER_init();
  myTimer.once( 120, turnWiFiOffISR );  // turn off WiFi radio after 2 minutes to save power
}

void loop() {
  static unsigned long lastSampleTime = 0;
  unsigned long now = micros();

  // ADC sampling interval check
  if( now - lastSampleTime >= SAMPLE_INTERVAL_US ) {
    lastSampleTime += SAMPLE_INTERVAL_US;  // maintain precise interval

    if( digitalRead(LO_M) == HIGH && !electrodeError ) {
      Serial.println( "Right Arm electrode error!" );
      electrodeError = true;
    } else if( digitalRead(LO_P) == HIGH && !electrodeError ) {
      Serial.println( "Left Arm electrode error!" );
      electrodeError = true;
    } else if( !electrodeError ) {
      // uint16_t adc = analogRead( A0 );
      uint16_t adc = system_adc_read();  // low level ADC read (faster, possible issue when WiFi used at the same time)
      // Serial.println( adc );

      if( process_ecg_sample(adc) ) {
        Serial.print( "BPM = " );
        Serial.println( (float)last_bpm/100, 2 );

        // store sample in buffer
        if( ecg_buffer_index < MAX_SAMPLES ) {
          ecg_buffer[ ecg_buffer_index++ ] = last_bpm;
        } else {      // buffer full, save to file
          unsigned long t0, t1, dt_us;
          static uint8_t saveAttempt = 0;

          saveAttempt++;
          Serial.printf( "ECG buffer full, saving to file (try %u)...\n", saveAttempt );

          t0 = micros();
          bool saved = HELPER_saveToFile( "/ecg_data.bin", (uint8_t*)ecg_buffer, ecg_buffer_index * sizeof(uint16_t) );
          t1 = micros();
          dt_us = t1 - t0;
          uint16_t missed = dt_us / ((1000 / FS) * 1000UL);   // how much samples were missed during saving?

          if( missed > MAX_FILL_SAMPLES ) {
            missed = MAX_FILL_SAMPLES;
          }

          for( uint16_t i = 0; i < missed; ++i ) {
            process_ecg_sample( adc );        // "fill" missed samples with last ADC value
          }

          if( saved ) {
            ecg_buffer_index = 0;
            ecg_buffer[ ecg_buffer_index++ ] = last_bpm; // store current sample
            Serial.printf( "Save took %lu us, missed %u samples (saved on try %u)\n", dt_us, missed, saveAttempt );
            saveAttempt = 0;
          } else {    // else keep buffer full and try to save again later
            ecg_buffer_index = MAX_SAMPLES;
            if( saveAttempt >= 5 ) {
              Serial.println( "Multiple save attempts failed, clearing buffer!" );
              ecg_buffer_index = 0;
              saveAttempt = 0;
            }
          }
        }
      }
    } else {
      // check if error condition cleared
      if( digitalRead(LO_M) == LOW && digitalRead(LO_P) == LOW ) {
        Serial.println( "Electrode error cleared." );
        electrodeError = false;
      }
    }
  }

  handleButton();

  // handle button click action
  if( btnClicked ) {
    Serial.println( "Button click handling..." );
    btnClicked = false;

    if( ecg_buffer_index > 0 ) {
      String fileName = "/data_" + HELPER_getDateTime() + ".txt";
      Serial.printf( "Saving ECG data to file: %s\n", fileName.c_str() );

      bool saved = HELPER_saveToFile( fileName.c_str(), (uint8_t*)ecg_buffer, ecg_buffer_index * sizeof(uint16_t) );
      if( !saved ) {
        Serial.println( "Error saving ECG data file!" );
      } else {
        Serial.println( "ECG data file saved successfully." );
        ecg_buffer_index = 0;
      }
    }
  }

  if( btnLongPress ) {
    Serial.println( "Button long press handling..." );
    btnLongPress = false;
    HELPER_radioOn();
  }

  if( HELPER_isWebServerRunning() ) {
    server.handleClient();
  }

  if( wifiOffFlag ) {
    wifiOffFlag = false;
    HELPER_radioOff();
  }

  yield();  // allow ESP8266 to handle WiFi, watchdog, etc.
}