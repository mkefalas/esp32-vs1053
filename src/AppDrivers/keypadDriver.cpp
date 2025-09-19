#include "keypadDriver.h"
#include "pins_new.h"

uint8_t volatile   g_keypadState, lastPressedKey=NONE,KeyUp = NONE;
uint16_t volatile KeyTimer = 0; // Timer for key press duration
bool ClearKeyUpFlag=0;

void keypadInit()
{
    pinMode(KEY_PREVIOUS_PIN, INPUT_PULLUP);
    pinMode(KEY_NEXT_PIN, INPUT_PULLUP);
    pinMode(KEY_DOWN_PIN, INPUT_PULLUP);
    pinMode(KEY_UP_PIN, INPUT_PULLUP);
#ifdef NEW_VERSION_PINS
    pinMode(SEL_KEYPAD_PIN, OUTPUT_OPEN_DRAIN);
#endif
}

void keypadRead()
{
    uint8_t tmpkey=NONE;
    if (digitalRead(KEY_PREVIOUS_PIN) == LOW) tmpkey = PREVIOUS;
    if (digitalRead(KEY_NEXT_PIN) == LOW) tmpkey |= NEXT;
    if (digitalRead(KEY_DOWN_PIN) == LOW) tmpkey |= DOWN;
    if (digitalRead(KEY_UP_PIN) == LOW) tmpkey |= UP;
    if (tmpkey != NONE)
    {
        if (lastPressedKey != tmpkey)
        {
            lastPressedKey = tmpkey;
            KeyTimer = 0;          // Reset timer for key press
        }
        else if (KeyTimer < 65535) KeyTimer+=1; // Increment timer for key press
        g_keypadState = tmpkey; // Update current key state
    }
    else
    {
         if (ClearKeyUpFlag)
    {
        ClearKeyUpFlag = false; // Reset flag after reading
        lastPressedKey=NONE;
    }
        KeyUp = lastPressedKey; // last pressed key when no key is pressed
        KeyTimer = 0;
    }
}

uint8_t  getKeypadState()
{
  
    return g_keypadState;
}

void clearCmd()
{
  g_keypadState = NONE; // Reset after reading
}

void clearUpKeys()
{
    KeyUp = NONE; // Reset after reading
    lastPressedKey = NONE; // Reset last pressed key
    KeyTimer = 0; // Reset key timer
    g_keypadState = NONE; // Reset current key state
}

uint8_t  getUpKeys()
{
    return (KeyUp);
}

bool CheckLongPress(uint16_t val)
{
    if (KeyTimer > val) // Long press threshold
    {
        return true;  // Long press detected
    }
    return false; // No long press
}

void ClearNextUpKeys()
{
    ClearKeyUpFlag=1;
     KeyTimer = 0; // Reset key timer
}