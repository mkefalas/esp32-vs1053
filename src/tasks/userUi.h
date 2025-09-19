#pragma once

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include "AudioTask.h"
#include "keypadDriver.h"
#include "optoInDriver.h"
#include "setupDriver.h"

// Common display functions
void setDisplayBrightness(uint8_t brightness);
void applyDisplayRotation();
bool checkExitCombo();
void handleScreenBrightness(bool keyPressed);
void resetScreenBrightness();

// External references
extern Adafruit_SSD1306 display;

// Constants
const uint8_t NORMAL_BRIGHTNESS = 255;
const uint8_t DIM_BRIGHTNESS = 64;
const unsigned long BRIGHTNESS_TIMEOUT_MS = 30000; // 30 seconds

// Helper classes forward declarations
namespace MainMenuHelpers {
    void drawMenu(Adafruit_SSD1306* display, int selectedItem);
}

namespace OperationHelpers {
    void drawStatus(Adafruit_SSD1306* display, uint8_t source, uint8_t volume, uint8_t floor);
}

namespace RadioScreenHelpers {
    void drawStaticUI(Adafruit_SSD1306* display);
    void updateFrequency(Adafruit_SSD1306* display, int freq);
    void updateVolume(Adafruit_SSD1306* display, uint8_t volume);
    void updateRSSI(Adafruit_SSD1306* display, uint8_t rssi);
}

/**
 * @brief UserUI system for ESP32 Audio Player
 * 
 * Provides a menu-driven interface for controlling audio functions.
 * Uses screen-based navigation where each screen can return the next screen ID.
 */

// Screen IDs enumeration - used by all screen files for return values
enum ScreenID {
    SCREEN_MUSIC_SOURCE = 0,
    SCREEN_RADIO_TUNE,
    SCREEN_WEB_SELECT,
    SCREEN_EQUALIZER,
    SCREEN_ANNOUNCEMENTS,
    SCREEN_SYSTEM_SELECT,
    SCREEN_MEZZANINE,
    SCREEN_MUSIC_MODE,
    SCREEN_MUSIC_TIMEOUT,
    SCREEN_MUSIC_RESUME,
    SCREEN_ALARM_INPUTS,
    SCREEN_FILTER_FUNCTION,
    SCREEN_MAIN_MENU,
    SCREEN_OPERATION,
    SCREEN_COUNT,
    SCREEN_EXIT = 255  // Special value to exit completely
};

/**
 * @brief Start the screen manager task
 * 
 * Creates and starts the UserUI task that handles screen navigation
 * and user input processing.
 */
void startScreenManager();

/**
 * @brief Call the specified screen function
 * 
 * @param screenId The ID of the screen to be displayed
 * @return ScreenID The ID of the next screen to be displayed
 */
ScreenID callScreen(ScreenID screenId);

/**
 * @brief Radio screen function
 * 
 * @return ScreenID The ID of the next screen to be displayed
 */
ScreenID RadioScreen();

/**
 * @brief Main menu screen function
 * 
 * @return ScreenID The ID of the next screen to be displayed
 */
ScreenID MainMenuScreen();

/**
 * @brief Operation screen function
 * 
 * @return ScreenID The ID of the next screen to be displayed
 */
ScreenID OperationScreen();

/**
 * @brief MainMenuHelpers namespace
 * 
 * Contains helper functions and constants for the main menu screen.
 */
namespace MainMenuHelpers {
    const uint8_t NORMAL_BRIGHTNESS = 255;
    const uint8_t DIM_BRIGHTNESS = 64;
    const unsigned long BRIGHTNESS_TIMEOUT_MS = 30000; // 30 seconds
    
    void initDisplay(Adafruit_SSD1306* display);
    void drawMenu(Adafruit_SSD1306* display, int selectedItem);
    void setDisplayBrightness(Adafruit_SSD1306* display, uint8_t brightness);
}

/**
 * @brief OperationHelpers namespace
 * 
 * Contains helper functions and constants for the operation screen.
 */
namespace OperationHelpers {
    const uint8_t NORMAL_BRIGHTNESS = 255;
    const uint8_t DIM_BRIGHTNESS = 64;
    const unsigned long BRIGHTNESS_TIMEOUT_MS = 30000; // 30 seconds
    
    void initDisplay(Adafruit_SSD1306* display);
    void drawStatus(Adafruit_SSD1306* display, uint8_t source, uint8_t volume, uint8_t floor);
    void setDisplayBrightness(Adafruit_SSD1306* display, uint8_t brightness);
    bool checkExitCombo();
}
