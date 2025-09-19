#include "keypadDriver.h"
#include "optoInDriver.h"
#include "setupDriver.h"
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <SI470X.h>
#include <pins.h>

// Display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// SI4703 setup
SI470X radio;
#define EEPROM_FREQ_ADDR 0

// UI state
enum Line
{
    SEEK_LINE,
    TUNE_LINE
};
Line selectedLine = SEEK_LINE;
bool lineBlink = false;
bool inEditMode = false;
unsigned long lastBlink = 0;
bool blinkState = false;
unsigned long lastRSSIUpdate = 0;
uint16_t lastFreq = 0;
uint8_t lastRSSI = 0;

#define NORMAL_BRIGHTNESS 0xFF
#define DIM_BRIGHTNESS 0x01
#define BRIGHTNESS_TIMEOUT_MS (5UL * 60UL * 1000UL) // 1 hour in ms

unsigned long lastKeyTime = 0;
bool isDimmed = false;

void my_1ms_task(void *pvParameters)
{
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t frequency = 1; // 1 tick = 1ms (configTICK_RATE_HZ=1000)

    while (1)
    {
        // --- Your 1ms code here ---
        // Example: toggle a pin, update a variable, etc.
        // digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        // --------------------------
        keypadRead();
        updateOptoEvents();
        vTaskDelayUntil(&lastWakeTime, frequency);
    }
}

void drawStaticUI()
{
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Top: FM Tuner
    display.setTextSize(1);
    display.setCursor(36, 0);
    display.print("FM Tuner");
    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
    display.setCursor(105, 30);
    display.print("MHZ");
    display.drawFastHLine(0, 46, 128, SSD1306_WHITE);
    // Bottom: RSSI and Vol labels
    display.setCursor(0, 56);
    display.print("RSSI:");
    display.setCursor(63, 56);
    display.print("VOL:");
    display.display();
}

void updateFrequency(int freq)
{
    display.setFont(&FreeMonoBold12pt7b);
    display.fillRect(0, 22, 99, 16, SSD1306_BLACK);
    display.setCursor(12, 36);
    display.setTextColor(SSD1306_WHITE);
    display.printf("%03d.%02d", freq / 100, freq % 100);
    display.display();
}

void updateRSSI(uint8_t rssi)
{
    display.setFont();
    display.setTextSize(1);
    // display.setFont(&FreeMonoBold9pt7b); // Use a smaller, clean font
    display.setTextSize(1);
    display.fillRect(34, 50, 26, 14, SSD1306_BLACK); // Clear old RSSI value
    display.setCursor(34, 56);
    display.setTextColor(SSD1306_WHITE);
    display.print(rssi);
    display.display();
}

void updateVolume(uint8_t volume)
{
    display.setFont(&FreeMonoBold9pt7b);
    // display.setFont(&FreeMonoBold9pt7b); // Use a smaller, clean font
    // display.setTextSize(1);
    display.fillRect(90, 50, 38, 18, SSD1306_BLACK); // Clear old Vol value
    display.setCursor(90, 63);
    display.setTextColor(SSD1306_WHITE);
    display.printf("%02d%%", volume * 100 / 15);
    display.display();
}

volatile uint8_t lastVolume;

void setDisplayBrightness(uint8_t val)
{
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(val);
    Serial.printf("bright:%03d\n", val);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Setup start");
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
    InitSetup();
    keypadInit();
    scanOptosInit(30); // threwshold level after which opto is considered ON
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    // Hardware 180� rotation (no software coordinate changes)
    display.ssd1306_command(SSD1306_SEGREMAP);   // Reverse left-right mapping
    display.ssd1306_command(SSD1306_COMSCANINC); // Reverse up-down mapping
    display.clearDisplay();

    radio.setup(RST_PIN, PIN_I2C_SDA);
    pinMode(TDA7052_SHUTDOWN_PIN, OUTPUT);
    digitalWrite(TDA7052_SHUTDOWN_PIN, LOW);
    delay(100);

    // Restore last frequency from EEPROM
    uint16_t freq = Setup.lastFrequency;
    // if (freq < 8700 || freq > 10800) freq = 10110; // Default 101.1 MHz
    radio.setFrequency(freq);
    lastFreq = freq;
    radio.setMono(true); // Set stereo mode
    radio.setRds(false); // turn the RDS decoder ON
    lastRSSI = radio.getRssi();
    drawStaticUI();
    // Restore last volume from EEPROM
    // uint8_t vol = EEPROM.readByte(EEPROM_FREQ_ADDR + 2);
    uint8_t vol = Setup.volume;
    // if (vol > 15) vol = 2; // Default volume if out of range
    radio.setVolume(vol);
    lastVolume = vol;
    //
    // pinMode(41, OUTPUT); did to checkirq duty cycle

    updateFrequency(lastFreq);
    updateRSSI(lastRSSI);
    updateVolume(lastVolume);
    // Create the 1ms task at higher priority than loopTask (default loopTask priority is 1)
    xTaskCreatePinnedToCore(my_1ms_task, // Task function
                            "1msTask",   // Name
                            2048,        // Stack size
                            NULL,        // Parameters
                            2,           // Priority (higher than loop)
                            NULL,        // Task handle
                            1            // Core 1 (same as Arduino)
    );
}

