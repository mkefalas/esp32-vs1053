#include "RadioScreen.h"
#include "userUi.h"
#include <Arduino.h>

/**
 * @brief Main RadioScreen function
 * @return ScreenID Next screen to display
 */
ScreenID RadioScreen()
{
    Serial.println("RadioScreen: Entering radio parameter adjustment");

    // Switch to FM Radio mode first
//audioManager.switchAudioSource(SRC_RADIO,"");
    //audioManager.switchMusicSource(_SOURCE_FM_RADIO);
    // Initialize state variables using Setup values
    uint16_t lastFreq = Setup.lastFrequency;
    uint8_t lastRSSI = 0;
    uint8_t lastVolume = Setup.radioVolume;


    unsigned long lastRSSIUpdate = 0;

    // Reset brightness
    resetScreenBrightness();

    // Draw initial UI
    RadioScreenHelpers::drawStaticUI(&display);
    RadioScreenHelpers::updateFrequency(&display, lastFreq);
    RadioScreenHelpers::updateVolume(&display, lastVolume);
    RadioScreenHelpers::updateRSSI(&display, lastRSSI);

    Serial.printf("RadioScreen: Display initialized with frequency %.1f MHz, volume %d\n",
                  lastFreq / 100.0, lastVolume);

    // Main radio adjustment loop
    while (true)
    {
        // Check for exit combo first
        if (checkExitCombo())
        {
            Serial.println("RadioScreen: Exit combo detected, returning to main menu");
            return SCREEN_MAIN_MENU;
        }

        // Handle key input
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
 // Handle brightness timeout using common function
        handleScreenBrightness(key);
        // Handle key presses
        if (shouldProcessKeys && key != NONE)
        {
           

            // Handle long press for seek operations
            if ((key == NEXT) && CheckLongPress(200))
            {
                int newFreq = audioManager.radioSeekUp();
                lastFreq = newFreq;
                RadioScreenHelpers::updateFrequency(&display, lastFreq);
                Setup.lastFrequency = lastFreq;
                markSetupDirty();
                ClearNextUpKeys();
            }
            else if ((key == PREVIOUS) && CheckLongPress(200))
            {
                int newFreq = audioManager.radioSeekDown();
                lastFreq = newFreq;
                RadioScreenHelpers::updateFrequency(&display, lastFreq);
                Setup.lastFrequency = lastFreq;
                markSetupDirty();
                ClearNextUpKeys();
            }

            clearCmd();
        }

        // Handle key releases (step and volume control)
        if (shouldProcessKeys)
        {
            uint8_t upKeys = getUpKeys();
            if (upKeys != NONE)
            {
                if (upKeys == PREVIOUS)
                {
                    // Step down by 0.1 MHz
                    uint16_t currentFreq = audioManager.radioGetChannel();
                    uint16_t newFreq = currentFreq - 10;
                    if (newFreq < 8750)
                        newFreq = 10800; // Wrap around

                    audioManager.radioSetChannel(newFreq);
                    lastFreq = newFreq;
                    RadioScreenHelpers::updateFrequency(&display, lastFreq);
                    Setup.lastFrequency = lastFreq;
                    markSetupDirty();
                }
                else if (upKeys == NEXT)
                {
                    // Step up by 0.1 MHz
                    uint16_t currentFreq = audioManager.radioGetChannel();
                    uint16_t newFreq = currentFreq + 10;
                    if (newFreq > 10800)
                        newFreq = 8750; // Wrap around

                    audioManager.radioSetChannel(newFreq);
                    lastFreq = newFreq;
                    RadioScreenHelpers::updateFrequency(&display, lastFreq);
                    Setup.lastFrequency = lastFreq;
                    markSetupDirty();
                }
                else if (upKeys == UP)
                {
                    // Volume up
                    uint8_t vol = audioManager.radioGetVolume();
                    if (vol < 15)
                    {
                        audioManager.radioSetVolume(vol + 1);
                        lastVolume = vol + 1;
                        RadioScreenHelpers::updateVolume(&display, lastVolume);
                         audioManager.setCurrentSourceVolume( lastVolume);
                    }
                }
                else if (upKeys == DOWN)
                {
                    // Volume down
                    uint8_t vol = audioManager.radioGetVolume();
                    if (vol > 0)
                    {
                        audioManager.radioSetVolume(vol - 1);
                        lastVolume = vol - 1;
                        RadioScreenHelpers::updateVolume(&display, lastVolume);
                        audioManager.setCurrentSourceVolume( lastVolume);
                    }
                }

                clearUpKeys();
            }
        }

        // Update RSSI every second
        if (millis() - lastRSSIUpdate > 1000)
        {
            uint8_t rssi = audioManager.radioGetRSSI();
            if (rssi != lastRSSI)
            {
                lastRSSI = rssi;
                RadioScreenHelpers::updateRSSI(&display, lastRSSI);
            }
            lastRSSIUpdate = millis();
        }

    
        // Check setup dirty flag
        CheckSetupDirty();
        // Small delay to prevent excessive CPU usage
        delay(10);
    }
}

/**
 * @brief Helper Functions Implementation
 */

void RadioScreenHelpers::drawStaticUI(Adafruit_SSD1306 *display)
{
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);

    // Top: FM Tuner
    display->setTextSize(1);
    display->setCursor(36, 0);
    display->print("FM Tuner");
    display->drawFastHLine(0, 12, 128, SSD1306_WHITE);
    display->setCursor(105, 30);
    display->print("MHZ");
    display->drawFastHLine(0, 46, 128, SSD1306_WHITE);

    // Bottom: RSSI and Vol labels
    display->setCursor(0, 56);
    display->print("RSSI:");
    display->setCursor(63, 56);
    display->print("VOL:");
    display->display();
}

void RadioScreenHelpers::updateFrequency(Adafruit_SSD1306 *display, int freq)
{
    display->setFont(&FreeMonoBold12pt7b);
    display->fillRect(0, 22, 99, 16, SSD1306_BLACK);
    display->setCursor(12, 36);
    display->setTextColor(SSD1306_WHITE);
    display->printf("%03d.%02d", freq / 100, freq % 100);
    display->display();
}

void RadioScreenHelpers::updateRSSI(Adafruit_SSD1306 *display, uint8_t rssi)
{
    display->setFont();
    display->setTextSize(1);
    display->fillRect(34, 50, 26, 14, SSD1306_BLACK);
    display->setCursor(34, 56);
    display->setTextColor(SSD1306_WHITE);
    display->print(rssi);
    display->display();
}

void RadioScreenHelpers::updateVolume(Adafruit_SSD1306 *display, uint8_t volume)
{
    display->setFont(&FreeMonoBold9pt7b);
    display->fillRect(90, 50, 38, 18, SSD1306_BLACK);
    display->setCursor(90, 63);
    display->setTextColor(SSD1306_WHITE);
    display->printf("%02d%%", volume * 100 / 15);
    display->display();
}