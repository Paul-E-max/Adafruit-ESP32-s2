/*
 * Spectral Sensor Firmware - ROBUST VERSION
 * Version: 2.0.1
 * Date: 2026-02-03
 *
 * Changelog:
 * v2.0.1 - Made sensor init non-blocking to prevent watchdog resets
 *        - Added I2C scanner to debug sensor connections
 *        - Disabled task watchdog to prevent resets
 * v2.0.0 - Added auto-gain algorithm to prevent sensor saturation
 *        - Added firmware version to JSON output
 * v1.0.0 - Initial multi-sensor integration (AS7341, LTR390, TSL2591)
 */

#include <Adafruit_AS7341.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LTR390.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_TSL2591.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_task_wdt.h>

// Firmware Version
#define FW_VERSION "2.0.1"

// Pins for Adafruit ESP32-S2 TFT Feather
#define TFT_CS 7
#define TFT_DC 39
#define TFT_RST 40
#define TFT_BACKLIGHT 45

// Auto-gain thresholds
#define SATURATION_THRESHOLD 60000 // If any channel exceeds this, reduce gain
#define LOW_SIGNAL_THRESHOLD 1000  // If all channels below this, increase gain

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_AS7341 as7341;
Adafruit_LTR390 ltr = Adafruit_LTR390();
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

// Sensor status flags
bool as7341_ok = false;
bool ltr390_ok = false;
bool tsl2591_ok = false;

// Available gain levels (ordered from lowest to highest)
as7341_gain_t gainLevels[] = {
    AS7341_GAIN_0_5X, AS7341_GAIN_1X,   AS7341_GAIN_2X,  AS7341_GAIN_4X,
    AS7341_GAIN_8X,   AS7341_GAIN_16X,  AS7341_GAIN_32X, AS7341_GAIN_64X,
    AS7341_GAIN_128X, AS7341_GAIN_256X, AS7341_GAIN_512X};
const int NUM_GAINS = 11;
int currentGainIndex = 5; // Start at 16X (middle ground)

// Gain multiplier names for display
const char *gainNames[] = {"0.5x", "1x",  "2x",   "4x",   "8x",  "16x",
                           "32x",  "64x", "128x", "256x", "512x"};

// I2C Scanner to help debug sensor connections
void scanI2C() {
  Serial.println("{\"i2c_scan\":\"starting\"}");
  byte count = 0;
  for (byte i = 1; i < 127; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("{\"i2c_device\":\"0x");
      Serial.print(i, HEX);
      Serial.println("\"}");
      count++;
      delay(1);
    }
  }
  Serial.print("{\"i2c_devices_found\":");
  Serial.print(count);
  Serial.println("}");
}

void setup() {
  // DISABLE WATCHDOG TIMER to prevent resets
  // Use disableCore0WDT() for ESP32-S2 (single core)
  disableCore0WDT();

  Serial.begin(115200);
  delay(500); // Longer initial delay

  Serial.println("{\"status\":\"boot\",\"fw\":\"" FW_VERSION "\"}");

  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH); // Turn on LED during init

  // Initialize I2C
  Wire.begin();
  Wire.setClock(100000); // Slow down I2C to 100kHz for reliability
  delay(200);

  Serial.println("{\"status\":\"i2c_initialized\"}");

  // Scan I2C bus
  scanI2C();

  // TFT Init (non-blocking)
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  Serial.println("{\"status\":\"init_tft\"}");
  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.print("FW v");
  tft.println(FW_VERSION);
  delay(100);

  // AS7341 - Non-blocking
  Serial.println("{\"status\":\"init_as7341\"}");
  tft.println("Init AS7341...");
  as7341_ok = as7341.begin();
  if (!as7341_ok) {
    Serial.println("{\"error\":\"as7341_not_found\"}");
    tft.println("AS7341: FAIL");
  } else {
    as7341.setATIME(100);
    as7341.setASTEP(999);
    as7341.setGain(gainLevels[currentGainIndex]);
    Serial.println("{\"status\":\"as7341_ok\"}");
    tft.println("AS7341: OK");
  }
  delay(200);

  // LTR390 - Non-blocking
  Serial.println("{\"status\":\"init_ltr390\"}");
  tft.println("Init LTR390...");
  ltr390_ok = ltr.begin();
  if (!ltr390_ok) {
    Serial.println("{\"error\":\"ltr390_not_found\"}");
    tft.println("LTR390: FAIL");
  } else {
    ltr.setMode(LTR390_MODE_UVS); // Stay in UV mode
    ltr.setGain(LTR390_GAIN_18);  // High sensitivity
    ltr.setResolution(
        LTR390_RESOLUTION_16BIT); // 16-bit: ~100ms measurement time
    ltr.setThresholds(0, 0);      // Disable interrupt thresholds
    Serial.println("{\"status\":\"ltr390_ok\"}");
    tft.println("LTR390: OK (UV mode)");
  }
  delay(200);

  // TSL2591 - Non-blocking
  Serial.println("{\"status\":\"init_tsl2591\"}");
  tft.println("Init TSL2591...");
  tsl2591_ok = tsl.begin();
  if (!tsl2591_ok) {
    Serial.println("{\"error\":\"tsl2591_not_found\"}");
    tft.println("TSL2591: FAIL");
  } else {
    tsl.setGain(TSL2591_GAIN_MED);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);
    Serial.println("{\"status\":\"tsl2591_ok\"}");
    tft.println("TSL2591: OK");
  }
  delay(200);

  Serial.print("{\"status\":\"ready\",\"as7341\":");
  Serial.print(as7341_ok ? "true" : "false");
  Serial.print(",\"ltr390\":");
  Serial.print(ltr390_ok ? "true" : "false");
  Serial.print(",\"tsl2591\":");
  Serial.print(tsl2591_ok ? "true" : "false");
  Serial.println("}");

  tft.println("Ready!");
  digitalWrite(13, LOW);
  delay(1000);
}

