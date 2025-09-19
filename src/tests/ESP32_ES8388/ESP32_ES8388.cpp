// the pin assignment matches the Olimex ADF board

#include "Arduino.h"
#include "WiFi.h"
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#include "Wire.h"
#include "ES8388.h"  // https://github.com/schreibfaul1/es8388
#include "Audio.h"   // https://github.com/schreibfaul1/ESP32-audioI2S
#include "pins.h"


// Amplifier enable
//#define GPIO_PA_EN    19

char ssid[] =     "MKEF";
char password[] = "1234567cbe";


int volume = 80;                            // 0...100

ES8388 es;
Audio audio;

//----------------------------------------------------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    Serial.println("\r\nReset");

    //SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
   // SPI.setFrequency(1000000);

    pinMode(PIN_SD_MMC_D0, INPUT_PULLUP);
    SD_MMC.setPins(PIN_SD_MMC_CLK,PIN_SD_MMC_CMD, PIN_SD_MMC_D0);
    if(!SD_MMC.begin( "/sdmmc", true, false, 20000)){
       Serial.println("Card Mount Failed");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while(WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(100);
    }

    Serial.printf_P(PSTR("Connected\r\nRSSI: "));
    Serial.print(WiFi.RSSI());
    Serial.print(" IP: ");
    Serial.println(WiFi.localIP());

    Serial.printf("Connect to ES8388 codec... ");
    while (not es.begin(PIN_I2C_SDA, PIN_I2C_SCL))
    {
        Serial.printf("Failed!\n");
        delay(1000);
    }
    Serial.printf("OK\n");

    es.volume(ES8388::ES_MAIN, volume);
    es.volume(ES8388::ES_OUT1, volume);
    es.volume(ES8388::ES_OUT2, volume);
    es.mute(ES8388::ES_OUT1, false);
    es.mute(ES8388::ES_OUT2, false);
    es.mute(ES8388::ES_MAIN, false);
    //es.setEqualizer();
    // Enable amplifier
   pinMode(TDA7052_SHUTDOWN_PIN, OUTPUT);
    digitalWrite(TDA7052_SHUTDOWN_PIN, LOW);

    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_SDOUT, I2S_MCLK);
    audio.setVolume(21); // 0...21
    audio.setTone(3,1,0);
   audio.connecttohost("https://stream.zeno.fm/ftly191pojttv");
   //audio.connecttohost("https://stream.radiojar.com/pr9r38w802hvv");
  //audio.connecttoFS(SD_MMC, "test1.mp3");
  uint32_t samplerate=audio.getSampleRate();
    Serial.printf("Sample rate: %d\n", samplerate); 
  //audio.connecttospeech("���� ����� ��� ���� �������� ����������� ��� ������������� .", "gr");
}
//----------------------------------------------------------------------------------------------------------------------
void loop(){
    vTaskDelay(1);
    audio.loop();
}
//----------------------------------------------------------------------------------------------------------------------

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
}
void audio_showstation(const char *info){
    Serial.print("station     ");Serial.println(info);
}
void audio_showstreaminfo(const char *info){
    Serial.print("streaminfo  ");Serial.println(info);
}
void audio_showstreamtitle(const char *info){
    Serial.print("streamtitle ");Serial.println(info);
}
void audio_bitrate(const char *info){
    Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info){  //duration in sec
    Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info){  //homepage
    Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    Serial.print("lasthost    ");Serial.println(info);
}
void audio_eof_speech(const char *info){
    Serial.print("eof_speech  ");Serial.println(info);
}

