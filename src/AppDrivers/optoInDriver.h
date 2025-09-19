
/**
 * @file optoInDriver.h
 * @brief Driver for scanning and processing opto-isolated digital inputs.
 *
 * This file provides functions to initialize, scan, and filter opto inputs,
 * as well as detect key events (press, release, longpress, double press).
 *
 * Main functions:
 *  - scanOptosInit(uint8_t threshold): Initialize hardware and buffers for opto input scanning.
 *  - scanOptos(): Scan opto inputs and update filtered values.
 *  - updateOptoEvents(): Update key event states for opto inputs.
 *  - getOptoInFiltered(): Get the current filtered opto input bitmask.
 *  - getOptoEvent(uint8_t keyIndex): Get and clear the event for a specific key.
 *  - getOptoEventFlag(): Get the index of the first key with a pending event.
 */
#ifndef OPTOINDRIVER_H
#define OPTOINDRIVER_H

#include <cstdint>

#define NUM_INPUTS 14
#define WINDOW 128

typedef enum {
    KEY_EVENT_NONE      = 0,
    KEY_EVENT_PRESS     = 1,
    KEY_EVENT_RELEASE   = 2,
    KEY_EVENT_LONGPRESS = 3,
    KEY_EVENT_DOUBLE    = 4,
} KeyEventType;

extern uint16_t sample_buffer[WINDOW];
extern uint8_t acc[NUM_INPUTS];
extern uint8_t pos;
extern uint16_t optoInFiltered;
extern uint8_t THRESHOLD;

void scanOptosInit(uint8_t threshold);
void updateOptoEvents();
uint16_t getOptoInFiltered(void);
void scanOptos(void);
KeyEventType getOptoEvent(uint8_t keyIndex);
int32_t getOptoEventFlag(void);

#endif // OPTOINDRIVER_H