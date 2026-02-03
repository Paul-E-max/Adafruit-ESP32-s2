#include <Adafruit_AS7341.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LTR390.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_TSL2591.h>
#include <SPI.h>
#include <Wire.h>

// Pins for Adafruit ESP32-S2 TFT Feather
#define TFT_CS 7
#define TFT_DC 39
#define TFT_RST 40
#define TFT_BACKLIGHT 45

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_AS7341 as7341;
Adafruit_LTR390 ltr = Adafruit_LTR390();
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591); // pass in a ID for the sensor

void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);

  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.println("Multi-Sensor Mode");

  // AS7341
  if (!as7341.begin()) {
    Serial.println("{\"error\":\"as7341_not_found\"}");
  } else {
    as7341.setATIME(100);
    as7341.setASTEP(999);
    as7341.setGain(AS7341_GAIN_256X);
  }

  // LTR390
  if (!ltr.begin()) {
    Serial.println("{\"error\":\"ltr390_not_found\"}");
  } else {
    ltr.setMode(LTR390_MODE_UVS);
    ltr.setGain(LTR390_GAIN_18); // Increased gain for better sensitivity
    ltr.setResolution(LTR390_RESOLUTION_18BIT);
  }

  // TSL2591
  if (!tsl.begin()) {
    Serial.println("{\"error\":\"tsl2591_not_found\"}");
  } else {
    tsl.setGain(TSL2591_GAIN_MED);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
  }

  Serial.println("{\"status\":\"initialized\"}");
}

void loop() {
  // Read AS7341
  as7341.enableSpectralMeasurement(true);
  bool asOk = as7341.readAllChannels();

  // Read LTR390
  ltr.setMode(LTR390_MODE_UVS);
  uint32_t uv = 0;
  if (ltr.newDataAvailable())
    uv = ltr.readUVS();

  ltr.setMode(LTR390_MODE_ALS);
  uint32_t als = 0;
  if (ltr.newDataAvailable())
    als = ltr.readALS();

  // Read TSL2591
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir = lum >> 16;
  uint16_t full = lum & 0xFFFF;
  float lux = tsl.calculateLux(full, ir);
  if (isnan(lux))
    lux = 0.0; // Fix invalid JSON NaN

  // JSON Output
  Serial.print("{");
  if (asOk) {
    Serial.print("\"F1\":");
    Serial.print(as7341.getChannel(AS7341_CHANNEL_415nm_F1));
    Serial.print(",\"F2\":");
    Serial.print(as7341.getChannel(AS7341_CHANNEL_445nm_F2));
    Serial.print(",\"F3\":");
    Serial.print(as7341.getChannel(AS7341_CHANNEL_480nm_F3));
    Serial.print(",\"F4\":");
    Serial.print(as7341.getChannel(AS7341_CHANNEL_515nm_F4));
    Serial.print(",\"F5\":");
    Serial.print(as7341.getChannel(AS7341_CHANNEL_555nm_F5));
    Serial.print(",\"F6\":");
    Serial.print(as7341.getChannel(AS7341_CHANNEL_590nm_F6));
    Serial.print(",\"F7\":");
    Serial.print(as7341.getChannel(AS7341_CHANNEL_630nm_F7));
    Serial.print(",\"F8\":");
    Serial.print(as7341.getChannel(AS7341_CHANNEL_680nm_F8));
    Serial.print(",");
  }
  Serial.print("\"UV\":");
  Serial.print(uv);
  Serial.print(",\"ALS\":");
  Serial.print(als);
  Serial.print(",\"TSL_Lux\":");
  Serial.print(lux);
  Serial.print(",\"TSL_IR\":");
  Serial.print(ir);
  Serial.print(",\"TSL_Full\":");
  Serial.print(full);
  Serial.println("}");

  // TFT Update
  tft.setCursor(0, 0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);
  tft.println("Multi-Sensor Data Stream");
  tft.setTextColor(ST77XX_WHITE);
  tft.print("AS7341: ");
  tft.println(asOk ? "OK" : "ERR");
  tft.print("UV (LTR): ");
  tft.println(uv);
  tft.print("Lux (TSL): ");
  tft.println(lux);

  digitalWrite(13, HIGH);
  delay(50);
  digitalWrite(13, LOW);
  delay(200);
}
