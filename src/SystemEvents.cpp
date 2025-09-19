#include "SystemEvents.h"

// Global event group
EventGroupHandle_t audioEvents = nullptr;

// Global audio status
AudioStatus currentAudioStatus = {"", "", 0, false};

void initSystemEvents() {
    audioEvents = xEventGroupCreate();
    if (audioEvents == nullptr) {
        Serial.println("SystemEvents: Failed to create event group");
    } else {
        Serial.println("SystemEvents: Event group created successfully");
    }
}
