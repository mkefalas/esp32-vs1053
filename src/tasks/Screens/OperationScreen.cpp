#include "../userUi.h"

#include "../AppDrivers/setupDriver.h"
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <pins.h>
#include <Wire.h>

/**
 * @brief Main OperationScreen function - shows current music source status
 * @return ScreenID Next screen to display
 */
ScreenID OperationScreen()
{
    Serial.println("OperationScreen: Entering normal operation display");

    // Start playback for current audio source
   // audioManager.switchAudioSource(Setup.currentMusicSource,audioManager.getCurrentSourceString());                // Get current source content from Setup);

    // Initialize state variables
    uint8_t lastSource = 255; // Force initial update
    uint8_t lastVolume = 90;
    uint8_t lastFloor = 255;
    String lastStatus = "";
    unsigned long lastUpdate = 0;

    // Reset brightness
    resetScreenBrightness();

    Serial.println("OperationScreen: Starting status monitoring");

    // Main operation monitoring loop
    while (true)
    {
        // Check for exit to main menu (Button 1 long press or PREV+NEXT)
        if (checkExitCombo() || CheckLongPress(1000))
        {
            Serial.println("OperationScreen: Exit detected, going to main menu");
            // Remove audioManager.saveSetup() - not needed for screen transitions
            return SCREEN_MAIN_MENU;
        }

        // Handle key input for volume control
        static unsigned long lastDebounce = 0;
        static uint8_t lastKey = NONE;

        uint8_t key = getKeypadState();

        // Debounce
        if (key != lastKey)
        {
            lastDebounce = millis();
            lastKey = key;
        }

        bool shouldProcessKeys = (millis() - lastDebounce >= 50);
        bool keyPressed = false;

        // Handle key releases
        if (shouldProcessKeys)
        {
            uint8_t upKeys = getUpKeys();
            if (upKeys != NONE)
            {
                keyPressed = true;

                if (upKeys == UP)
                {
                    if (audioManager.getCurrentSourceVolume() < 100)
                    {
                        audioManager.setCurrentSourceVolume(audioManager.getCurrentSourceVolume() + 5);
                        // markSetupDirty() is called inside sdSetVolume()
                    }
                }
            }
            else if (upKeys == DOWN)
            {
                if (audioManager.getCurrentSourceVolume() > 0)
                {
                    audioManager.setCurrentSourceVolume(audioManager.getCurrentSourceVolume() - 5);
                    // markSetupDirty() is called inside sdSetVolume()
                }   
            }

            clearUpKeys();
        }

    // Handle brightness timeout using common function
    handleScreenBrightness(keyPressed);

    // Update display every 500ms or when values change
    if (millis() - lastUpdate > 500)
    {
        bool needsUpdate = false;

        if (Setup.currentMusicSource != lastSource)
        {
            lastSource = Setup.currentMusicSource;
            needsUpdate = true;
        }

        uint8_t currentVolume = audioManager.getCurrentSourceVolume();
        if (currentVolume != lastVolume)
        {
            lastVolume = currentVolume;
            needsUpdate = true;
        }

        if (Setup.baseFloor != lastFloor)
        {
            lastFloor = Setup.baseFloor;
            needsUpdate = true;
        }

        if (needsUpdate)
        {
            OperationHelpers::drawStatus(&display, lastSource, lastVolume, lastFloor);
        }

        lastUpdate = millis();
    }

    // Check setup dirty flag
    CheckSetupDirty();

    // Small delay to prevent excessive CPU usage
    delay(10);
}
}

void OperationHelpers::drawStatus(Adafruit_SSD1306 *display, uint8_t source, uint8_t volume, uint8_t floor)
{
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    display->setTextSize(1);

    // Title
    display->setCursor(25, 0);
    display->print("MUSIC PLAYER");
    display->drawFastHLine(0, 12, 128, SSD1306_WHITE);

    // Current source
    display->setCursor(0, 18);
    display->print("Source: ");
    switch (source)
    {
    case 0:
        display->printf("FM %.1f MHz", Setup.lastFrequency / 100.0);
        break;
    case 1:
        display->print("Web Radio");
        break;
    case 2:
        display->print("SD Card");
        break;
    default:
        display->print("Unknown");
        break;
    }

    // Volume
    display->setCursor(0, 30);
    if (source == 0)
    {
        display->printf("Volume: %d%% (0-15)", (volume * 100) / 15);
    }
    else
    {
        display->printf("Volume: %d%% (0-100)", volume);
    }

    // Floor number
    display->setCursor(0, 42);
    display->printf("Floor: %d", floor);

    // Music state
    display->setCursor(70, 42);
    display->print("READY");

    // Instructions
    display->drawFastHLine(0, 52, 128, SSD1306_WHITE);
    display->setCursor(0, 56);
    display->print("UP/DN:Vol HOLD:Menu");

    display->display();
}
