#include "SerialMonitor.h"
#include "Audiotask.h"
#include <stdarg.h>
#include <setupDriver.h>

// Task handle
static TaskHandle_t serialMonitorTaskHandle = NULL;

// Command structure
struct command_d
{
    void (*cmd_fnct)(int, char **);
    const char *cmd_name;
    const char *cmd_help;
};

// Command table
const struct command_d commands[] = {
    {cmd_help, "help", "Show available commands"},
    {cmd_test, "test", "Run system test"},
    {cmd_play, "play", "Start playback [radio|card|web]"},
    {cmd_stop, "stop", "Stop playback"},
    {cmd_pause, "pause", "Pause/Resume playback"},
    {cmd_next, "next", "Next track (card mode)"},
    {cmd_prev, "prev", "Previous track (card mode)"},
    {cmd_volume, "vol", "Set volume [0-100]"},
    {cmd_source, "src", "Set source [radio|card|web]"},
    {cmd_freq, "freq", "Set radio frequency [87.5-108.0]"},
    {cmd_status, "stat", "Show system status"},
    {cmd_pwd, "pwd", "Enter password [8010]"},
    {cmd_exit, "exit", "Exit monitor (task continues)"}};

#define NCOMMANDS (sizeof(commands) / sizeof(struct command_d))
#define ARGVECSIZE 10
#define BUFFER_SIZE 128

// Global variables
static bool unlockTerminal = false;
static const char *PASSWORD = "8010";

// Add AudioManager instance pointer

// Initialize serial monitor and create task
void SerialMonitor_Init()
{
    Serial.begin(115200);

    // Create the serial monitor task
    BaseType_t result = xTaskCreate(
        SerialMonitor_Task,             // Task function
        "SerialMonitor",                // Task name
        SERIAL_MONITOR_TASK_STACK_SIZE, // Stack size
        NULL,                           // Parameters
        SERIAL_MONITOR_TASK_PRIORITY,   // Priority
        &serialMonitorTaskHandle        // Task handle
    );

    if (result == pdPASS)
    {
        Serial.println("SerialMonitor task created successfully");
    }
    else
    {
        Serial.println("Failed to create SerialMonitor task");
    }
}

// Get task handle
TaskHandle_t SerialMonitor_GetTaskHandle()
{
    return serialMonitorTaskHandle;
}

// Serial printf function
void SerPrintf(const char *fmt, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Serial.print(buffer);
}

