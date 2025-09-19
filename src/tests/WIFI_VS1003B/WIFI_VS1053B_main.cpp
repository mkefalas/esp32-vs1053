#include <Arduino.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <VS1053.h>  // from ESP_VS1053_Library
#include <pins.h>  // from ESP_VS1053_Library
#include "esp_log.h"

// Wi-Fi + stream config
const char* WIFI_SSID  = "MKEF";
const char* WIFI_PASS  = "1234567cbe";
const char* STREAM_URL = "https://file-examples.com/wp-content/uploads/2017/11/file_example_MP3_700KB.mp3";


// only 3 pins here!
VS1053 player(VS1003B_CS_PIN, VS1003B_DCS_PIN, VS1003B_DREQ_PIN);
WiFiClient client;
uint8_t mp3buff[8192];
WiFiClientSecure secureClient;
HTTPClient http;

//  http://comet.shoutca.st:8563/1
const char *host = "ice2.onestreaming.com";
const char *path = "/akousjazzin";
int httpPort = 80;
// Default volume
#define VOLUME  90

// The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.


void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.disconnect(true);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting Wi-Fi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > 20000) {
      Serial.println("\nRetrying...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      t0 = millis();
    }
    Serial.print('.');
    delay(500);
  }
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  pinMode(VS1003B_DREQ_PIN, INPUT_PULLUP);
  esp_log_level_set("*", ESP_LOG_INFO);      // enable INFO+ globally
  // Wait for VS1053 and PAM8403 to power up
  // otherwise the system might not start up correctly
  delay(3000);

  // This can be set in the IDE no need for ext library
  // system_update_cpu_freq(160);

  Serial.println("\n\nSimple Radio Node WiFi Radio");
// reset & SPI init
pinMode(VS1003B_RST_PIN, OUTPUT);
digitalWrite(VS1003B_RST_PIN, LOW);
delay(100);
digitalWrite(VS1003B_RST_PIN, HIGH);
delay(100);
  SPI.begin(VS1003B_CLK_PIN,VS1003B_MISO_PIN,VS1003B_MOSI_PIN,VS1003B_CS_PIN);

  player.begin();
  //player.loadDefaultVs1053Patches();
  player.switchToMp3Mode();
  player.setVolume(VOLUME);

  Serial.print("Connecting to SSID ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("connecting to ");
  Serial.println(host);

  if (!client.connect(host, httpPort)) {
      Serial.println("Connection failed");
      return;
  }

  Serial.print("Requesting stream: ");
  Serial.println(path);

  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
}


void loop() {
  if (!client.connected()) {
    Serial.println("Reconnecting...");
    if (client.connect(host, httpPort)) {
        client.print(String("GET ") + path + " HTTP/1.1\r\n" +
        "Host: " + host + "\r\n" +
        "Connection: close\r\n\r\n");
    }
}

if (client.available() > 0) {
    // The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
    uint8_t bytesread = client.read(mp3buff,8192);
    Serial.printf(
      "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
      mp3buff[0], mp3buff[1], mp3buff[2], mp3buff[3],
      mp3buff[4], mp3buff[5], mp3buff[6], mp3buff[7],
      mp3buff[8], mp3buff[9], mp3buff[10], mp3buff[11],
      mp3buff[12], mp3buff[13], mp3buff[14], mp3buff[15]);
}
}