// Auto-gain adjustment function
bool adjustGainIfNeeded(uint16_t channels[8]) {
  uint16_t maxVal = 0;
  uint16_t minVal = 65535;

  for (int i = 0; i < 8; i++) {
    if (channels[i] > maxVal)
      maxVal = channels[i];
    if (channels[i] < minVal)
      minVal = channels[i];
  }

  // Check for saturation - need to decrease gain
  if (maxVal >= SATURATION_THRESHOLD && currentGainIndex > 0) {
    currentGainIndex--;
    as7341.setGain(gainLevels[currentGainIndex]);
    return true; // Gain changed, re-read needed
  }

  // Check for low signal - need to increase gain
  if (maxVal < LOW_SIGNAL_THRESHOLD && currentGainIndex < NUM_GAINS - 1) {
    currentGainIndex++;
    as7341.setGain(gainLevels[currentGainIndex]);
    return true; // Gain changed, re-read needed
  }

  return false; // No change needed
}

void loop() {
  // Read AS7341 with auto-gain (only if sensor is available)
  uint16_t channels[8] = {0};
  bool asOk = false;

  if (as7341_ok) {
    int attempts = 0;
    const int MAX_ATTEMPTS = 5;

    as7341.enableSpectralMeasurement(true);

    do {
      asOk = as7341.readAllChannels();
      if (asOk) {
        channels[0] = as7341.getChannel(AS7341_CHANNEL_415nm_F1);
        channels[1] = as7341.getChannel(AS7341_CHANNEL_445nm_F2);
        channels[2] = as7341.getChannel(AS7341_CHANNEL_480nm_F3);
        channels[3] = as7341.getChannel(AS7341_CHANNEL_515nm_F4);
        channels[4] = as7341.getChannel(AS7341_CHANNEL_555nm_F5);
        channels[5] = as7341.getChannel(AS7341_CHANNEL_590nm_F6);
        channels[6] = as7341.getChannel(AS7341_CHANNEL_630nm_F7);
        channels[7] = as7341.getChannel(AS7341_CHANNEL_680nm_F8);
      }
      attempts++;
    } while (asOk && adjustGainIfNeeded(channels) && attempts < MAX_ATTEMPTS);
  }

  // Read LTR390 UV (only if sensor is available)
  // Note: Sensor stays in UV mode for consistent readings
  // ALS reading skipped to prioritize UV responsiveness
  uint32_t uv = 0;
  uint32_t als = 0; // Not read - sensor in UV mode only

  if (ltr390_ok) {
    // Simply read UV data - sensor is constantly measuring in UV mode
    uv = ltr.readUVS();
  }

  // Read TSL2591 (only if sensor is available)
  uint16_t ir = 0;
  uint16_t full = 0;
  float lux = 0.0;

  if (tsl2591_ok) {
    uint32_t lum = tsl.getFullLuminosity();
    ir = lum >> 16;
    full = lum & 0xFFFF;
    lux = tsl.calculateLux(full, ir);
    if (isnan(lux))
      lux = 0.0;
  }

  // JSON Output
  Serial.print("{");
  Serial.print("\"fw\":\"");
  Serial.print(FW_VERSION);
  Serial.print("\",\"gain\":\"");
  Serial.print(gainNames[currentGainIndex]);
  Serial.print("\",");

  if (asOk) {
    Serial.print("\"F1\":");
    Serial.print(channels[0]);
    Serial.print(",\"F2\":");
    Serial.print(channels[1]);
    Serial.print(",\"F3\":");
    Serial.print(channels[2]);
    Serial.print(",\"F4\":");
    Serial.print(channels[3]);
    Serial.print(",\"F5\":");
    Serial.print(channels[4]);
    Serial.print(",\"F6\":");
    Serial.print(channels[5]);
    Serial.print(",\"F7\":");
    Serial.print(channels[6]);
    Serial.print(",\"F8\":");
    Serial.print(channels[7]);
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
  tft.print("Spectral FW v");
  tft.println(FW_VERSION);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("AS7341: ");
  tft.print(asOk ? "OK" : "ERR");
  tft.print(" Gain: ");
  tft.println(gainNames[currentGainIndex]);
  tft.print("UV (LTR): ");
  tft.println(uv);
  tft.print("Lux (TSL): ");
  tft.println(lux);

  digitalWrite(13, HIGH);
  delay(50);
  digitalWrite(13, LOW);
  delay(200);
}
