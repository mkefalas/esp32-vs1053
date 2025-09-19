#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

// Global event group handle
extern EventGroupHandle_t audioEvents;

// Event bits
#define AUDIO_EVENT_NEW_SONG_PLAYING  (1 << 0)

// Shared audio status structure
struct AudioStatus {
    String currentFile;
    String currentTitle;
    uint8_t currentSource;  // 0=FM, 1=WEB, 2=SD
    bool isPlaying;
};

extern AudioStatus currentAudioStatus;

// Initialize the event system
void initSystemEvents();
