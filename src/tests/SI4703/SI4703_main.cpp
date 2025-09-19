#include <Arduino.h>
#include <SI470X.h>
#include <Wire.h>
#include <pins.h>
#include "esp_sleep.h"

constexpr int PIN_UART0_RX = 44;
constexpr int PIN_UART0_TX = 43;

// ????????????????????????????????? CONFIG ????????????????????????????????????
constexpr uint16_t BAND_MIN    = 875;    // 87.5 MHz  (?0.1)
constexpr uint16_t BAND_MAX    = 1080;   // 108.0 MHz (?0.1)
constexpr uint16_t STEP        = 1;      // 0.1 MHz channel step
constexpr uint32_t RSSI_PERIOD = 500;    // ms between idle RSSI prints
// ?????????????????????????????????????????????????????????????????????????????

SI470X           radio;
uint16_t         freq = 1010;   // 101.0 MHz (?0.1)
uint8_t          vol  = 8;      // 0?15
int8_t           lastRSSI = -128;
unsigned long    tLastRSSI = 0;

// ?????????????????????????? HELPERS ?????????????????????????????????????????
inline void hwSetFreq()        { radio.setFrequency(freq * 10); }   // lib: 10?kHz units
inline void hwFreqFromSeek()   { freq = radio.getFrequency() / 10; }
inline void wrapFreq()         { if (freq < BAND_MIN) freq = BAND_MAX; else if (freq > BAND_MAX) freq = BAND_MIN; }

void printStatus() {
  Serial.print(F("Freq: ")); Serial.print(freq / 10); Serial.print('.'); Serial.print(freq % 10);
  Serial.print(F(" MHz | RSSI: ")); Serial.print(lastRSSI = radio.getRssi());
  Serial.print(F(" | VOL: ")); Serial.println(vol);
}

// ?????????????????????????? COMMANDS ???????????????????????????????????????
void cmdPrev()      { freq -= STEP; wrapFreq(); hwSetFreq(); printStatus(); }
void cmdNext()      { freq += STEP; wrapFreq(); hwSetFreq(); printStatus(); }
void cmdVolDown()   { if (vol)     radio.setVolume(--vol); printStatus(); }
void cmdVolUp()     { if (vol < 15) radio.setVolume(++vol); printStatus(); }
void cmdSeekDown()  { radio.seek(SI470X_SEEK_WRAP, SI470X_SEEK_DOWN); hwFreqFromSeek(); printStatus(); }
void cmdSeekUp()    { radio.seek(SI470X_SEEK_WRAP, SI470X_SEEK_UP);   hwFreqFromSeek(); printStatus(); }
void cmdHelp();

struct Cmd { char key; void (*fn)(); };
const Cmd CMDS[] = {
  { 'z', cmdPrev      }, { 'x', cmdNext   }, { 'b', cmdVolDown },
  { 'n', cmdVolUp     }, { 'c', cmdSeekDown}, { 'v', cmdSeekUp  },
  { 'h', cmdHelp      }
};

void cmdHelp() {
  Serial.println(F("Commands: z prev | x next | b vol? | n vol+ | c seek? | v seek+ | h help"));
  Serial.println(F("Type frequency (e.g. 102.1) and press Enter. Backspace to edit."));
}

// ????????????????????? NUMERIC INPUT BUFFERS ???????????????????????????????
char buf[6] = {};  // holds "108.0" + null
uint8_t len = 0;
inline void clearBuf() { len = 0; buf[0] = '\0'; }
inline void backspace() { if (len) { len--; buf[len] = '\0'; Serial.print("\b \b"); } }

void tuneFromBuf() {
  if (!len) return;
  float f = atof(buf); clearBuf();
  if (f < 87.5f || f > 108.0f) { Serial.println(F("Out of band")); return; }
  freq = static_cast<uint16_t>(round(f * 10.f)); hwSetFreq(); printStatus();
}

void setup() {
    Serial.begin(115200);
    Serial.println("Setup si4703 start");
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
    radio.setup(RST_PIN, PIN_I2C_SDA);
     pinMode(TDA7052_SHUTDOWN_PIN, OUTPUT);
    digitalWrite(TDA7052_SHUTDOWN_PIN, LOW);
    delay(100);
    hwSetFreq();
    cmdHelp();
    printStatus();
    radio.setMono(true); // Set stereo mode
    radio.setRds(true);          // turn the RDS decoder ON
    //radio.setSeekThreshold(20); // Set seek threshold to 10
   // esp_sleep_enable_timer_wakeup( (uint64_t)2000 * 1000 );
}

void loop() {
 // Serial handling
 while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\b' || ch == 0x7F) { backspace(); continue; }
    if (isdigit(ch) || ch == '.') {
      if (len < sizeof(buf)-1 && !(ch=='.' && strchr(buf,'.'))) {
        Serial.write(ch);
        buf[len++] = ch; buf[len] = '\0';
      }
      continue;
    }
    if (ch == '\n' || ch == '\r') { Serial.println(); tuneFromBuf(); continue; }

    // command keys (not echoed)
    for (const auto &c : CMDS) {
      if (tolower(ch) == c.key) { c.fn(); clearBuf(); break; }
    }
  }

  // Periodic RSSI update
  if (millis() - tLastRSSI > RSSI_PERIOD) {
    tLastRSSI = millis(); int8_t r = radio.getRssi(); if (abs(r - lastRSSI) >= 3) printStatus();
  }

}