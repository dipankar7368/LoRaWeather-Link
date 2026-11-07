/*
  LoRa Weather Receiver with smooth animated OLED UI
  Raspberry Pi Pico 2 + RA-02 (SX1278) + 0.96" SSD1306 OLED (128x64, I2C)

  Features
   - Values glide smoothly to the new reading (easing animation, ~25 FPS)
   - Icons for temperature, humidity and pressure
   - Animated progress bars
   - Signal-strength bars + blinking dot when a packet arrives
   - Startup splash screen with loading bar
   - "No signal" warning if the transmitter stops

  Board core : "Raspberry Pi Pico/RP2040/RP2350" by Earle Philhower
  Board      : Raspberry Pi Pico 2
  Libraries  : LoRa (Sandeep Mistry), Adafruit SSD1306, Adafruit GFX

  WIRING (RA-02 is 3.3V ONLY)
  RA-02: 3.3V->3V3, GND->GND, SCK->GP18, MISO->GP16, MOSI->GP19,
         NSS->GP17, RST->GP20, DIO0->GP21
  OLED : VCC->3V3, GND->GND, SDA->GP4, SCL->GP5
*/

#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Pins ----------
#define LORA_SCK   18
#define LORA_MISO  16
#define LORA_MOSI  19
#define LORA_CS    17
#define LORA_RST   20
#define LORA_DIO0  21
#define LORA_FREQ  433E6        // must match transmitter

#define I2C_SDA 4
#define I2C_SCL 5
#define OLED_ADDR 0x3C          // try 0x3D if blank

// ---------- Settings ----------
#define FRAME_MS        40      // 25 frames per second
#define EASE            0.18f   // 0..1, higher = faster glide
#define SIGNAL_TIMEOUT  30000   // ms without packet -> "No signal"

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Latest received values (targets)
float tgtT = 0, tgtH = 0, tgtP = 1000;
// Values currently shown (animated)
float dispT = 0, dispH = 0, dispP = 1000;
float barT = 0, barH = 0, barP = 0;     // animated bar fill 0..1

String count = "-";
int rssi = -150;
bool gotData = false;
unsigned long lastPacket = 0;
unsigned long lastFrame = 0;

// ---------- Helpers ----------
float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
float mapf(float v, float a, float b) { return clamp01((v - a) / (b - a)); }

int rssiToBars(int r) {
  if (r > -70)  return 4;
  if (r > -85)  return 3;
  if (r > -100) return 2;
  if (r > -115) return 1;
  return 0;
}

// ---------- Icons (drawn with shapes, ~11x14 px) ----------
void iconThermo(int x, int y) {
  display.drawRoundRect(x + 3, y, 5, 10, 2, SSD1306_WHITE);
  display.fillCircle(x + 5, y + 11, 3, SSD1306_WHITE);
  display.fillRect(x + 5, y + 4, 1, 6, SSD1306_WHITE);
}
void iconDrop(int x, int y) {
  display.fillTriangle(x + 5, y, x, y + 8, x + 10, y + 8, SSD1306_WHITE);
  display.fillCircle(x + 5, y + 9, 5, SSD1306_WHITE);
}
void iconGauge(int x, int y) {
  display.drawCircle(x + 5, y + 7, 6, SSD1306_WHITE);
  display.drawLine(x + 5, y + 7, x + 8, y + 4, SSD1306_WHITE);
  display.drawPixel(x + 5, y + 7, SSD1306_WHITE);
}

// ---------- Drawing blocks ----------
void drawSignal(int x, int y, int bars, bool alive) {
  for (int i = 0; i < 4; i++) {
    int h = 2 + i * 2;
    int bx = x + i * 4;
    int by = y + 8 - h;
    if (alive && i < bars) display.fillRect(bx, by, 3, h, SSD1306_WHITE);
    else                   display.drawRect(bx, by, 3, h, SSD1306_WHITE);
  }
}

void drawBar(int x, int y, int w, int h, float frac) {
  display.drawRoundRect(x, y, w, h, 2, SSD1306_WHITE);
  int fw = (int)((w - 4) * clamp01(frac));
  if (fw > 0) display.fillRect(x + 2, y + 2, fw, h - 4, SSD1306_WHITE);
}

// One data row: value right-aligned to x=72, unit at x=75, bar on the right
void drawValue(int y, const char *txt) {
  display.setTextSize(2);
  int w = strlen(txt) * 12;
  display.setCursor(72 - w, y);
  display.print(txt);
}

