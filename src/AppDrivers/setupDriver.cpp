#include <Arduino.h>
#include <EEPROM.h>
#include <cstring> // For memcmp
#include "setupDriver.h"

SETUP Setup;

unsigned long setupDirtyTime = 0;
bool setupDirty = false;

bool saveSetup()
{
  Serial.printf("Saving setup to EEPROM... InitState: 0x%08X\n", Setup.InitState);
  Serial.printf("Volume: %d, LastFreq: %d\n", Setup.musicVolume, Setup.lastFrequency);
  Serial.print("Memory frequencies: ");
  for (int i = 0; i < 6; i++)
  {
    Serial.printf("M%d:%.1f ", i + 1, Setup.freqMemories[i] / 100.0);
  }
  Serial.println();

  EEPROM.put(0, Setup);
  if (EEPROM.commit())
  {
    Serial.println("Setup saved successfully to EEPROM.");
    return true;
  }
  else
  {
    Serial.println("Failed to save setup to EEPROM.");
    return false;
  }
}

void saveSetupIfChanged()
{
  SETUP temp;
  EEPROM.get(0, temp);

  // Compare the memory of both structures
  if (memcmp(&temp, &Setup, sizeof(SETUP)) != 0)
  {
    saveSetup();
    Serial.printf("setup changed, saving new setup.\n");
  }
}

bool InitSetup()
{
  bool init = 0;
  EEPROM.begin(sizeof(SETUP));
  Serial.printf("EEPROM initialized with %d bytes for SETUP structure\n", sizeof(SETUP));

  EEPROM.get(0, Setup);
  Serial.printf("Loaded setup from EEPROM - InitState: 0x%08X\n", Setup.InitState);

  if (Setup.InitState != 0xDEADBEEF)
  {
    Serial.println("InitState invalid, will reset to defaults");
    init = 1;
  }
  if (Setup.lastFrequency < 8750 || Setup.lastFrequency > 10800 || Setup.radioVolume > 100)
  {
    Serial.printf("Invalid frequency (%d) or volume (%d), will reset\n", Setup.lastFrequency, Setup.radioVolume);
    init = 1;
  }
  for (int i = 0; i < 20; i++)
  {
    if (Setup.freqMemories[i] < 8750 || Setup.freqMemories[i] > 10800)
    {
      Serial.printf("Invalid memory %d frequency: %d, will reset\n", i + 1, Setup.freqMemories[i]);
      init = 1;
      break;
    }
  }

  if (init)
  {
    // First run or invalid data, initialize with defaults
    Serial.println("Initializing setup with default values...");
    Setup.radioVolume = 90;            // Radio volume (0-15)
    Setup.lastFrequency = 10110;  // 101.1 MHz
    Setup.musicVolume = 90;          // SD card volume (0-100)
     Setup.webVolume = 90;          // SD card volume (0-100)
     Setup.announcementVolume = 90; // Announcement volume (0-100)
    Setup.currentMusicSource = SRC_RADIO; // Default to FM radio (0=FM, 1=WEB, 2=SD)
    Setup.playMode = 0;           // Default to single play mode
    Setup.eqPreset = 0;           // Default to flat EQ
    Setup.baseFloor = -1;              // Default floor number
    strcpy(Setup.lastSDFile, ""); // No last file initially
    strcpy(Setup.lastWebURL, ""); // No last web URL initially

    Setup.eqPreset=0;

    // Initialize frequency memories with common FM stations
    Setup.freqMemories[0] = 10110;  // 101.1 MHz
    Setup.freqMemories[1] = 10140;  // 101.4 MHz  
    Setup.freqMemories[2] = 10170;  // 101.7 MHz
    Setup.freqMemories[3] = 10200;  // 102.0 MHz
    Setup.freqMemories[4] = 10230;  // 102.3 MHz
    Setup.freqMemories[5] = 10260;  // 102.6 MHz
    // Fill rest with default frequency
    for (int i = 6; i < 20; i++)
      Setup.freqMemories[i] = 10110; 

    // Clear reserved area
    memset(Setup.reserved, 0, sizeof(Setup.reserved));

    Setup.InitState = 0xDEADBEEF; // Set a valid state
    saveSetup();                  // Save the initialized setup
    Serial.println("Setup initialized and saved with defaults");
  }
  else
  {
    Serial.println("Setup loaded successfully from EEPROM");
    Serial.printf("Current music source: %d (0=FM, 1=WEB, 2=SD)\n", Setup.currentMusicSource);
    Serial.printf("Base Floor: %d, Volume: %d, SD Volume: %d\n", Setup.baseFloor, Setup.radioVolume, Setup.musicVolume);
    Serial.print("Memory frequencies: ");
    for (int i = 0; i < 6; i++)
    {
      Serial.printf("M%d:%.1f ", i + 1, Setup.freqMemories[i] / 100.0);
    }
    Serial.println();
  }
  return true;
}

void markSetupDirty()
{
  setupDirty = true;
  setupDirtyTime = millis();
}

void CheckSetupDirty()
{
  if (setupDirty && (millis() - setupDirtyTime > 2000))
  { // 5 seconds threshold
    saveSetupIfChanged();
    setupDirty = false; // Reset dirty flag after saving
  }
}
