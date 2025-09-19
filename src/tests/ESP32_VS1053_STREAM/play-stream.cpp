#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <VS1053.h>               // https://github.com/baldram/ESP_VS1053_Library
#include <ESP32_VS1053_Stream.h>
#include <pins.h>



ESP32_VS1053_Stream stream;

const char* SSID = "MKEF";
const char* PSK = "1234567cbe";

void setup() {
    Serial.begin(115200);

    while (!Serial)
        delay(10);

    Serial.println("\n\nVS1053 Radio Streaming Example\n");
    
    // VS1053 Reset logic
    pinMode(VS1003B_RST_PIN, OUTPUT);
    digitalWrite(VS1003B_RST_PIN, LOW);
    delay(100);
    digitalWrite(VS1003B_RST_PIN, HIGH);
    delay(100);

    // Connect to Wi-Fi
    Serial.printf("Connecting to WiFi network: %s\n", SSID);
    WiFi.begin(SSID, PSK);  
    WiFi.setSleep(false);  // Important to disable sleep to ensure stable connection

    while (!WiFi.isConnected())
        delay(10);

    Serial.println("WiFi connected - starting decoder...");

    // Start SPI bus
    SPI.setHwCs(true);
    SPI.begin(VS1003B_CLK_PIN, VS1003B_MISO_PIN, VS1003B_MOSI_PIN);

    // Initialize the VS1053 decoder
    if (!stream.startDecoder(VS1003B_CS_PIN, VS1003B_DCS_PIN, VS1003B_DREQ_PIN) || !stream.isChipConnected()) {
        Serial.println("Decoder not running - system halted");
        while (1) delay(100);
    }
    Serial.println("VS1053 running - starting radio stream");

    // Connect to the radio stream
   stream.connecttohost("http://radiostreaming.ert.gr/ert-talaika");
   // stream.connecttohost("http://stream.zeno.fm/t272zvxu4yzuv");
   // stream.connecttohost("https://stream.radiojar.com/pr9r38w802hvv");
    //stream.connecttohost("https://radio.streamings.gr/proxy/topmelody?mp=/stream");
    
    unsigned long startWait = millis();
    while (!stream.isRunning() && millis() - startWait < 20000) {
        stream.loop();
        delay(1);
    }
    if (!stream.isRunning()) {
        Serial.println("Stream not running - system halted");
        while (1) delay(100);
    }
    if (!stream.isRunning()) {
        Serial.println("Stream not running - system halted");
        while (1) delay(100);
    }

    Serial.print("Codec: ");
    Serial.println(stream.currentCodec());

    Serial.print("Bitrate: ");
    Serial.print(stream.bitrate());
    Serial.println(" kbps");


    uint8_t eq[4] = {0x6, 0x0, 0x0a, 0x0a}; // Example values
    stream.setTone(eq);
    stream.setVolume(99);
}

void loop() {
    stream.loop();
    delay(5);
}

void audio_showstation(const char* info) {
    Serial.printf("Station: %s\n", info);
}

void audio_showstreamtitle(const char* info) {
    Serial.printf("Stream title: %s\n", info);
}

void audio_eof_stream(const char* info) {
    Serial.printf("End of stream: %s\n", info);
}
