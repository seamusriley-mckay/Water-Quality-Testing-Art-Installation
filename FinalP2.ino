#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ==================================================
// SENSOR PINS
// ==================================================
#define PH_PIN A3
#define TURBIDITY_PIN A2
#define TDS_PIN A0
#define TEMP_PIN 2  // DS18B20 1-Wire pin

#define VREF 5.0

// ==================================================
// STABLE ADC READ
// ==================================================
int readStable(int pin) {
  analogRead(pin);
  delayMicroseconds(50);
  int sum = 0;
  for (int i = 0; i < 10; i++) sum += analogRead(pin);
  return sum / 10;
}

// ==================================================
// RGB LED PINS
// ==================================================

// Turbidity RGB (COMMON CATHODE)
int tRed   = 5;
int tGreen = 3;
int tBlue  = 4;

// pH RGB (COMMON CATHODE)
int pRed   = 8;
int pGreen = 10;
int pBlue  = 9;

// TDS RGB (COMMON ANODE)
int cRed   = 13;
int cGreen = 11;
int cBlue  = 12;

// ==================================================
// NEOPIXEL STRIP
// ==================================================
#define NEO_PIN 6
#define NUM_LEDS 60
Adafruit_NeoPixel strip(NUM_LEDS, NEO_PIN, NEO_GRB + NEO_KHZ800);
uint8_t brightness = 100; // 0-255

uint32_t currentColor[NUM_LEDS]; // Store current LED colors for smooth transition

// ==================================================
// DS18B20 SETUP
// ==================================================
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// ==================================================
// WAVE PARAMETERS
// ==================================================
float waveSpeed = 0.05;
float waveLength = 2.0;
float t = 0; // time variable

// ==================================================
// UTILITY FUNCTIONS
// ==================================================
uint32_t lerpColor(uint32_t c1, uint32_t c2, float amt) {
  uint8_t r = ((c2>>16)&0xFF - (c1>>16)&0xFF) * amt + ((c1>>16)&0xFF);
  uint8_t g = ((c2>>8)&0xFF - (c1>>8)&0xFF) * amt + ((c1>>8)&0xFF);
  uint8_t b = ((c2&0xFF) - (c1&0xFF)) * amt + (c1&0xFF);
  return strip.Color(r,g,b);
}

uint32_t applyWave(uint32_t color, float wave) {
  uint8_t r = ((color >> 16) & 0xFF) * wave * brightness / 255.0;
  uint8_t g = ((color >> 8) & 0xFF) * wave * brightness / 255.0;
  uint8_t b = (color & 0xFF) * wave * brightness / 255.0;
  return strip.Color(r,g,b);
}

// ==================================================
// SETUP
// ==================================================
void setup() {
  Serial.begin(9600);

  pinMode(tRed, OUTPUT); pinMode(tGreen, OUTPUT); pinMode(tBlue, OUTPUT);
  pinMode(pRed, OUTPUT); pinMode(pGreen, OUTPUT); pinMode(pBlue, OUTPUT);
  pinMode(cRed, OUTPUT); pinMode(cGreen, OUTPUT); pinMode(cBlue, OUTPUT);

  strip.begin();
  strip.show();

  sensors.begin();

  // Initialize currentColor array
  for(int i=0; i<NUM_LEDS; i++) currentColor[i] = strip.Color(0,0,0);

  Serial.println("Sensor-Friendly Wave System Started...");
}

// ==================================================
// LOOP
// ==================================================
void loop() {
  //--------- Read Sensors ----------
  int rawPH   = readStable(PH_PIN);
  int rawTurb = readStable(TURBIDITY_PIN);
  int rawTDS  = readStable(TDS_PIN);

  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  //--------- Convert Voltages ----------
  float phVolt   = rawPH   * (VREF / 1024.0);
  float turbVolt = rawTurb * (VREF / 1024.0);
  float tdsVolt  = rawTDS  * (VREF / 1024.0);

  //--------- Sensor Calculations ----------
  float pHValue = 2.157 * phVolt + 2.814;
  float tdsPPM  = tdsVolt * 500;

  // ====================================================
  //  TURBIDITY RGB (COMMON CATHODE)
  // ====================================================
  if (turbVolt > 3.5) {
    analogWrite(tRed, 255); analogWrite(tGreen, 90); analogWrite(tBlue, 0); 
  } else if (turbVolt > 1.5) {
    analogWrite(tRed, 255); analogWrite(tGreen,25 ); analogWrite(tBlue, 0);
  } else {
    analogWrite(tRed, 255); analogWrite(tGreen, 0); analogWrite(tBlue, 0);
  }

  // ====================================================
  //  pH RGB (COMMON CATHODE)
  // ====================================================
  if (pHValue < 5) {
    analogWrite(pRed, 255); analogWrite(pGreen, 0); analogWrite(pBlue, 60);
  } else if (pHValue <= 7.5) {
    analogWrite(pRed, 255); analogWrite(pGreen, 0); analogWrite(pBlue, 255);
  } else {
    analogWrite(pRed, 0); analogWrite(pGreen, 0); analogWrite(pBlue, 255);
  }

  // ====================================================
  //  TDS RGB (COMMON ANODE)
  // ====================================================
  if (tdsPPM < 100) {
    analogWrite(cRed, 255); analogWrite(cGreen, 175); analogWrite(cBlue, 0);
  } else if (tdsPPM < 350) {
    analogWrite(cRed, 255); analogWrite(cGreen, 0); analogWrite(cBlue, 0);
  } else {
    analogWrite(cRed, 255); analogWrite(cGreen, 0); analogWrite(cBlue, 255);
        
   
  }

  // ====================================================
  //  TEMPERATURE → TARGET COLOR FOR NEOPIXEL
  // ====================================================
  uint32_t targetColor;

  if (temperatureC < 20) targetColor = strip.Color(0,100,255);
  else if (temperatureC < 28) targetColor = strip.Color(255,180,0);
  else if (temperatureC < 35) targetColor = strip.Color(255,50,0);
  else targetColor = strip.Color(255,0,0);

  // ====================================================
  //  PROGRESSIVE LED TRANSITION + WAVE
  // ====================================================
  for(int i=0; i<NUM_LEDS; i++){
    // Gradually move current color toward target color
    currentColor[i] = lerpColor(currentColor[i], targetColor, 0.05); // smooth step

    // Apply wave brightness
    float wave = (sin((float)i/NUM_LEDS * waveLength * 2 * PI + t) + 1.0)/2.0;
    strip.setPixelColor(i, applyWave(currentColor[i], wave));
  }

  strip.show();
  t += waveSpeed;

  // ====================================================
  //  SERIAL OUTPUT
  // ====================================================
  Serial.print("Temp: "); Serial.print(temperatureC); Serial.print(" C | ");
  Serial.print("pH: "); Serial.print(pHValue); Serial.print(" | ");
  Serial.print("Turb(V): "); Serial.print(turbVolt); Serial.print(" | ");
  Serial.print("TDS(V): "); Serial.print(tdsVolt); Serial.print(" | PPM: ");
  Serial.println(tdsPPM);

  delay(30); // smooth animation
}
