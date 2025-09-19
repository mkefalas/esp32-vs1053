#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

#include <Arduino.h>
#include <SPI.h>
#include <SD_MMC.h> 
#include <SPIFFS.h>
#include <WiFi.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <VS1053.h>
#include <MySi4703.h>
#include "stdint.h"
#include "string.h"
#include "setupDriver.h"
#include <Wire.h>

//#include "VolumeManager.h"
//#include "PlaylistManager.h"

//─────────────────────────────────────────────────────────────────────────────
// Commands and parameter union
enum class AudioCommandType : uint8_t {
  PlayMusic,
  StreamMusic,
  PlayRadio,
  PlayAnnouncement,
  StartTest,
  PlayTestSource,
  StopTest,
  NextTrack,
  PrevTrack,
  Pause,
  Resume,
  Stop,
  SetMusicVol,
  SetAnnounceVol,
  MuteToggle
};

struct AudioCommand {
  AudioCommandType type;
  union {
    char     str[64];    // file path, URL, frequency string
    uint32_t value;      // numeric param (e.g. resume offset)
  } param;
};

//─────────────────────────────────────────────────────────────────────────────
// Playback and state enums
enum class PlaybackType : uint8_t {
  None,
  File,
  Stream,
  Radio,
  Announcement,
  Test
};

enum class Mode : uint8_t {
  Normal,
  Test
};

enum AudioFormat : uint8_t {
    FORMAT_UNKNOWN    = 0,  // Not recognized or unsupported
    FORMAT_WAV_PCM,        // RIFF/WAVE PCM
    FORMAT_WAV_IMA,        // RIFF/WAVE IMA ADPCM
    FORMAT_MP3,            // MPEG Layer I/II/III
    FORMAT_OGG,            // Ogg Vorbis
    FORMAT_AAC,            // MPEG-4 AAC
    FORMAT_WMA,            // Windows Media Audio
    FORMAT_FLAC            // Free Lossless Audio Codec
};


enum class PlayState : uint8_t {
  Idle,
  PlaybackInit,
  PlaybackPlay,
  AnnouncementInit,
  AnnouncementPlay
};

// Save/restore info
struct PlaybackState {
  String   filePath;
  uint32_t filePos;
  String   streamUrl;
  float    radioFreq;
};

class AudioTask {
public:
  AudioTask();
  void begin();

  // ISR-safe APIs
  void playMusic(const char* path);
  void streamMusic(const char* url);
  void playRadio(const char* freqStr);
  void playAnnouncement(const char* path);

  void startTest();
  void playTestSource(const char* src);
  void stopTest();

  void nextTrack();
  void prevTrack();

  void pause();
  void resume();
  void stop();

  void setMusicVolume(uint8_t vol);
  void setAnnouncementVolume(uint8_t vol);

  void toggleMute();
  bool isMuted() const;

  void setRetriggerMode(RetriggerMode m);
  RetriggerMode getRetriggerMode() const;

  Mode         getMode() const;
  PlaybackType getPlaybackType() const;
  PlaybackState getCurrentState() const;

private:
  // RTOS task
  static void     taskEntry(void* pv);
  void            taskLoop();

  // Command handling
  void            handleCommand(const AudioCommand& cmd);

  // Unified playback
  void            initPlayback();
  bool            stepPlayback();
  void            onPlaybackFinished();

  // Announcement
  void            initAnnouncement();
  bool            stepAnnouncement();
  void            restorePreviousSource();

  // Test mode
  void            saveTestState();

  // Helpers
  void            setHWVolume(uint8_t vol);
  void            loadRetriggerMode();
  void            saveRetriggerMode();

  void scanRootFolder();
  AudioFormat detectFormat(File &file, uint8_t *buf);

  // Hardware interfaces
  VS1053               player;
  Si4703               fmradio;

  // Queue and buffer
  static constexpr int QUEUE_LEN = 12;
  QueueHandle_t        cmdQueue;
  static constexpr int BUF_SZ    = 64;
  uint8_t              buf[BUF_SZ];

  // State machine
  PlayState            state             = PlayState::Idle;
  PlaybackType         currentType       = PlaybackType::None;
  Mode                 mode              = Mode::Normal;
  RetriggerMode        retriggerMode     = RetriggerMode::ResumeOnRelease;

  // Parameters for next playback
  char     nextParamBuf[64];
  uint32_t nextValue        = 0;
  float    nextRadioFreq    = 0.0f;

  // File and network handles
  File     fileHandle;
  WiFiClient httpClient;
  File     annHandle;

  // Playback snapshot
  PlaybackState        currentState      = { "", 0, "", 0.0f };
  PlaybackState        savedStateBeforeTest;

  // Volume & mute
  bool     muted               = false;
  uint8_t  musicVolume         = 100;
  uint8_t  announcementVolume  = 100;
  uint8_t  backupMusicVol      = 100;
  uint8_t  backupAnnVol        = 100;
};

// Single global instance
extern AudioTask audioTask;

#endif // AUDIO_TASK_H