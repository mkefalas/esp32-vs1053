/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <Arduino.h>
#include <VS1003B.h>
#include <MySi4703.h>
#include <Adafruit_SSD1306.h>

#include <WiFi.h>
#include <pins.h>
#include <SerialMonitor.h>
#include "../Tasks/OptoKeypadInTask.h"
#include "../Tasks/userUi.h"
#include "AudioTask.h"
#include <setupDriver.h>




uint8_t vol = 15;

void setup()
{
  InitSetup();
  SerialMonitor_Init();
  Serial.println("Setup start");
  initOptoKeypadInTask();
  // Initialize the complete audio system (includes hardware and flash settings)

  // Initialize UserUI system
  Serial.println("Initializing UserUI system...");
  audioTask.begin();
  startScreenManager();
  Serial.println("UI system initialization complete");

}

void loop()
{
}