// Parse command line into argv
int parseCommand(char *line, char **argv, int maxArgs)
{
    int argc = 0;
    char *token = strtok(line, " \t\n\r");

    while (token && argc < maxArgs)
    {
        // Convert to lowercase
        for (char *p = token; *p; p++)
        {
            *p = tolower(*p);
        }
        argv[argc++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    return argc;
}

// Read line with timeout (blocking until command received)
bool readLineWithTimeout(char *buffer, size_t bufferSize, TickType_t timeout)
{
    size_t index = 0;

    while (1)
    {
        if (Serial.available())
        {
            char ch = Serial.read();

            // Handle backspace
            if (ch == '\b' || ch == 0x7F)
            {
                if (index > 0)
                {
                    index--;
                    Serial.write("\b \b"); // Erase character
                }
                continue;
            }

            // Echo character (except for CR/LF)
            if (ch != '\n' && ch != '\r')
            {
                Serial.write(ch);
            }

            // Check for end of line
            if (ch == '\n' || ch == '\r')
            {
                if (index > 0)
                { // Only process if we have content
                    buffer[index] = '\0';
                    Serial.println(); // Add newline for clean output
                    return true;
                }
                continue; // Ignore empty lines
            }

            // Add character to buffer
            if (index < bufferSize - 1)
            {
                buffer[index++] = ch;
            }
        }

        // Small delay to prevent task from hogging CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return false; // Should never reach here
}

// Main serial monitor task
void SerialMonitor_Task(void *parameter)
{
    static char buffer[BUFFER_SIZE];
    static char *argv[ARGVECSIZE];

    // Task startup delay - ensure serial is ready
    vTaskDelay(pdMS_TO_TICKS(500));

    SerPrintf("\n\r=== ESP32 Audio Player Monitor ===\n");
    SerPrintf("FreeRTOS Task Running\n");
    SerPrintf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    SerPrintf("Task Stack High Water Mark: %d\n", uxTaskGetStackHighWaterMark(NULL));
    SerPrintf("Type 'help' for available commands\n");
    SerPrintf("Enter password to unlock advanced features\n");

    // Ensure all output is flushed
    Serial.flush();
    vTaskDelay(pdMS_TO_TICKS(100));

    SerPrintf("Cmd> ");
    Serial.flush();

    // Main task loop - wait for commands
    while (1)
    {
        // Read command (blocking)
        if (readLineWithTimeout(buffer, BUFFER_SIZE, portMAX_DELAY))
        {
            // Parse and execute command
            int argc = parseCommand(buffer, argv, ARGVECSIZE);

            if (argc > 0)
            {
                // Find and execute command
                bool found = false;
                for (int i = 0; i < NCOMMANDS; i++)
                {
                    if (strcmp(argv[0], commands[i].cmd_name) == 0)
                    {
                        found = true;

                        // Check if terminal is locked
                        if (!unlockTerminal && strcmp(argv[0], "pwd") != 0 &&
                            strcmp(argv[0], "help") != 0 && strcmp(argv[0], "test") != 0)
                        {
                            SerPrintf("Terminal locked! Enter password with 'pwd <password>'\n");
                        }
                        else
                        {
                            commands[i].cmd_fnct(argc, argv);
                        }
                        break;
                    }
                }

                if (!found)
                {
                    SerPrintf("Unknown command '%s'. Type 'help' for available commands.\n", argv[0]);
                }
            }

            // Show prompt for next command
            SerPrintf("Cmd> ");
            Serial.flush();
        }

        // Small delay to allow other tasks to run
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// Command implementations
void cmd_help(int argc, char **argv)
{
    SerPrintf("\nAvailable commands:\n");
    SerPrintf("===================\n");
    for (int i = 0; i < NCOMMANDS; i++)
    {
        SerPrintf("  %-8s - %s\n", commands[i].cmd_name, commands[i].cmd_help);
    }
    SerPrintf("\nTask Info:\n");
    SerPrintf("  Stack High Water Mark: %d\n", uxTaskGetStackHighWaterMark(NULL));
    SerPrintf("  Free Heap: %d bytes\n", ESP.getFreeHeap());
    SerPrintf("\nExamples:\n");
    SerPrintf("  vol 75        - Set volume to 75%%\n");
    SerPrintf("  freq 101.5    - Set radio to 101.5 MHz\n");
    SerPrintf("  src radio     - Switch to radio source\n");
    SerPrintf("  play          - Start playback\n");
    SerPrintf("\n");
}

void cmd_test(int argc, char **argv)
{
    SerPrintf("=== System Test ===\n");
    SerPrintf("Hardware Version: ESP32-S3\n");
    SerPrintf("Software Version: 1.0.0\n");
    SerPrintf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    SerPrintf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
    SerPrintf("Flash Size: %d bytes\n", ESP.getFlashChipSize());
    SerPrintf("Task Stack High Water Mark: %d\n", uxTaskGetStackHighWaterMark(NULL));
    SerPrintf("Task Priority: %d\n", uxTaskPriorityGet(NULL));
    SerPrintf("Tick Count: %lu\n", xTaskGetTickCount());
    SerPrintf("System test completed.\n");
}

void cmd_play(int argc, char **argv)
{
    if (argc == 1)
    {
        SerPrintf("Starting playback with current source...\n");
     
          //  audioManager->switchAudioSource(Setup.currentMusicSource,audioManager->getCurrentSourceString());
      
    }
    else if (argc == 2)
    {
        SerPrintf("Starting playback with source: %s\n", argv[1]);
      
            if (strcmp(argv[1], "radio") == 0)
            {
               // audioManager->switchAudioSource(SRC_RADIO,"");
            }
            else if (strcmp(argv[1], "web") == 0)
            {
               // audioManager->switchAudioSource(SRC_WEB,Setup.lastWebURL);
            }
            else if (strcmp(argv[1], "card") == 0)
            {
               // audioManager->switchAudioSource(SRC_MUSIC_SD,Setup.lastSDFile);
            }
           
       
    }
    else
    {
        SerPrintf("Usage: play [radio|card|web]\n");
    }
}

void cmd_stop(int argc, char **argv)
{
    SerPrintf("Stopping playback...\n");
    // TODO: Call AudioManager->stop()
}

void cmd_pause(int argc, char **argv)
{
    SerPrintf("Toggling pause...\n");
    // TODO: Call AudioManager->pause() or resume()
}

void cmd_next(int argc, char **argv)
{
    SerPrintf("Next track...\n");
    // TODO: Call AudioManager->next()
}

void cmd_prev(int argc, char **argv)
{
    SerPrintf("Previous track...\n");
    // TODO: Call AudioManager->previous()
}

void cmd_volume(int argc, char **argv)
{
    if (argc == 2)
    {
        int vol = atoi(argv[1]);
        if (vol >= 0 && vol <= 100)
        {
            SerPrintf("Setting volume to %d%%\n", vol);
           
                // Set both radio and SD volume
               // audioManager->radioSetVolume(vol * 15 / 100); // Convert 0-100 to 0-15 for radio
               // audioManager->setCurrentSourceVolume(vol);
           
        }
        else
        {
            SerPrintf("Volume must be between 0-100\n");
        }
    }
    else
    {
        SerPrintf("Usage: vol <0-100>\n");
           // SerPrintf("Current radio volume: %d/15\n", audioManager->radioGetVolume());
            SerPrintf("Current SD volume: %d%%\n", Setup.musicVolume);
       
    }
}

void cmd_source(int argc, char **argv)
{
    if (argc == 2)
    {
        if (strcmp(argv[1], "radio") == 0)
        {
            SerPrintf("Setting source to: radio\n");
           
               // audioManager->switchAudioSource(SRC_RADIO,"");
            
        }
        else if (strcmp(argv[1], "web") == 0)
        {
            SerPrintf("Setting source to: web\n");
           // if (audioManager)
           // {
                //audioManager->switchAudioSource(SRC_WEB,"");
            
        }
        else if (strcmp(argv[1], "card") == 0)
        {
            SerPrintf("Setting source to: card\n");
        
               // audioManager->switchAudioSource(SRC_MUSIC_SD,"");
        }
        else
        {
            SerPrintf("Invalid source. Use: radio, card, web\n");
        }
    }
    else
    {
        SerPrintf("Usage: src <radio|card|web>\n");
        SerPrintf("Current source: %d\n", Setup.currentMusicSource);
    }
}

void cmd_freq(int argc, char **argv)
{
    if (argc == 2)
    {
        float freq = atof(argv[1]);
        if (freq >= 87.5 && freq <= 108.0)
        {
            SerPrintf("Setting radio frequency to %.1f MHz\n", freq);
        
                int freqInt = (int)(freq * 100); // Convert to integer format
               // audioManager->radioSetChannel(freqInt);
            
          
        }
        else
        {
            SerPrintf("Frequency must be between 87.5-108.0 MHz\n");
        }
    }
    else
    {
        SerPrintf("Usage: freq <87.5-108.0>\n");
        
        
           // SerPrintf("Current frequency: %.1f MHz\n", audioManager->radioGetChannel() / 100.0);
        
      
    }
}

void cmd_status(int argc, char **argv)
{
    SerPrintf("\n=== System Status ===\n");
    SerPrintf("Uptime: %lu ms\n", millis());
    SerPrintf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    SerPrintf("Task Stack High Water Mark: %d\n", uxTaskGetStackHighWaterMark(NULL));
    SerPrintf("Terminal: %s\n", unlockTerminal ? "Unlocked" : "Locked");
    SerPrintf("Task Priority: %d\n", uxTaskPriorityGet(NULL));
    SerPrintf("Tick Count: %lu\n", xTaskGetTickCount());

    
        SerPrintf("Audio Source: %d\n", Setup.currentMusicSource);
      //  SerPrintf("Radio Freq: %.1f MHz\n", audioManager->radioGetChannel() / 100.0);
      //  SerPrintf("Radio Volume: %d/15\n", audioManager->radioGetVolume());
        SerPrintf("SD Volume: %d%%\n", Setup.musicVolume);
      //  SerPrintf("EQ Preset: %s\n", audioManager->getEqualizerPresetName(audioManager->getCurrentEqualizerPreset()));
       // SerPrintf("Radio RSSI: %d\n", audioManager->radioGetRSSI());
       // SerPrintf("Radio Stereo: %s\n", audioManager->radioGetStereo() ? "Yes" : "No");
    
    
    SerPrintf("Current Track: Unknown\n");
    SerPrintf("=====================\n");
}

void cmd_pwd(int argc, char **argv)
{
    if (argc == 2)
    {
        if (strcmp(argv[1], PASSWORD) == 0)
        {
            unlockTerminal = true;
            SerPrintf("Terminal unlocked! All commands are now available.\n");
        }
        else
        {
            SerPrintf("Invalid password.\n");
        }
    }
    else
    {
        SerPrintf("Usage: pwd <password>\n");
    }
}

void cmd_exit(int argc, char **argv)
{
    SerPrintf("Monitor task continues running in background.\n");
    SerPrintf("Commands still available via serial.\n");
    // Task continues running, just acknowledges the exit command
}