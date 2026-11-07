/*
  LoRa Transmitter: Raspberry Pi Pico 2 + RA-02 (SX1278) + BME280
  Board core : "Raspberry Pi Pico/RP2040/RP2350" by Earle Philhower
  Board      : Raspberry Pi Pico 2
  Libraries  : "LoRa" by Sandeep Mistry
               "Adafruit BME280 Library" (+ Adafruit Unified Sensor)

  WIRING (RA-02 is 3.3V ONLY - never connect to 5V)
  RA-02      Pico 2
  3.3V   ->  3V3 (pin 36)
  GND    ->  GND
  SCK    ->  GP18
  MISO   ->  GP16
  MOSI   ->  GP19
  NSS    ->  GP17
  RST    ->  GP20
  DIO0   ->  GP21
  (attach the antenna before powering on!)

  BME280     Pico 2
  VIN    ->  3V3
  GND    ->  GND
  SDA    ->  GP4
  SCL    ->  GP5
*/

#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// LoRa pins
#define LORA_SCK   18
#define LORA_MISO  16
#define LORA_MOSI  19
#define LORA_CS    17
#define LORA_RST   20
#define LORA_DIO0  21

// BME280 I2C pins
#define I2C_SDA 4
#define I2C_SCL 5

#define LORA_FREQ 433E6          // RA-02 SX1278: 433 MHz (must match receiver)
#define SEND_INTERVAL 5000       // ms

Adafruit_BME280 bme;
unsigned long counter = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("LoRa Transmitter + BME280");

  // ---- BME280 ----
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();

  if (!bme.begin(0x76) && !bme.begin(0x77)) {
    Serial.println("BME280 not found! Check wiring.");
    while (true) delay(1000);
  }

  // ---- LoRa ----
  SPI.setRX(LORA_MISO);
  SPI.setTX(LORA_MOSI);
  SPI.setSCK(LORA_SCK);
  LoRa.setSPI(SPI);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa init failed! Check wiring.");
    while (true) delay(1000);
  }

  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.setTxPower(17);

  Serial.println("Setup OK");
}

void loop() {
  float temperature = bme.readTemperature();        // C
  float humidity    = bme.readHumidity();           // %
  float pressure    = bme.readPressure() / 100.0F;  // hPa

  if (isnan(temperature) || isnan(humidity) || isnan(pressure)) {
    Serial.println("Sensor read error");
    delay(2000);
    return;
  }

  // Packet format: counter,temperature,humidity,pressure
  String packet = String(counter) + "," + String(temperature, 2) + "," +
                  String(humidity, 2) + "," + String(pressure, 2);

  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();

  Serial.print("Sent: ");
  Serial.println(packet);

  counter++;
  delay(SEND_INTERVAL);
}
