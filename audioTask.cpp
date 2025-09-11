#include "AudioManager.h"
#include <Adafruit_VS1053.h>
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>


// ---------------- VS1053 + SD pins ----------------
#define VS1053_CS 6
#define VS1053_DCS 7
#define VS1053_DREQ 3
#define CARDCS 4

// ---------------- Control pins (NORMAL mode) ------
#define PIN_PLAYCONTROL 5 // HIGH = play, LOW = pause/stop
#define PIN_INTERRUPT 8   // Positive edge plays selected SPIFFS announcement

// --------------- Globals --------------------------
Adafruit_VS1053_FilePlayer musicPlayer(VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);
AudioManager audio(musicPlayer);

// NORMAL mode pin-state helpers
static bool wasPlaying = false;
static int lastInterruptState = LOW;
static int resumeMode = 0; // 0=resume (SD), 1=next track (SD)

// Announcement selection (set by your UI/task)
static String annFolderSPIFFS = "/announcements/en";
static String annFileSPIFFS = "welcome.mp3"; // default
static String annFolderSD = "/announcements/en";
static String annFileSD = "welcome.mp3"; // for TEST mode SD testing

// ---------------- Setters your tasks can call -----
void setControlMode(ControlMode mode)
{
    audio.setControlMode(mode);
}
void setSource(SourceMode src)
{
    audio.setSource(src);
}

// SD
bool playSdFile(const String &path)
{
    audio.setSource(SOURCE_SD);
    return audio.playFile(path.c_str());
}

// Web
void setWebUrl(const String &url)
{
    audio.setWebStream(url.c_str());
}
void startWeb()
{
    audio.setSource(SOURCE_WEB);
    audio.startWebStream();
}
void stopWeb()
{
    audio.stopWebStream();
}

// Radio (your tuner should be configured elsewhere)
void enterRadioMode()
{
    audio.setSource(SOURCE_RADIO);
} // tuner config not shown

// Announcement selection (called by UI/task)
void setAnnouncementSPIFFS(const String &folder, const String &file)
{
    annFolderSPIFFS = folder;
    annFileSPIFFS = file;
}
void setAnnouncementSD(const String &folder, const String &file)
{
    annFolderSD = folder;
    annFileSD = file;
}

// ---------------- Announcement helpers -------------
// Play announcement from SPIFFS (pause current source, play, then resume)
void playAnnouncementFromSPIFFS_Selected()
{
#if defined(ARDUINO_ARCH_ESP32)
    String fullPath = annFolderSPIFFS + "/" + annFileSPIFFS;
    File f = SPIFFS.open(fullPath, "r");
    if (!f) return;

    // Pause current audio source
    audio.pause();

    // Feed SPIFFS file directly to VS1053
    while (f.available())
    {
        if (musicPlayer.readyForData())
        {
            uint8_t buf[128];
            size_t n = f.read(buf, sizeof(buf));
            if (n > 0) musicPlayer.sdiWrite(buf, n);
        }
        else
        {
            delay(1);
        }
    }
    f.close();

    // Resume previous source
    audio.resume();
#endif
}

// TEST mode helper to play announcement from SD selection
void playAnnouncementFromSD_Selected()
{
    String fullPath = annFolderSD + "/" + annFileSD;
    // Pause, then play using SD feeding
    audio.pause();
    if (audio.playFile(fullPath.c_str()))
    {
        // Let loop() feed SD stream until EOF, then your task can resume prior content if needed
    }
    else
    {
        // If file missing, just resume
        audio.resume();
    }
}

// External function to create the audio task
void createAudioTask(QueueHandle_t commandQueue)
{
    BaseType_t result = xTaskCreate(
        audioTask,    // Task function
        "AudioTask",  // Task name
        4096,         // Stack size (8KB)
        NULL, // Parameter (command queue)
        2,            // Priority
        NULL          // Task handle
    );

    if (result == pdPASS)
    {
        Serial.println("AudioTask: Task created successfully");
    }
    else
    {
        Serial.println("AudioTask: Failed to create task");
    }
}

// ---------------- Setup ---------------------------
void audioTask()
{
    // No serial/UI here; keep core clean
#if defined(ARDUINO_ARCH_ESP32)
    SPIFFS.begin(true);
#endif

    pinMode(PIN_PLAYCONTROL, INPUT_PULLUP);
    pinMode(PIN_INTERRUPT, INPUT_PULLUP);

    SD.begin(CARDCS);

    musicPlayer.begin();
    musicPlayer.setVolume(20, 20);
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);

    // Optional: preload SD playlist
    audio.scanRootFolder();
    if (!audio.playlist.empty())
    {
        audio.setSource(SOURCE_SD);
        audio.playFile(audio.playlist[0].c_str());
        wasPlaying = true;
    }

// ---------------- Loop ----------------------------
do
{
    const ControlMode cMode = audio.getControlMode();
    const SourceMode sMode = audio.getSource();

    if (cMode == CONTROL_NORMAL)
    {
        // --- GPIO-driven control ---
        int playState = digitalRead(PIN_PLAYCONTROL);
        int interruptState = digitalRead(PIN_INTERRUPT);

        // Play/Stop via play pin
        if (playState == HIGH && !wasPlaying)
        {
            if (sMode == SOURCE_SD)
            {
                if (resumeMode == 0) audio.resume();
                else audio.playNext();
            }
            else if (sMode == SOURCE_WEB)
            {
                audio.startWebStream();
            }
            else if (sMode == SOURCE_LINEIN || sMode == SOURCE_RADIO)
            {
                audio.setSource(sMode); // ensure passthrough enabled
            }
            wasPlaying = true;
        }
        if (playState == LOW && wasPlaying)
        {
            audio.pause();
            wasPlaying = false;
        }

        // Positive-edge announcement from SPIFFS
        if (lastInterruptState == LOW && interruptState == HIGH)
        {
            // Only resume if play pin remains HIGH after the announcement
            bool resumeAfter = (playState == HIGH);
            playAnnouncementFromSPIFFS_Selected();
            if (!resumeAfter)
            {
                audio.pause();
                wasPlaying = false;
            }
        }
        lastInterruptState = interruptState;
    }
    else
    {
        // CONTROL_TEST: ignore GPIO entirely.
        // Your tasks should call playSdFile(), startWeb(), enterRadioMode(),
        // playAnnouncementFromSPIFFS_Selected(), or playAnnouncementFromSD_Selected()
        // as appropriate.
    }

    // Feed audio based on source and mode
    if (cMode == CONTROL_TEST || wasPlaying)
    {
        if (sMode == SOURCE_SD)
        {
            audio.resumeStream();
        }
        else if (sMode == SOURCE_WEB)
        {
            audio.feedWebStream();
        }
        else
        {
            // SOURCE_LINEIN and SOURCE_RADIO are handled by hardware/tuner path
        }
    }

    delay(2);
} (while(1));
}
