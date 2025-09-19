#include "OptoKeypadInTask.h"
#include "../AppDrivers/keypadDriver.h"
#include "../AppDrivers/optoInDriver.h"
#include "userUi.h"
#include <Arduino.h>

// Task handle for the OptoKeypadInTask
static TaskHandle_t optoKeypadTaskHandle = nullptr;

// Task statistics and monitoring
static unsigned long taskRunCount = 0;
static unsigned long lastStatsReport = 0;

// Enable/disable debug output (set to false for production)
static const bool DEBUG_TASK = false;

/**
 * @brief Main task function for OptoKeypadInTask
 * 
 * Runs at 100Hz (10ms intervals) to provide responsive input scanning.
 * Based on the original my_1ms_task but optimized for better timing.
 */
void optoKeypadInTask(void *pvParameters)
{
    Serial.println("OptoKeypadInTask: Starting input scanning task");
    
    // Initialize timing for precise periodic execution
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(10); // 10ms = 100Hz
    
    // Task main loop
    while (true)
    {
        // --- Core input scanning operations ---
        
        // Scan keypad inputs (debouncing handled internally)
        keypadRead();
        
        // Scan opto inputs and update filtered values
        scanOptos();
        
        // Update opto event states (press, release, longpress, etc.)
        updateOptoEvents();
        
       
        // --- Precise timing control ---
        // Use vTaskDelayUntil for precise periodic execution
        // This ensures the task runs exactly every 10ms regardless of execution time
        vTaskDelayUntil(&lastWakeTime, frequency);
    }
}

/**
 * @brief Initialize and start the OptoKeypadInTask
 */
bool initOptoKeypadInTask()
{
    // Prevent multiple task creation
    if (optoKeypadTaskHandle != nullptr)
    {
        Serial.println("OptoKeypadInTask: Task already running");
        return false;
    }
    
    // Ensure input drivers are initialized
    Serial.println("OptoKeypadInTask: Initializing input drivers...");
    
    // Initialize keypad driver
    keypadInit();
    Serial.println("OptoKeypadInTask: Keypad driver initialized");
    
    // Initialize opto input driver with default threshold
    scanOptosInit(30); // Threshold level after which opto is considered ON
    Serial.println("OptoKeypadInTask: Opto input driver initialized");
    
    // Create the FreeRTOS task directly
    BaseType_t result = xTaskCreatePinnedToCore(
        optoKeypadInTask,           // Task function
        "OptoKeypadInTask",         // Task name
        2048,                       // Stack size (2KB)
        nullptr,                    // Task parameters
        2,                          // Priority
        &optoKeypadTaskHandle,      // Task handle storage
        0                           // Core 0
    );
    
    
    
    if (result == pdPASS)
    {
        Serial.printf("OptoKeypadInTask: Task created successfully on core %d with priority %d\n", 
                     0, 2);
        return true;
    }
    else
    {
        Serial.println("OptoKeypadInTask: Failed to create task");
        optoKeypadTaskHandle = nullptr;
        return false;
    }
}





