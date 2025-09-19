#include "MainMenuScreen.h"
#include "../userUi.h"
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <pins.h>
#include <Wire.h>

// Menu items
typedef struct
{
    const char *name;
    ScreenID screenId;
} MenuItem;

const MenuItem menuItems[] = {
    {"Radio Tuner", SCREEN_RADIO_TUNE},
    {"Music Source", SCREEN_MUSIC_SOURCE},
    {"Web Radio", SCREEN_WEB_SELECT},
    {"Equalizer", SCREEN_EQUALIZER},
    {"Announcements", SCREEN_ANNOUNCEMENTS},
    {"System Setup", SCREEN_SYSTEM_SELECT}};

const int menuItemCount = sizeof(menuItems) / sizeof(MenuItem);

/**
 * @brief Main MenuScreen function
 * @return ScreenID Next screen to display
 */
ScreenID MainMenuScreen()
{
    Serial.println("MainMenuScreen: Entering main menu");

    // Initialize state variables
    int selectedItem = 0;

    // Draw initial UI and reset brightness
    MainMenuHelpers::drawMenu(&display, selectedItem);
    resetScreenBrightness();

    Serial.printf("MainMenuScreen: Menu initialized with %d items\n", menuItemCount);

    // Main menu loop
    while (true)
    {
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
                    // Move up in menu
                    selectedItem--;
                    if (selectedItem < 0)
                        selectedItem = menuItemCount - 1;
                    MainMenuHelpers::drawMenu(&display, selectedItem);
                }
                else if (upKeys == DOWN)
                {
                    // Move down in menu
                    selectedItem++;
                    if (selectedItem >= menuItemCount)
                        selectedItem = 0;
                    MainMenuHelpers::drawMenu(&display, selectedItem);
                }
                else if (upKeys == NEXT)
                {
                    // Select current item
                    Serial.printf("MainMenuScreen: Selected item %d: %s\n", selectedItem, menuItems[selectedItem].name);
                    return menuItems[selectedItem].screenId;
                }
                else if (upKeys == PREVIOUS)
                {
                    // Go to normal operation screen
                    Serial.println("MainMenuScreen: Going to normal operation screen");
                    return SCREEN_OPERATION;
                }

                clearUpKeys();
            }
        }

        // Handle brightness timeout using common function
        handleScreenBrightness(keyPressed);

        // Small delay to prevent excessive CPU usage
        delay(10);
    }
}


void MainMenuHelpers::drawMenu(Adafruit_SSD1306 *display, int selectedItem)
{
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    display->setTextSize(1);

    // Title
    display->setCursor(30, 0);
    display->print("MAIN MENU");
    display->drawFastHLine(0, 12, 128, SSD1306_WHITE);

    // Draw visible menu items (3 items max)
    int startItem = selectedItem - 1;
    if (startItem < 0)
        startItem = 0;
    if (startItem > menuItemCount - 3)
        startItem = menuItemCount - 3;
    if (startItem < 0)
        startItem = 0;

    for (int i = 0; i < 3 && (startItem + i) < menuItemCount; i++)
    {
        int itemIndex = startItem + i;
        int y = 20 + i * 12;

        // Highlight selected item
        if (itemIndex == selectedItem)
        {
            display->fillRect(0, y - 2, 128, 12, SSD1306_WHITE);
            display->setTextColor(SSD1306_BLACK);
            display->setCursor(2, y);
            display->printf("> %s", menuItems[itemIndex].name);
            display->setTextColor(SSD1306_WHITE);
        }
        else
        {
            display->setCursor(4, y);
            display->print(menuItems[itemIndex].name);
        }
    }

    // Instructions
    display->drawFastHLine(0, 52, 128, SSD1306_WHITE);
    display->setCursor(0, 56);
    display->print("UP/DN:Select NEXT:Go");

    display->display();
}

