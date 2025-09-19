// ESP32 + VS1053B: Webstream + SD (WAV/MP3 Interrupts)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <SD_MMC.h>
#include <VS1053.h>
#include "pins_new.h"


int icyMetaInt = 0;
int icyMetaCount = 0;
bool icyHeadersParsed = false;
bool inChunked = false;

const char* ssid = "MKEF";
const char* password = "1234567cbe";
//const char* streamURL = "https://stream.myip.gr/proxy/deejay?mp=/stream"; 
//const char* streamURL = "http://radiostreaming.ert.gr/ert-talaika";
//const char* streamURL = "http://ice.onestreaming.com/athensparty";
//const char* streamURL = "https://stream.radiojar.com/pr9r38w802hvv";
const char* streamURL = "https://stream.zeno.fm/lwv6zqgtv1dtv";
//const char* streamURL = "https://stream.radiojar.com/pepper";
//const char* streamURL = "http://galaxy.live24.gr/galaxy9292";


VS1053 player(VS1003B_CS_PIN, VS1003B_DCS_PIN, VS1003B_DREQ_PIN);
WiFiClient* radioClient = nullptr;
HTTPClient http;
File audioFile;

enum Mode { BUFFERING, STREAMING, WAV_INTERRUPT, MP3_SD };
Mode mode = BUFFERING;
Mode resumeMode = STREAMING;  // Used to remember what to return to
bool streamingWebRadio = true;  // true = web stream, false = SD card

#define BUFFER_SIZE 32768
#define CHUNK_SIZE  256
uint8_t ringBuffer[BUFFER_SIZE];
volatile size_t head = 0, tail = 0;

size_t bufferAvailable() {
  return head >= tail ? head - tail : BUFFER_SIZE - tail + head;
}

size_t bufferFree() {
  return BUFFER_SIZE - bufferAvailable() - 1;
}

void bufferWrite(uint8_t b) {
  size_t next = (head + 1) % BUFFER_SIZE;
  if (next != tail) {
    ringBuffer[head] = b;
    head = next;
  }
}

int bufferRead(uint8_t* buf, size_t len) {
  size_t count = 0;
  while (count < len && bufferAvailable()) {
    buf[count++] = ringBuffer[tail];
    tail = (tail + 1) % BUFFER_SIZE;
  }
  return count;
}

