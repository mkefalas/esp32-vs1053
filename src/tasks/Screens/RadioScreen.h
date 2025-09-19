#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "../userUi.h"  // For ScreenID enumeration
#include "../../../AudioTask.h" // For audioManager object

/**
 * @brief RadioScreen function for FM radio parameter adjustment
 * 
 * Called from UserUI when user wants to adjust radio parameters.
 * Runs its own loop until user presses PREVIOUS+NEXT to exit.
 * Uses global audioManager object declared in main.cpp.
 * 
 * @return ScreenID Next screen to display (determined by user input)
 */
ScreenID RadioScreen();

// Helper functions (can be used internally by RadioScreen)
namespace RadioScreenHelpers {
    void drawStaticUI(Adafruit_SSD1306* display);
    void updateFrequency(Adafruit_SSD1306* display, int freq);
    void updateRSSI(Adafruit_SSD1306* display, uint8_t rssi);
    void updateVolume(Adafruit_SSD1306* display, uint8_t volume);
    
    // Note: initDisplay, setDisplayBrightness, and checkExitCombo are now in userUI.cpp
}
