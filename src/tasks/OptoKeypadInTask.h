#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>

/**
 * @brief OptoKeypadInTask - High frequency input scanning task
 * 
 * This task runs at 100Hz (10ms intervals) to scan keypad and opto inputs.
 * It provides fast response for user interface elements while maintaining
 * system responsiveness.
 * 
 * The task performs:
 * - Keypad scanning and debouncing
 * - Opto input scanning and filtering
 * - Event generation for key presses/releases
 */

/**
 * @brief Initialize and start the OptoKeypadInTask
 * @param stackSize Stack size for the task (default: 2048 bytes)
 * @param priority Task priority (default: 2 - higher than loop task)
 * @param coreId Core to pin the task to (default: 0)
 * @return True if task creation successful, false otherwise
 */
bool initOptoKeypadInTask();

/**
 * @brief Get the task handle for OptoKeypadInTask
 * @return Task handle or NULL if task not created
 */
TaskHandle_t getOptoKeypadInTaskHandle();

/**
 * @brief Check if OptoKeypadInTask is running
 * @return True if task is running, false otherwise
 */
bool isOptoKeypadInTaskRunning();

/**
 * @brief Stop and delete the OptoKeypadInTask
 */
void stopOptoKeypadInTask();

/**
 * @brief Initialize complete input and UI system
 * 
 * This function handles the initialization of both OptoKeypadInTask and UserUI system.
 * It's designed to be called from main setup to simplify the initialization process.
 */
void initInputAndUISystem();

/**
 * @brief Main task function for OptoKeypadInTask
 * @param pvParameters Task parameters (unused)
 * 
 * This is the actual FreeRTOS task function that runs the input scanning loop.
 * Should not be called directly - use initOptoKeypadInTask() instead.
 */
void optoKeypadInTask(void *pvParameters);