#define FILL_BUFSIZE 1024
void fillBufferFromStream() {
    if (!radioClient) return;

    // Persistent state
    static size_t chunkRemaining = 0;
    static bool waitingForChunkHeader = true;
    static String chunkSizeStr = "";
    static int icyCounter = 0;
    static int metaSkip = 0;

    uint8_t tempBuf[1024]; // temp buffer

    while (bufferFree() > 0 && radioClient->available()) {

        // ---- Handle ICY metadata skip ----
        if (metaSkip > 0) {
            int skipNow = min(metaSkip, (int)radioClient->available());
            for (int i = 0; i < skipNow; i++) radioClient->read();
            metaSkip -= skipNow;
            if (metaSkip > 0) break; // Not enough bytes yet
            continue;
        }

        // ---- Handle ICY metadata point ----
        if (icyMetaInt > 0 && icyCounter == icyMetaInt) {
            if (radioClient->available()) {
                int metaLen = radioClient->read();
                if (metaLen > 0) {
                    metaSkip = metaLen * 16;
                    Serial.printf("ICY Metadata detected, skipping %d bytes\n", metaSkip);
                }
                icyCounter = 0;
            } else {
                break;
            }
            continue;
        }

        // ---- Chunked mode ----
        if (inChunked) {
            // Read chunk header if needed
            if (waitingForChunkHeader) {
                char c;
                while (radioClient->available() && (c = radioClient->read()) != '\n') {
                    if (c != '\r') chunkSizeStr += c;
                }
                if (chunkSizeStr.length() == 0) break; // Wait for full header
                if (c != '\n') break; // Wait for newline
                chunkSizeStr.trim();
                chunkRemaining = strtol(chunkSizeStr.c_str(), nullptr, 16);
                chunkSizeStr = "";
                waitingForChunkHeader = false;
                if (chunkRemaining == 0) break; // End of stream
                continue;
            }

            // Read as much as we can from the chunk
            int toRead = min(min((int)chunkRemaining, (int)bufferFree()), (int)radioClient->available());
            toRead = min(toRead, (int)sizeof(tempBuf));
            if (toRead > 0) {
                int n = radioClient->read(tempBuf, toRead);
                for (int i = 0; i < n; i++) {
                    bufferWrite(tempBuf[i]);
                    icyCounter++;
                }
                chunkRemaining -= n;
            }

            if (chunkRemaining == 0 && !waitingForChunkHeader) {
                if (radioClient->available()) radioClient->read(); // skip \r
                if (radioClient->available()) radioClient->read(); // skip \n
                waitingForChunkHeader = true;
            }
            continue;
        }

        // ---- Normal, non-chunked ----
        int bytesUntilMeta = (icyMetaInt > 0) ? (icyMetaInt - icyCounter) : 1024;
        int toRead = min(min(bytesUntilMeta, (int)bufferFree()), (int)radioClient->available());
        toRead = min(toRead, (int)sizeof(tempBuf));
        if (toRead > 0) {
            int n = radioClient->read(tempBuf, toRead);
            for (int i = 0; i < n; i++) bufferWrite(tempBuf[i]);
            icyCounter += n;
        } else {
            break;
        }
    }
}





