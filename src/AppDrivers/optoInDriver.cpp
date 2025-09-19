#define WINDOW 128
#include <Arduino.h>
#include "soc/gpio_struct.h"
#include "pins.h"
#include "optoInDriver.h"

uint16_t sample_buffer[WINDOW];
uint8_t acc[NUM_INPUTS] = {0};
uint8_t pos = 0;
uint16_t optoInFiltered = 0;
uint8_t THRESHOLD = 35;

#define LONG_PRESS_TICKS 300 // 3 seconds win 10ms every loop
#define DOUBLE_PRESS_TICKS 30

static uint16_t prevOptoInFiltered = 0;
static uint16_t longPressCounter[NUM_INPUTS] = {0};
static uint16_t doublePressCounter[NUM_INPUTS] = {0};
static uint8_t doublePressPending[NUM_INPUTS] = {0};
static KeyEventType KeyEventTypeArr[NUM_INPUTS] = {KEY_EVENT_NONE};
static uint16_t KeyEventFlag = 0;

/**
 * @brief Initialize opto input scanning hardware and buffers.
 * @param threshold The threshold value for input filtering.
 */
void scanOptosInit(uint8_t threshold)
{
    // Set all row pins HIGH, then set row 0 LOW to start
    pinMode(selOptoRow1, OUTPUT_OPEN_DRAIN);
    pinMode(selOptoRow2, OUTPUT_OPEN_DRAIN);
    gpio_set_level(selOptoRow2, 1);
    gpio_set_level(selOptoRow1, 0);
    pinMode(optoIn1, INPUT_PULLUP);
    pinMode(optoIn2, INPUT_PULLUP);
    pinMode(optoIn3, INPUT_PULLUP);
    pinMode(optoIn4, INPUT_PULLUP);
    pinMode(optoIn5, INPUT_PULLUP);
    pinMode(optoIn6, INPUT_PULLUP);
    pinMode(optoIn7, INPUT_PULLUP);

    // Clear accumulators and sample buffer
    for (int i = 0; i < NUM_INPUTS; i++)
        acc[i] = 0;
    for (int i = 0; i < WINDOW; i++)
        sample_buffer[i] = 0;

    pos = 0;
    optoInFiltered = 0;
    THRESHOLD = threshold;
}

/**
 * @brief Scan opto inputs and update filtered values.
 *        Should be called periodically.
 */
void scanOptos(void)
{
    static uint8_t current_row = 0;
    static uint16_t optoIn_assembled = 0;
    static uint8_t optoInLo = 0, optoInHi = 0;
    // --- 2. Prepare next row: set all HIGH, then next row LOW ---
    // --- 2. Prepare next row: set both HIGH, then next row LOW ---
    if (current_row == 0)
    {
        optoInLo = (GPIO.in >> 4) & 0x7F;
        gpio_set_level(selOptoRow1, 1);
        gpio_set_level(selOptoRow2, 0);
    }
    else
    {
        optoInHi = (GPIO.in >> 4) & 0x7F;
        gpio_set_level(selOptoRow1, 0);
        gpio_set_level(selOptoRow2, 1);
    }
    current_row++;
    if (current_row >= 2)
    {
        optoIn_assembled = ((optoInHi << 7) + optoInLo) ^ 0x3fff;
        // All rows scanned, process the 16-bit word
        uint16_t old_sample = sample_buffer[pos];
        sample_buffer[pos] = optoIn_assembled;

        for (int i = 0; i < NUM_INPUTS; i++)
        {
            acc[i] -= (old_sample >> i) & 1;
            acc[i] += (optoIn_assembled >> i) & 1;
            if (acc[i] >= THRESHOLD)
                optoInFiltered |= (1 << i);
            else
                optoInFiltered &= ~(1 << i);
        }
        pos++;
        if (pos >= WINDOW)
            pos = 0;
        optoIn_assembled = 0; // Reset for next scan cycle
        current_row = 0;
    }
}

/**
 * @brief Get the current filtered opto input value.
 * @return Filtered opto input as a bitmask.
 */
uint16_t getOptoInFiltered(void)
{
    return optoInFiltered;
}

/**
 * @brief Update key event states for opto inputs (press, release, longpress).
 *        Should be called periodically after scanOptos().
 */
void updateOptoEvents()
{
    static uint8_t cnt = 0;
    static bool longPressDetected[NUM_INPUTS] = {0};  // Prevents duplicate longpresses
 scanOptos();

    if (cnt < 10)
    {
        cnt++;
        return;
    } // ~30ms debounce
    cnt = 0;
//2 is since 0,1 we use for on/off
    for (int i = 2; i < NUM_INPUTS; i++)
    {
        bool prev = (prevOptoInFiltered >> i) & 1;
        bool curr = (optoInFiltered >> i) & 1;

        // Just pressed (rising edge)
        if (!prev && curr)
        {
            KeyEventTypeArr[i] = KEY_EVENT_PRESS;
            KeyEventFlag |= (1 << i);
            longPressCounter[i] = 0;
            longPressDetected[i] = false;
        }
        // Just released (falling edge)
        else if (prev && !curr)
        {
            KeyEventTypeArr[i] = KEY_EVENT_RELEASE;
            KeyEventFlag |= (1 << i);
            longPressCounter[i] = 0;
            longPressDetected[i] = false;
        }
        // Still pressed
        else if (curr)
        {
            if (!longPressDetected[i])
            {
                longPressCounter[i]++;
                if (longPressCounter[i] >= LONG_PRESS_TICKS)
                {
                    KeyEventTypeArr[i] = KEY_EVENT_LONGPRESS;
                    KeyEventFlag |= (1 << i);
                    longPressDetected[i] = true; // Only one longpress per hold
                }
            }
        }
        // Not pressed at all, do nothing
    }

    prevOptoInFiltered = optoInFiltered;
}

// Call this to get and clear the event for a key
/**
 * @brief Get and clear the event for a specific opto key.
 * @param keyIndex Index of the key to check.
 * @return The event type (press, release, longpress, none).
 */
KeyEventType getOptoEvent(uint8_t keyIndex)
{
    if (keyIndex >= NUM_INPUTS)
        return KEY_EVENT_NONE;
    if (KeyEventFlag & (1 << keyIndex))
    {
        KeyEventFlag &= ~(1 << keyIndex); // Clear the flag
        KeyEventType evt = KeyEventTypeArr[keyIndex];
        KeyEventTypeArr[keyIndex] = KEY_EVENT_NONE;
        return evt;
    }
    return KEY_EVENT_NONE;
}

/**
 * @brief Get the index of the first key with a pending event flag.
 * @return Key index with event, or -1 if none.
 */
int32_t getOptoEventFlag(void)
{

    for (int i = 2; i < NUM_INPUTS; i++)
    {
        if (KeyEventFlag & (1 << i))
        {
            return i;
        }
    }
    return -1;
}