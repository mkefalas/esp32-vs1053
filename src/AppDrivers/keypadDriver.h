#pragma once

#include <Arduino.h>
#include "stdint.h"

// Key enum for keypad
enum Key { NONE, PREVIOUS=1, NEXT=2, DOWN=4, UP=8 };

// Initialize keypad pins (call in setup)
void keypadInit();

// Read the current key state
void  keypadRead();

uint8_t  getKeypadState();
uint8_t  getUpKeys();
void clearCmd(); // Clear command state after reading
void clearUpKeys(); // Clear up keys state after reading
bool CheckLongPress(uint16_t val); // Check if a key is pressed for a long duration
void ClearNextUpKeys(); // Clear next up keys flag