bool resolveFinalStreamURL(const String& inputURL, String& resolvedURL, int maxRedirects = 5) {
  String url = inputURL;
  for (int i = 0; i < maxRedirects; i++) {
    http.end();
    WiFiClient* client = url.startsWith("https") ? (WiFiClient*)new WiFiClientSecure() : new WiFiClient();
    if (url.startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();
    http.begin(*client, url);
    http.collectHeaders((const char*[]){"Location", "Content-Type"}, 2);
    int code = http.GET();
    if (code == HTTP_CODE_OK) return resolvedURL = url, true;
    if (code == HTTP_CODE_FOUND || code == HTTP_CODE_MOVED_PERMANENTLY) {
      String loc = http.header("Location");
      if (loc.length() == 0) return false;
      url = loc.startsWith("/") ? url.substring(0, url.indexOf('/', 8)) + loc : loc;
    } else return false;
  }
  return false;
}

bool startStream() {
  String finalURL;
  if (!resolveFinalStreamURL(streamURL, finalURL)) return false;
  http.end();
  WiFiClient* client = finalURL.startsWith("https") ? (WiFiClient*)new WiFiClientSecure() : new WiFiClient();
  if (finalURL.startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();
  http.begin(*client, finalURL);

 // http.addHeader("Icy-MetaData", "1");
  http.addHeader("Connection", "keep-alive");
  http.addHeader("Keep-Alive", "timeout=60, max=100");

  // List all headers you want to see here!
  const char* headers[] = {
    "Transfer-Encoding", "Icy-Metaint", "Icy-Description", "Icy-Genre",
    "Icy-Name", "Icy-Pub", "Icy-Br", "Content-Type", "Connection", "Keep-Alive",
    "Server", "Cache-Control", "Pragma", "Expires"
  };
  http.collectHeaders(headers, sizeof(headers) / sizeof(headers[0]));

  if (http.GET() != HTTP_CODE_OK) return false;

  radioClient = http.getStreamPtr();
  inChunked = false;

  // Print all collected headers for debugging
  Serial.println("---- HTTP HEADERS RECEIVED ----");
  for (int i = 0; i < http.headers(); i++) {
      String name = http.headerName(i);
      String value = http.header(i);
      Serial.printf("[%d] '%s': '%s'\n", i, name.c_str(), value.c_str());
      name.trim(); value.trim();
      name.toLowerCase(); value.toLowerCase();
      if (name == "transfer-encoding" && value.indexOf("chunked") >= 0) {
          inChunked = true;
          Serial.println("Detected chunked transfer encoding!");
      }
  }
  Serial.printf("inChunked = %s\n", inChunked ? "true" : "false");

  String metaIntStr = http.header("icy-Metaint");
  if (metaIntStr.length() > 0) {
    icyMetaInt = metaIntStr.toInt();
  }

  return true;
}


void startWavInterrupt(const char* filename = "/test2.wav") {
  if (mode == WAV_INTERRUPT) return;
  Serial.println("Playing WAV...");
  audioFile = SD_MMC.open(filename);
  if (!audioFile) return;
  resumeMode = mode;
  player.softReset();
  mode = WAV_INTERRUPT;
}

void startMp3Playback(const char* filename) {
  if (mode == MP3_SD) return;
  Serial.printf("Playing MP3: %s\n", filename);
  audioFile = SD_MMC.open(filename);
  if (!audioFile) return;
  resumeMode = mode;
  player.softReset();
  mode = MP3_SD;
}

void resumePreviousMode() {
  player.softReset();
  if (resumeMode == STREAMING) mode = BUFFERING;
  else mode = resumeMode;
}

void toggleStreamingMode() {
  if (streamingWebRadio) {
    // Currently on web stream, switch to SD card (keep web connection alive)
    Serial.println("Switching from web stream to SD card MP3 (keeping web connection alive)...");
    streamingWebRadio = false;
    startMp3Playback("/output.mp3");
  } else {
    // Currently on SD card, switch to web stream
    Serial.println("Switching from SD card to web stream...");
    streamingWebRadio = true;
    if (audioFile) audioFile.close();
    player.softReset();
    
    // If we don't have a web connection, start one
    if (!radioClient) {
      if (startStream()) {
        mode = BUFFERING;
        Serial.println("Web stream started - buffering...");
      } else {
        Serial.println("Failed to start web stream");
        streamingWebRadio = false; // Revert flag on failure
        return;
      }
    } else {
      // We already have a connection, just switch to streaming mode
      mode = STREAMING;
      Serial.println("Resuming web stream playback...");
    }
  }
}

uint8_t volume=90;
void setup() {
  Serial.begin(115200);
  pinMode(VS1003B_RST_PIN, OUTPUT);
  digitalWrite(VS1003B_RST_PIN, LOW); delay(100);
  digitalWrite(VS1003B_RST_PIN, HIGH); delay(100);
  SPI.begin(VS1003B_CLK_PIN, VS1003B_MISO_PIN, VS1003B_MOSI_PIN);
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" connected!");
  Serial.printf("WiFi signal strength: %d dBm\n", WiFi.RSSI());
  Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
  
  SD_MMC.setPins(PIN_SD_MMC_CLK, PIN_SD_MMC_CMD, PIN_SD_MMC_D0);
  if (!SD_MMC.begin("/sdmmc", true)) while (1);
  
  player.begin();
  player.setVolume(volume);

     pinMode(TDA7052_SHUTDOWN_PIN, OUTPUT);
    digitalWrite(TDA7052_SHUTDOWN_PIN, LOW);
    delay(100);

     uint8_t eq[4] = {0xd, 0x0b, 0x08, 0x08}; // Example values
    player.setTone(eq);
  if (!startStream()) while (1);
  Serial.println("READY. Commands: 's'=wav interrupt, 'm'=toggle web/SD");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
   yield(); // <-- Add here to keep WiFi and networking alive
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's') startWavInterrupt();
    if (c == 'm') toggleStreamingMode();
    // --- EQ adjustment ---
    static uint8_t eq[4] = {0x3, 0x7, 0x04, 0x06}; // Make sure this matches your global eq!
    bool eqChanged = false;
    if (c == '1' && eq[0] < 15) { eq[0]++; eqChanged = true; }
    if (c == '2' && eq[0] > 0)  { eq[0]--; eqChanged = true; }
    if (c == '3' && eq[1] < 15) { eq[1]++; eqChanged = true; }
    if (c == '4' && eq[1] > 0)  { eq[1]--; eqChanged = true; }
     if (c == '5' && eq[2] < 15) { eq[2]++; eqChanged = true; }
    if (c == '6' && eq[2] > 0)  { eq[2]--; eqChanged = true; }
    if (c == '7' && eq[3] < 15) { eq[3]++; eqChanged = true; }
    if (c == '8' && eq[3] > 0)  { eq[3]--; eqChanged = true; }
     if (c == '9' && volume) { volume++; eqChanged = true; }
    if (c == '0' && volume >70)  { volume--; eqChanged = true; }
    if (eqChanged) {
      player.setTone(eq);
      player.setVolume(volume);
      Serial.printf("EQ: [%d, %d, %d, %d] Volume: %d\n", eq[0], eq[1], eq[2], eq[3], volume );
    }
  }

  if (mode == BUFFERING) {
    fillBufferFromStream();
    static unsigned long lastBufferUpdate = 0;
    if (millis() - lastBufferUpdate > 1000) {  // Update every second
Serial.printf("Buffering: %d/%d bytes (%.1f%%)\n", 
              bufferAvailable(), BUFFER_SIZE, 
              (float)bufferAvailable() / BUFFER_SIZE * 100);
      lastBufferUpdate = millis();
    }
    if (bufferAvailable() > (BUFFER_SIZE * 75 / 100)) {
      Serial.println("Buffer ready - starting playback");
      mode = STREAMING;
    }
  } else if (mode == STREAMING) {
    fillBufferFromStream();
    // Buffer statistics during streaming
    static unsigned long lastStats = 0;
   /* if (millis() - lastStats > 10000) {  // Every 10 seconds
      Serial.printf("Stream Stats - Mode: %s, Buffer: %d/%d bytes (%.1f%%), Heap: %d, WiFi: %d dBm\n", 
                    streamingWebRadio ? "WEB" : "SD",
                    bufferAvailable(), BUFFER_SIZE, 
                    (float)bufferAvailable() / BUFFER_SIZE * 100,
                    ESP.getFreeHeap(), WiFi.RSSI());
      lastStats = millis();
    }*/
    
    if (digitalRead(VS1003B_DREQ_PIN) && bufferAvailable() >= CHUNK_SIZE) {
      uint8_t buf[CHUNK_SIZE];
      int n = bufferRead(buf, sizeof(buf));
      if (n > 0) player.playChunk(buf, n);
    }
    
    // Check for buffer underrun
    if (bufferAvailable() < (BUFFER_SIZE * 25 / 100)) {
      Serial.println("Buffer underrun - switching to buffering mode");
      mode = BUFFERING;
    }
  } 
  else if (mode == WAV_INTERRUPT || mode == MP3_SD) {
    // Continue buffering web stream in background during SD playback if connection exists
    if (radioClient) {
      fillBufferFromStream();
    }
    
    if (audioFile && digitalRead(VS1003B_DREQ_PIN)) {
      uint8_t buf[CHUNK_SIZE];
      int n = audioFile.read(buf, sizeof(buf));
      if (n > 0) {
        player.playChunk(buf, n);
      } else {
        audioFile.close();
        Serial.println("SD playback finished");
        
        // Resume appropriate mode based on current streaming setting
        if (streamingWebRadio) {
          // Return to web streaming
          resumePreviousMode();
        } else {
          // Stay in SD mode, but stop playback
          mode = MP3_SD;
          Serial.println("SD playback stopped. Press 'm' to switch to web stream.");
        }
      }
    }
  }
 
}
