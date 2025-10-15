#include <Arduino.h>
extern "C" {
  #include "user_interface.h"
}

#define LO_M D0         // Right arm electrode
#define LO_P D1         // Left arm electrode

#define FS 200
#define RISE_THRESHOLD            300     // próg wzrostu (w jednostkach '3 sample sum')
#define FALL_THRESHOLD            -500    // próg spadku
#define REFRACTORY_PERIOD         (FS/5)  // 200ms okres refrakcji
#define RISE_FALL_MAX_PERIOD      8       // maksymalny okres (w próbkach) pomiędzy wzrostem i opadnięciem

static unsigned long current_time = 0;
static float last_bpm = 0.0f;
static uint8_t rise_to_fall_period = 0;
static uint8_t refractory_counter = 0;

/**
 * Call this function FS times per second (e.g. in loop with delay(1000/FS))
 * with adc_value being the latest ADC sample from ECG input.
 * It returns true if a heartbeat was detected.
 */
bool process_ecg_sample( uint16_t adc_value ) {
  static int16_t deriv_buf[3] = {0};    // wielkość bufora jest uzależniona od FS (powinna wynosić czasowo ~15ms, @FS=200 => 3 próbki, @FS=400 => 6 próbek)
  static uint16_t prev_adc_value;
  static bool rise_detected = false;
  static bool fall_detected = false;
  bool heartbeat_detected = false;
  int16_t deriv = adc_value - prev_adc_value;

  prev_adc_value = adc_value;
  current_time++;

  deriv_buf[2] = deriv_buf[1];
  deriv_buf[1] = deriv_buf[0];
  deriv_buf[0] = deriv;

  if( refractory_counter > 0 ) {
    refractory_counter--;
    return false;
  }

  int16_t deriv_sum = deriv_buf[0] + deriv_buf[1] + deriv_buf[2];

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

    // Oblicz BPM
    if( current_time > 0 ) {
      last_bpm = 60.0f * FS / (float)current_time;
    }
    current_time = 0;
  }

  return heartbeat_detected;
}

void setup() {
  Serial.begin( 115200 );
}

void loop() {
  if( digitalRead(LO_M) == HIGH ) {
    Serial.println( "Right Arm electrode error!" );
  // } else if( digitalRead(LO_P) == HIGH ) {
  //   Serial.println( "Left Arm electrode error!" );
  } else {
    // uint16_t adc = analogRead( A0 );
    uint16_t adc = system_adc_read();  // niskopoziomowy odczyt ADC (szybszy, może zgrzytać przy używaniu WiFi)
    // Serial.println( adc );

    if( process_ecg_sample(adc) ) {
      Serial.print( "BPM = " );
      Serial.println( last_bpm );
    }
  }
  delay( 1000/FS ); // 200 Hz
}