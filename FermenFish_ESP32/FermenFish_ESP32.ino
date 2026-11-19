#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ============================================================================
// 1. DEFINISI PIN ESP32 DEV MODULE
// ============================================================================
// Pin OLED 7-Pin SPI (Hardware VSPI: D0/SCK = 18, D1/MOSI = 23)
#define OLED_MOSI   23  // Pin D1 pada OLED
#define OLED_CLK    18  // Pin D0 pada OLED
#define OLED_DC     17  // Pin DC pada OLED (TX2)
#define OLED_CS     5   // Pin CS pada OLED
#define OLED_RESET  16  // Pin RES pada OLED (RX2)

// Pin Sensor & Relay
#define PIN_DS18B20    13  // Pin DATA DS18B20 (+ resistor 4.7k ke 3V3)
#define PIN_PH_SENSOR  34  // Pin Analog Po Sensor pH
#define RELAY_EXHAUST  21  // Relay Kipas Exhaust (ON saat suhu > 30.0°C)
#define RELAY_HEATER   22  // Relay Kipas Heater  (ON saat suhu < 20.0°C)
#define RELAY_KIPAS    25  // Relay Kipas Sirkulasi

// Logika Modul Relay (Active-LOW untuk modul relay biru standar)
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

const float SUHU_MAX_EXHAUST = 30.0;
const float SUHU_MIN_HEATER  = 20.0;
const float HISTERESIS       = 0.3;

// ============================================================================
// 2. INISIALISASI OBJEK OLED & SENSOR
// ============================================================================
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);

float suhu         = 0.0;
float ph           = 7.00;
float phVoltage    = 2.50;
bool  sensorSuhuOK = false;
float phOffset     = 0.00; // Koreksi kalibrasi pH

bool autoMode  = true;
bool exhaustOn = false;
bool heaterOn  = false;

unsigned long lastUpdate = 0;

// ============================================================================
// 3. LOGIKA KONTROL RELAY OTOMATIS
// ============================================================================
void updateRelayPins() {
  digitalWrite(RELAY_EXHAUST, exhaustOn ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY_HEATER,  heaterOn  ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY_KIPAS,   (exhaustOn || heaterOn) ? RELAY_ON : RELAY_OFF);
}

void runAutoControl() {
  if (!autoMode) return;

  if (!sensorSuhuOK) {
    exhaustOn = false;
    heaterOn  = false;
    updateRelayPins();
    return;
  }

  if (suhu > SUHU_MAX_EXHAUST) {
    exhaustOn = true;
  } else if (suhu < (SUHU_MAX_EXHAUST - HISTERESIS)) {
    exhaustOn = false;
  }

  if (suhu < SUHU_MIN_HEATER) {
    heaterOn = true;
  } else if (suhu > (SUHU_MIN_HEATER + HISTERESIS)) {
    heaterOn = false;
  }

  updateRelayPins();
}

// ============================================================================
// 4. BACA SENSOR & GAMBAR TAMPILAN OLED 128x64
// ============================================================================
void readSensors() {
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);
  if (t != DEVICE_DISCONNECTED_C && t > -50.0 && t < 125.0) {
    suhu = t;
    sensorSuhuOK = true;
  } else {
    sensorSuhuOK = false;
  }

  long sumADC = 0;
  for (int i = 0; i < 20; i++) {
    sumADC += analogRead(PIN_PH_SENSOR);
    delay(2);
  }
  float avgADC = sumADC / 20.0;
  phVoltage = avgADC * (3.3 / 4095.0);
  ph = constrain(7.0 + ((2.50 - phVoltage) / 0.18) + phOffset, 0.0, 14.0);
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("MONITORING AIR");
  display.setCursor(98, 0);
  display.print(autoMode ? "[AUTO]" : "[MAN]");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // Baris Suhu
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.print("SUHU:");
  display.setTextSize(2);
  display.setCursor(36, 13);
  if (sensorSuhuOK) {
    display.printf("%.1f C", suhu);
  } else {
    display.print("ERR D13");
  }

  // Baris pH
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("pH  :");
  display.setTextSize(2);
  display.setCursor(36, 33);
  display.printf("%.2f", ph);

  display.setTextSize(1);
  display.setCursor(92, 39);
  display.printf("%.2fV", phVoltage);

  // Garis Pemisah & Status Relay di Bagian Bawah
  display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.printf("EXH:%-3s", exhaustOn ? "ON" : "OFF");
  display.setCursor(68, 56);
  display.printf("HTR:%-3s", heaterOn ? "ON" : "OFF");

  display.display();
}

// ============================================================================
// 5. SETUP & LOOP UTAMA
// ============================================================================
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(RELAY_EXHAUST, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_KIPAS, OUTPUT);
  updateRelayPins();

  pinMode(PIN_DS18B20, INPUT_PULLUP);
  sensors.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("OLED gagal diinisialisasi!");
    for (;;);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(18, 22);
  display.println("SISTEM MONITORING");
  display.setCursor(28, 34);
  display.println("SUHU & KADAR pH");
  display.display();
  delay(1000);
}

void loop() {
  // Uji relay manual lewat Serial Monitor (ketik A, E, atau H lalu Enter)
  if (Serial.available()) {
    char cmd = toupper(Serial.read());
    if (cmd == 'A') {
      autoMode = true;
    } else if (cmd == 'E') {
      autoMode = false;
      exhaustOn = !exhaustOn;
      updateRelayPins();
      updateDisplay();
    } else if (cmd == 'H') {
      autoMode = false;
      heaterOn = !heaterOn;
      updateRelayPins();
      updateDisplay();
    }
  }

  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    readSensors();
    runAutoControl();
    updateDisplay();

    Serial.printf("Suhu: %.2f C (%s) | pH: %.2f (%.2f V) | EXH: %s | HTR: %s\n",
                  suhu, sensorSuhuOK ? "OK" : "ERR", ph, phVoltage,
                  exhaustOn ? "ON" : "OFF", heaterOn ? "ON" : "OFF");
  }
}
