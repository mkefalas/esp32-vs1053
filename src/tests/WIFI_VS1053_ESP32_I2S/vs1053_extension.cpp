#include "Arduino.h"
#include "WiFi.h"
#include "SPI.h"
//#include "SD.h"
//#include "FS.h"
#include "vs1053_ext.h"
#include "pins.h" // ESP32 pin definitions
#include "esp_log.h"

// Digital I/O used
//#define SD_CS        5


String ssid =     "MKEF";
String password = "1234567cbe";

int volume=30;


VS1053 mp3(VS1003B_CS_PIN, VS1003B_DCS_PIN, VS1003B_DREQ_PIN, SPI2_HOST , VS1003B_MOSI_PIN, VS1003B_MISO_PIN, VS1003B_CLK_PIN);

//The setup function is called once at startup of the sketch
void setup() {
  
    pinMode(SD_CS, OUTPUT);      digitalWrite(SD_CS, HIGH);
    Serial.begin(115200);
   
   // esp_log_level_set("*", ESP_LOG_INFO);      // enable INFO+ globally
   // SPI.begin(VS1003B_CLK_PIN, VS1003B_MISO_PIN, VS1003B_MOSI_PIN);
pinMode(VS1003B_RST_PIN, OUTPUT);
digitalWrite(VS1003B_RST_PIN, LOW);
delay(100);
digitalWrite(VS1003B_RST_PIN, HIGH);
delay(100);
//SPI.begin(VS1003B_CLK_PIN,VS1003B_MISO_PIN,VS1003B_MOSI_PIN,VS1003B_CS_PIN);
  // SD.begin(SD_CS);
   // WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) delay(1500);
     Serial.println("\n\nVS1053 WiFi Radio Example\n");
    mp3.begin();
    mp3.setVolume(volume);
    uint8_t eq[4] = {0x0f, 0x08, 0x0f, 0x0b}; // Example values
    mp3.setTone(eq);
   // mp3.connecttohost("streambbr.ir-media-tec.com/berlin/mp3-128/vtuner_web_mp3/");
    //mp3.connecttohost("stream.landeswelle.de/lwt/mp3-192");                 // mp3 192kb/s
   // mp3.connecttohost("http://radio.hear.fi:8000/hear.ogg");                // ogg
    //mp3.connecttohost("tophits.radiomonster.fm/320.mp3");                   // bitrate 320k
   // mp3.connecttohost("http://star.jointil.net/proxy/jrn_beat?mp=/stream"); // chunked data transfer
   // mp3.connecttohost("http://stream.srg-ssr.ch/rsp/aacp_48.asx");          // asx
   //mp3.connecttohost("www.surfmusic.de/m3u/100-5-das-hitradio,4529.m3u");  // m3u
    //mp3.connecttohost("https://raw.githubusercontent.com/schreibfaul1/ESP32-audioI2S/master/additional_info/Testfiles/Pink-Panther.wav"); // webfile
    //mp3.connecttohost("http://stream.revma.ihrhls.com/zc5060/hls.m3u8");    // HLS
    //mp3.connecttohost("https://mirchiplaylive.akamaized.net/hls/live/2036929/MUM/MEETHI_Auto.m3u8"); // HLS transport stream
   mp3.connecttoFS(SD, "test1.mp3"); // SD card, local file
    //mp3.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de");
     //audio.connecttohost("https://pub0202.101.ru:8000/stream/trust/mp3/128/173");//ru.deep house
 //mp3.connecttohost("http://cast3.radiohost.ovh:8352/"); // blues radio 
  //mp3.connecttohost("http://stream.zeno.fm/u8pcm6se31zuv");//sound fm 
//mp3.connecttohost("https://stream.zeno.fm/ftly191pojttv"); //caracas radio
// audio.connecttohost("https://webinternetradio.eu:48796/");// radio thespies
 //audio.connecttohost("http://strm112.1.fm/deeptech_mobile_mp3"); //deep techno 
// mp3.connecttohost("https://stream.radiojar.com/pr9r38w802hvv"); //dalkas
 //http://athina984.live24.gr/athina984;984
// mp3.connecttohost("http://cast4.my-control-panel.com:9003/stream"); //984 kriti 
 //mp3.connecttohost("https://maximacenterdata.com/8132/stream"); //radio oasis 
//mp3.connecttohost("http://radiostreaming.ert.gr/ert-talaika"); // ert radio laika 
//mp3.connecttohost("https://i2.streams.ovh/sc/tostekit/stream"); // radio 1
}

// The loop function is called in an endless loop
void loop()
{
    mp3.loop();
}

// next code is optional:
void vs1053_info(const char *info) {                // called from vs1053
    Serial.print("DEBUG:        ");
    Serial.println(info);                           // debug infos
}
void vs1053_showstation(const char *info){          // called from vs1053
    Serial.print("STATION:      ");
    Serial.println(info);                           // Show station name
}
void vs1053_showstreamtitle(const char *info){      // called from vs1053
    Serial.print("STREAMTITLE:  ");
    Serial.println(info);                           // Show title
}
void vs1053_showstreaminfo(const char *info){       // called from vs1053
    Serial.print("STREAMINFO:   ");
    Serial.println(info);                           // Show streaminfo
}
void vs1053_eof_mp3(const char *info){              // called from vs1053
    Serial.print("vs1053_eof:   ");
    Serial.print(info);                             // end of mp3 file (filename)
}
void vs1053_bitrate(const char *br){                // called from vs1053
    Serial.print("BITRATE:      ");
    Serial.println(String(br)+"kBit/s");            // bitrate of current stream
}
void vs1053_commercial(const char *info){           // called from vs1053
    Serial.print("ADVERTISING:  ");
    Serial.println(String(info)+"sec");             // info is the duration of advertising
}
void vs1053_icyurl(const char *info){               // called from vs1053
    Serial.print("Homepage:     ");
    Serial.println(info);                           // info contains the URL
}
void vs1053_eof_speech(const char *info){           // called from vs1053
    Serial.print("end of speech:");
    Serial.println(info);
}
void vs1053_lasthost(const char *info){             // really connected URL
    Serial.print("lastURL:      ");
    Serial.println(info);
}

