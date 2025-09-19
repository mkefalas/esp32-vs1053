/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <Arduino.h>
#include <VS1003B.h>
#include <MySi4703.h> 
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <WiFi.h>
#include <pins.h>
#include <SerialMonitor.h>

VS1003B player(VS1003B_CS_PIN, VS1003B_DCS_PIN, VS1003B_DREQ_PIN, VS1003B_RST_PIN);
Si4703 radio;             // using default values for all settings

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

uint8_t vol=15;
Adafruit_SSD1306 display; 

String ssid = "MKEF";
String password = "1234567cbe";
String streamHost = "stream.zeno.fm";
const int streamPort = 80;
String streamPath = "/ftly191pojttv";
WiFiClient radioClient;

uint8_t streamBuffer[64 * 1024];  // Or use ps_malloc if needed
size_t head = 0, tail = 0;

void setup() {
  Serial.println("Setup start");
  SerialMonitor_Init();
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW); // Set high
  delay(1);
  digitalWrite(RST_PIN, HIGH); // Set high
  digitalWrite(42, HIGH);  // Bring Si4703 out of reset with SDIO set to low and SEN pulled high with on-board resistor
  delay(10);
  SPI.begin(VS1003B_CLK_PIN, VS1003B_MISO_PIN, VS1003B_MOSI_PIN); 
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));

    for (;;); // Don't proceed, loop forever
}
 
  display.clearDisplay();
 radio.start();
  delay(10);
 radio.setChannel(8800); // Set frequency 94.4 Mhz  
  radio.setVolume(vol);
//  player.begin();
 WiFi.disconnect(true); 
 delay(1000);            // Give it a moment to reset
   Serial.printf("Connecting to WiFi network: %s\n", ssid);
  WiFi.begin(ssid, password);  
  do {
    delay(100);
    Serial.print(".");
  } while (WiFi.status() != WL_CONNECTED);

  Serial.println("WiFi connected - starting decoder...");


  radioClient.print(String("GET ") + streamPath + " HTTP/1.1\r\n" +
                    "Host: " + streamHost + "\r\n" +
                    "Connection: keep-alive\r\n\r\n");


  while (radioClient.connected()) {
    String line = radioClient.readStringUntil('\n');
    if (line == "\r") break;
  }

  Serial.println("Streaming started");
  //
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
}
//
  player.setVolume(40, 40);
  player.setEqualizer(5, 5);
  player.setSource(SOURCE_SPI);

//
  // After some time, play a file via SPI
  delay(5000);

}

void loop() {
 // Fill buffer
 while (radioClient.available() && ((head + 1) % sizeof(streamBuffer)) != tail) {
  streamBuffer[head] = radioClient.read();
  head = (head + 1) % sizeof(streamBuffer);
}

// Send to VS1003B
if (digitalRead(VS1003B_DREQ_PIN)) {
  uint8_t chunk[32];
  size_t count = 0;
  while (tail != head && count < sizeof(chunk)) {
    chunk[count++] = streamBuffer[tail];
    tail = (tail + 1) % sizeof(streamBuffer);
  }
  if (count > 0) {
    player.sdiWrite(chunk, count);
  }
}
}