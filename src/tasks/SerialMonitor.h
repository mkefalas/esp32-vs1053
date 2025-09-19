#ifndef SERIAL_MONITOR_H
#define SERIAL_MONITOR_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "setupDriver.h"

// Task configuration
#define SERIAL_MONITOR_TASK_STACK_SIZE 4096
#define SERIAL_MONITOR_TASK_PRIORITY 2
#define SERIAL_MONITOR_QUEUE_SIZE 10

// Function prototypes
void SerialMonitor_Init();
void SerialMonitor_Task(void *parameter);
TaskHandle_t SerialMonitor_GetTaskHandle();

// Command function prototypes
void cmd_help(int argc, char **argv);
void cmd_test(int argc, char **argv);
void cmd_play(int argc, char **argv);
void cmd_stop(int argc, char **argv);
void cmd_pause(int argc, char **argv);
void cmd_next(int argc, char **argv);
void cmd_prev(int argc, char **argv);
void cmd_volume(int argc, char **argv);
void cmd_source(int argc, char **argv);
void cmd_freq(int argc, char **argv);
void cmd_status(int argc, char **argv);
void cmd_pwd(int argc, char **argv);
void cmd_exit(int argc, char **argv);

// Serial printf function
void SerPrintf(const char *fmt, ...);

#endif // SERIAL_MONITOR_H