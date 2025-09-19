#include "AudioTask.h"
#include "SD_MMC.h"
#include "pins.h"
#include <Arduino.h>
#include <SPI.h>
#include <VS1053.h>

VS1053 player(VS1003B_CS_PIN, VS1003B_DCS_PIN, VS1003B_DREQ_PIN);

struct TrackState
{
    String filename;
    uint32_t pos;
};
TrackState tracks[2] = {{"/M1.mp3", 0}, {"/test2.wav", 0}};

int currentTrack = 0;
bool isPaused = false;
File mp3file;

const size_t REWIND_BYTES = 1024; // Only used for MP3

void playTrack(int trackNum)
{
    if (mp3file) mp3file.close();
    player.softReset(); // <--- add this line
    mp3file = SD_MMC.open(tracks[trackNum].filename);
    if (!mp3file)
    {
        Serial.println("Cannot open file: " + tracks[trackNum].filename);
        return;
    }
    uint32_t startPos = tracks[trackNum].pos;
    if (!isWavFile(tracks[trackNum].filename))
    {
        if (startPos > REWIND_BYTES) startPos -= REWIND_BYTES;
        else startPos = 0;
    }
    mp3file.seek(startPos);
    Serial.printf("Playing %s from byte %lu %s\n", tracks[trackNum].filename.c_str(), startPos,
                  isWavFile(tracks[trackNum].filename) ? "(WAV, no rewind)" : "(MP3, rewound)");
    isPaused = false;
}

// --- Function to pause, saving current position ---
void pauseTrack()
{
    if (mp3file)
    {
        tracks[currentTrack].pos = mp3file.position();
        mp3file.close();
        isPaused = true;
        Serial.printf("Paused at byte %lu\n", tracks[currentTrack].pos);
    }
}

// --- Function to switch track (remembers position) ---
void switchTrack()
{
    pauseTrack();
    currentTrack = (currentTrack + 1) % 2;
    playTrack(currentTrack);
}

// --- Setup ---
void setup()
{
    Serial.begin(115200);
    delay(1000);

    pinMode(VS1003B_RST_PIN, OUTPUT);
    digitalWrite(VS1003B_RST_PIN, LOW);
    delay(100);
    digitalWrite(VS1003B_RST_PIN, HIGH);
    delay(100);

    SPI.begin(VS1003B_CLK_PIN, VS1003B_MISO_PIN, VS1003B_MOSI_PIN);
    SD_MMC.setPins(PIN_SD_MMC_CLK, PIN_SD_MMC_CMD, PIN_SD_MMC_D0);
    delay(10);

    if (!SD_MMC.begin("/sdmmc", true, false, 20000))
    {
        Serial.println("Card Mount Failed");
        while (1)
            ;
    }
    Serial.println("SD Card initialized.");

    player.begin();
    player.setVolume(95);

    Serial.println("\nCommands:\n  p=play/resume, s=switch, q=pause\n");
}

void loop()
{
    if (Serial.available())
    {
        char c = Serial.read();
        if (c == 'p')
        {
            playTrack(currentTrack);
        }
        if (c == 'q')
        {
            pauseTrack();
        }
        if (c == 's')
        {
            switchTrack();
        }
    }

    if (mp3file && !isPaused)
    {
        uint8_t buf[32];
        if (digitalRead(VS1003B_DREQ_PIN))
        {
            int n = mp3file.read(buf, sizeof(buf));
            if (n > 0)
            {
                player.playChunk(buf, n);
            }
            else
            {
                mp3file.close();
                Serial.println("Track finished");
                tracks[currentTrack].pos = 0;
                isPaused = true;

                // If WAV finished, always go back to MP3 (track 0)
                if (isWavFile(tracks[currentTrack].filename))
                {
                    Serial.println("WAV finished, auto-playing MP3...");
                    currentTrack = 0; // Always MP3 track (index 0)
                    playTrack(currentTrack);
                }
            }
        }
    }
}
