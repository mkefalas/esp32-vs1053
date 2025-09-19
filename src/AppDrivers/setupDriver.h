#ifndef SETUP_DRIVER_H
#define SETUP_DRIVER_H

#include <Arduino.h>

enum SOURCEMODE {
    SRC_MUSIC_SD = 0,
    SRC_WEB,
    SRC_RADIO,
    SRC_SPIFFS,
};

enum RetriggerMode : uint8_t {
  ResumeOnRelease = 0,
  NextTrackOnRelease
};

// Play mode flags
#define PLAY_MODE_NEXT_SONG       0    ///< After power off, play next song
#define PLAY_MODE_SAME_SONG       1    ///< After power off, play same song from start

/**
 * @brief Structure to hold setup parameters for the device.
 */
typedef struct _SETUP {
    uint32_t InitState; ///< Initialization state marker

    uint16_t freqMemories[20]; ///< Array of frequency memories
    // Individual source volumes (0-100 scale)
    uint8_t musicVolume; ///< Music playback volume (SD/files) (0-100)
    uint8_t webVolume; ///< Web streaming volume (0-100)
    uint8_t radioVolume; ///< FM radio volume (0-100)
    uint8_t announcementVolume; ///< Announcement playback volume (0-100)
    uint8_t eqPreset; ///< Equalizer preset index (0-4: Flat, Bass, Treble, Rock, Voice)
    SOURCEMODE currentMusicSource; ///< Current music source (FM/SD/Web)
    uint16_t lastFrequency; ///< Last used frequency
    char lastSDFile[32]; ///< Last played SD file for restore
    char lastWebURL[64]; ///< Last played web radio URL for restore
    uint8_t playMode; ///< 0=next song after power off, 1=same song from start
    uint8_t reserved[5]; ///< Reserved for future use (increased from 2 to 5 due to removed fields)
    int8_t baseFloor;
    RetriggerMode retriggerMode;
} SETUP;

extern SETUP Setup;

/**
 * @brief Save the current Setup structure to EEPROM.
 * @return true if save was successful, false otherwise.
 */
bool saveSetup();

/**
 * @brief Compare EEPROM contents to Setup and save if different.
 */
void saveSetupIfChanged();

/**
 * @brief Initialize the Setup structure from EEPROM or set defaults if invalid.
 * @return true if initialization was successful.
 */
bool InitSetup();

void markSetupDirty();

void CheckSetupDirty();
#endif // SETUP_DRIVER_H
