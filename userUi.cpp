#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "userUi.h"
#include "Screens/RadioScreen.h"
#include "Screens/MainMenuScreen.h"
#include "Screens/OperationScreen.h"
#include "../AppDrivers/setupDriver.h"

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Global objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ScreenID currentScreen = SCREEN_OPERATION; // Start with operation screen

// Reference to global AudioManager object (declared in main.cpp)

// Global brightness state
static unsigned long globalLastKeyTime = 0;
static bool globalIsDimmed = false;

// Function prototypes
void initDisplay();
void uiTask(void *parameter);
ScreenID callScreen(ScreenID screenId);

// Common display helper functions
void setDisplayBrightness(uint8_t brightness)
{
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(brightness);
}

void applyDisplayRotation()
{
    display.ssd1306_command(SSD1306_SEGREMAP);
    display.ssd1306_command(SSD1306_COMSCANINC);
}

bool checkExitCombo()
{
    uint8_t key = getKeypadState();
    return ((key & PREVIOUS) && (key & NEXT));
}

/**
 * @brief Handle screen brightness timeout - call this in all screen loops
 * @param keyPressed True if any key was pressed this cycle
 */
void handleScreenBrightness(bool keyPressed)
{
    if (keyPressed)
    {
        globalLastKeyTime = millis();
        if (globalIsDimmed)
        {
            setDisplayBrightness(NORMAL_BRIGHTNESS);
            globalIsDimmed = false;
        }
    }
    else
    {
        // Handle brightness timeout
        if (!globalIsDimmed && (millis() - globalLastKeyTime > BRIGHTNESS_TIMEOUT_MS))
        {
            setDisplayBrightness(DIM_BRIGHTNESS);
            globalIsDimmed = true;
        }
    }
}

/**
 * @brief Reset brightness to normal (call when entering a screen)
 */
void resetScreenBrightness()
{
    globalLastKeyTime = millis();
    globalIsDimmed = false;
    setDisplayBrightness(NORMAL_BRIGHTNESS);
}

void initDisplay()
{
    // I2C bus should already be initialized in main.cpp
    // Wire.begin() is not called here to avoid conflicts with FM radio

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println("UserUI: SSD1306 allocation failed");
        return;
    }

    // Apply hardware rotation
    applyDisplayRotation();

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.display();
    Serial.println("UserUI: Display initialized (I2C bus shared with FM radio)");
}

ScreenID callScreen(ScreenID screenId)
{
    Serial.printf("UserUI: Calling screen %d\n", screenId);

    switch (screenId)
    {
    case SCREEN_RADIO_TUNE:
        return RadioScreen();

    case SCREEN_MAIN_MENU:
        return MainMenuScreen();

    case SCREEN_OPERATION:
        return OperationScreen();

    case SCREEN_MUSIC_SOURCE:
    case SCREEN_WEB_SELECT:
    case SCREEN_EQUALIZER:
    case SCREEN_ANNOUNCEMENTS:
    case SCREEN_SYSTEM_SELECT:
    case SCREEN_MEZZANINE:
    case SCREEN_MUSIC_MODE:
    case SCREEN_MUSIC_TIMEOUT:
    case SCREEN_MUSIC_RESUME:
    case SCREEN_ALARM_INPUTS:
    case SCREEN_FILTER_FUNCTION:
        // These screens are not implemented yet
        Serial.printf("UserUI: Screen %d not implemented yet\n", screenId);
        // Return to main menu after a short delay
        vTaskDelay(pdMS_TO_TICKS(1000));
        return SCREEN_MAIN_MENU;

    default:
        Serial.printf("UserUI: Unknown screen %d, going to operation screen\n", screenId);
        return SCREEN_OPERATION;
    }
}

void uiTask(void *parameter)
{
    Serial.println("UserUI: Starting UI task");

    initDisplay();

    Serial.println("UserUI: Display initialized, starting screen flow");

    // Wait for AudioManager to complete initialization and activate saved music source
    vTaskDelay(pdMS_TO_TICKS(2000));

    Serial.printf("UserUI: Starting with current music source: %d\n", Setup.currentMusicSource);

    // Simple screen flow manager - just calls screens and follows return values
    while (true)
    {
        ScreenID nextScreen = callScreen(currentScreen);
        currentScreen = nextScreen;

        // Small delay to prevent excessive CPU usage
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void startScreenManager()
{
    xTaskCreate(
        uiTask,
        "UI_Task",
        4096,
        NULL,
        1,
        NULL);
}