void drawUI() {
  unsigned long now = millis();
  bool alive = gotData && (now - lastPacket < SIGNAL_TIMEOUT);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // ----- Header -----
  display.setTextSize(1);
  display.setCursor(0, 1);
  if (!gotData) {
    display.print("Waiting for data");
    int dots = (now / 400) % 4;
    for (int i = 0; i < dots; i++) display.print('.');
  } else if (!alive) {
    if ((now / 500) % 2) display.print("NO SIGNAL");   // blinking
  } else {
    display.print("RX #");
    display.print(count);
    display.setCursor(70, 1);
    display.print(rssi);
  }
  // blinking dot on new packet
  if (alive && now - lastPacket < 250) display.fillCircle(104, 4, 2, SSD1306_WHITE);
  drawSignal(112, 0, rssiToBars(rssi), alive);
  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  char buf[12];

  // ----- Temperature -----
  iconThermo(0, 14);
  snprintf(buf, sizeof(buf), "%.1f", dispT);
  drawValue(14, buf);
  display.setTextSize(1);
  display.setCursor(75, 14); display.write(247);        // degree
  display.setCursor(81, 14); display.print("C");
  drawBar(96, 17, 31, 9, barT);

  // ----- Humidity -----
  iconDrop(0, 31);
  snprintf(buf, sizeof(buf), "%.1f", dispH);
  drawValue(31, buf);
  display.setTextSize(1);
  display.setCursor(75, 31); display.print("%");
  drawBar(96, 34, 31, 9, barH);

  // ----- Pressure -----
  iconGauge(0, 48);
  snprintf(buf, sizeof(buf), "%.0f", dispP);
  drawValue(48, buf);
  display.setTextSize(1);
  display.setCursor(75, 48); display.print("hPa");
  drawBar(96, 51, 31, 9, barP);

  display.display();
}

// ---------- Splash ----------
void splash() {
  for (int p = 0; p <= 100; p += 4) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(22, 8);
    display.print("LoRa");
    display.setTextSize(1);
    display.setCursor(30, 28);
    display.print("Weather Station");
    drawBar(14, 44, 100, 10, p / 100.0f);
    display.display();
    delay(25);
  }
  delay(200);
}

void fatal(const char *l1, const char *l2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20); display.println(l1);
  display.println(l2);
  display.display();
  while (true) delay(1000);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found!");
    while (true) delay(1000);
  }
  splash();

  SPI.setRX(LORA_MISO);
  SPI.setTX(LORA_MOSI);
  SPI.setSCK(LORA_SCK);
  LoRa.setSPI(SPI);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQ)) fatal("LoRa init failed!", "Check wiring");

  // Must match transmitter
  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  Serial.println("Receiver ready");
}

// ---------- Loop ----------
void loop() {
  // 1) Receive (non-blocking, runs every pass)
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    String data = "";
    while (LoRa.available()) data += (char)LoRa.read();

    int i1 = data.indexOf(',');
    int i2 = data.indexOf(',', i1 + 1);
    int i3 = data.indexOf(',', i2 + 1);

    if (i1 > 0 && i2 > 0 && i3 > 0) {
      count = data.substring(0, i1);
      tgtT  = data.substring(i1 + 1, i2).toFloat();
      tgtH  = data.substring(i2 + 1, i3).toFloat();
      tgtP  = data.substring(i3 + 1).toFloat();
      rssi  = LoRa.packetRssi();
      lastPacket = millis();

      if (!gotData) {            // first packet: start animation from a neutral value
        dispT = tgtT - 10;
        dispH = tgtH - 20;
        dispP = tgtP - 30;
        gotData = true;
      }
      Serial.printf("#%s T=%.2f C H=%.2f %% P=%.2f hPa RSSI=%d\n",
                    count.c_str(), tgtT, tgtH, tgtP, rssi);
    }
  }

  // 2) Animate + draw at a fixed frame rate
  unsigned long now = millis();
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;

    // easing: move a fraction of the remaining distance each frame
    dispT += (tgtT - dispT) * EASE;
    dispH += (tgtH - dispH) * EASE;
    dispP += (tgtP - dispP) * EASE;
    if (fabsf(tgtT - dispT) < 0.05f) dispT = tgtT;   // snap when close
    if (fabsf(tgtH - dispH) < 0.05f) dispH = tgtH;
    if (fabsf(tgtP - dispP) < 0.5f)  dispP = tgtP;

    // bars glide to their targets too (ranges: -10..50 C, 0..100 %, 950..1050 hPa)
    barT += (mapf(dispT, -10, 50)   - barT) * EASE;
    barH += (mapf(dispH, 0, 100)    - barH) * EASE;
    barP += (mapf(dispP, 950, 1050) - barP) * EASE;

    drawUI();
  }
}