void loop()
{
    static unsigned long lastDebounce = 0;
    static Key lastKey = NONE;
    static bool LastOnOffState=1;
    
    //
    CheckSetupDirty();
    //
    Key key = getKeypadState();
    // Debounce
    if (key != lastKey)
    {
        lastDebounce = millis();
        lastKey = key;
    }
    if (millis() - lastDebounce < 50) return;
    if (key != NONE)
    {
        lastKeyTime = millis();
        if (isDimmed)
        {
            setDisplayBrightness(NORMAL_BRIGHTNESS);
            isDimmed = false;
        }
        if (key == NEXT)
        {
            radio.seek(SI470X_SEEK_WRAP, SI470X_SEEK_UP);
            lastFreq = radio.getFrequency();
            updateFrequency(lastFreq);
            Setup.lastFrequency = lastFreq;
            markSetupDirty();
        }
        if (key == PREVIOUS)
        {
            radio.seek(SI470X_SEEK_WRAP, SI470X_SEEK_DOWN);
            lastFreq = radio.getFrequency();
            updateFrequency(lastFreq);
            Setup.lastFrequency = lastFreq;
            markSetupDirty();
        }
        if (key == UP)
        {
            uint8_t vol = radio.getVolume();
            if (vol < 15)
            {
                radio.setVolume(vol + 1);
                lastVolume = vol + 1;
                updateVolume(lastVolume);
                Setup.volume = lastVolume;
                markSetupDirty();
            }
        }
        if (key == DOWN)
        {
            uint8_t vol = radio.getVolume();
            if (vol > 0)
            {
                radio.setVolume(vol - 1);
                lastVolume = vol - 1;
                updateVolume(lastVolume);
                Setup.volume = lastVolume;
                markSetupDirty();
            }
        }
    }
    // scan optos for memory functionality 
    int32_t tmp = getOptoEventFlag();

        KeyEventType tmpkeyEvent = getOptoEvent(tmp);
        if (tmpkeyEvent == KEY_EVENT_RELEASE)
        {
            radio.setFrequency(Setup.freqMemories[tmp]);
            lastFreq = radio.getFrequency();
            updateFrequency(lastFreq);
            Setup.lastFrequency = lastFreq;
            markSetupDirty();
        }
        else if (tmpkeyEvent == KEY_EVENT_LONGPRESS)
        {
            Setup.freqMemories[tmp] = radio.getFrequency();
            markSetupDirty();
        }
        delay(200); // Simple debounce for key repeat
    // check radio on/off
    {
        uint16_t tmp2=(getOptoInFiltered())&0x01;
        if (tmp2!=LastOnOffState)
        {
           LastOnOffState=(tmp2&0x01); 
           if (tmp2) radio.setMute(false);
           else radio.setMute(true);
        }
    }
    if (!isDimmed && (millis() - lastKeyTime > BRIGHTNESS_TIMEOUT_MS))
    {
        setDisplayBrightness(DIM_BRIGHTNESS);
        isDimmed = true;
    }
    // RSSI update every second
    if (millis() - lastRSSIUpdate > 1000)
    {
        uint8_t rssi = radio.getRssi();
        if (rssi != lastRSSI)
        {
            lastRSSI = rssi;
            updateRSSI(lastRSSI);
        }
        // Serial.printf("Optos:%04x \n", getOptoInFiltered());
        lastRSSIUpdate = millis();
    }
}