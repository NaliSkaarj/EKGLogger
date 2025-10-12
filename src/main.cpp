#include <Arduino.h>
extern "C" {
  #include "user_interface.h"
}

#define LO_M D0         // Right arm
#define LO_P D1         // Left arm

#define FS 200                         // częstotliwość próbkowania [Hz]
#define WINDOW_SIZE 30                 // okno integracji (~150 ms)
#define REFRACTORY_PERIOD (FS * 0.25)  // 250 ms - przerwa po wykryciu
#define ALPHA 0.1f                     // współczynnik filtru IIR (0–1)

// ================================
// Bufory i zmienne globalne
// ================================
static float diff_buf[5] = {0};
static float int_buf[WINDOW_SIZE] = {0};
static int int_index = 0;
static int refractory_counter = 0;

static float threshold = 0.0f;
static float signal_level = 0.0f;
static float noise_level = 0.0f;

static unsigned long current_time = 0;
static unsigned long last_peak_time = 0;
static float last_bpm = 0.0f;

static float filtered_value = 0.0f;  // wyjście z filtru IIR
static float baseline = 512.0f;      // wartość spoczynkowa ADC (po kalibracji)

/**
 * Filtr dolnoprzepustowy IIR
**/
static inline float lowpass_filter( float input ) {
  filtered_value += ALPHA * ( input - filtered_value );
  return filtered_value;
}

/**
* Normalizacja (odejmuje tło sygnału)
**/
static inline float normalize( float sample ) {
  // można dodać wolne uaktualnianie baseline
  baseline = 0.999f * baseline + 0.001f * sample;
  return sample - baseline;
}

/**
* Główna funkcja przetwarzająca próbkę
**/
bool process_ecg_sample( uint16_t adc_value ) {
  current_time++;

  // 1️⃣ Filtr dolnoprzepustowy i normalizacja
  float sample = lowpass_filter( (float)adc_value );
  sample = normalize( sample );

  // 2️⃣ Pochodna (filtr różnicowy 5-punktowy)
  diff_buf[4] = diff_buf[3];
  diff_buf[3] = diff_buf[2];
  diff_buf[2] = diff_buf[1];
  diff_buf[1] = diff_buf[0];
  diff_buf[0] = sample;

  float deriv = ( 2 * diff_buf[0] + diff_buf[1] - diff_buf[3] - 2 * diff_buf[4] ) / 8.0f;

  // 3️⃣ Potęgowanie
  float squared = deriv * deriv;

  // 4️⃣ Całkowanie ruchome (okno przesuwne)
  int_buf[int_index] = squared;
  int_index = (int_index + 1) % WINDOW_SIZE;

  float sum = 0.0f;
  for( int i = 0; i < WINDOW_SIZE; i++ ) {
    sum += int_buf[i];
  }
  float integrated = sum / WINDOW_SIZE;

  // 5️⃣ Adaptacyjny próg i wykrywanie
  bool detected = false;
  if( refractory_counter > 0 ) {
    refractory_counter--;
  }

  if( integrated > threshold && refractory_counter == 0 ) {
    detected = true;
    refractory_counter = REFRACTORY_PERIOD;
    signal_level = 0.125f * integrated + 0.875f * signal_level;
  }
  else {
    noise_level = 0.125f * integrated + 0.875f * noise_level;
  }

  threshold = noise_level + 0.25f * (signal_level - noise_level);

  // 6️⃣ Oblicz BPM
  if( detected ) {
    unsigned long dt = current_time - last_peak_time;
    if( dt > 0 ) {
      last_bpm = 60.0f * FS / (float)dt;
    }
    last_peak_time = current_time;
  }

  return detected;
}

void setup() {
  Serial.begin( 115200 );
}

void loop() {
  if( digitalRead(LO_M) == HIGH ) {
    Serial.println( "Right Arm electrode error!" );
  } else if( digitalRead(LO_P) == HIGH ) {
    Serial.println( "Left Arm electrode error!" );
  } else {
    uint16_t adc = analogRead( A0 );
    // uint16_t adc = system_adc_read();  // niskopoziomowy odczyt ADC
    // Serial.println( analogRead(adc) );

    if( process_ecg_sample(adc) ) {
      Serial.print( "BPM = " );
      Serial.println( last_bpm );
    }
  }
  delay( 5 ); // 200 Hz
}