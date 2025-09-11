#pragma once
#include <Adafruit_VS1053.h>
#include "setupDriver.h"
#include <SD.h>
#include <vector>
#include <string>
#include "pins.h"

enum AudioFormat {
  FORMAT_UNKNOWN,
  FORMAT_WAV_PCM,
  FORMAT_WAV_IMA,
  FORMAT_MP3,
  FORMAT_OGG,
  FORMAT_AAC
};

enum ControlMode {
  CONTROL_NORMAL = 0,  // GPIO play + announcement pin active
  CONTROL_TEST         // All GPIO ignored; task-driven control
};

enum SourceMode {
  SOURCE_SD = 0,       // SD file playback (manual feed)
  SOURCE_LINEIN,       // VS1053 line-in passthrough (requires patch/wiring)
  SOURCE_WEB,          // Web radio via WiFiClient -> VS1053
  SOURCE_RADIO         // External tuner; audio routed to VS1053 line-in or codec path
};

class AudioManager {
public:
  AudioManager(Adafruit_VS1053_FilePlayer &player);
  void begin();
  // Modes
  void setControlMode(ControlMode mode);
  ControlMode getControlMode() const;
  void setSource(SourceMode source);
  SourceMode getSource() const;

  // SD playback
  bool playFile(const char* filename);
  void pause();
  void resume();
  void playNext();
  void resumeStream();
  void scanRootFolder();

  // Web radio
  void setWebStream(const char* url);
  void startWebStream();
  void stopWebStream();
  void feedWebStream();

  // Volume
  void setVolume(uint8_t left, uint8_t right);

  // Playlist
  std::vector<std::string> playlist;

private:
  AudioFormat detectFormat(File &file, uint8_t *headerBuf);
  void enableLineIn(bool en);
  bool vsReady() const;

  Adafruit_VS1053_FilePlayer &vsPlayer;

  // SD state
  File currentFile;
  AudioFormat currentFormat = FORMAT_UNKNOWN;
  size_t resumePos = 0;
  uint8_t wavHeader[44] = {0};
  size_t currentIndex = 0;

  // Modes
  ControlMode controlMode = CONTROL_NORMAL;
  SourceMode sourceMode = SOURCE_SD;

  // Web radio
  String webUrl;
  bool webConnected = false;

  // Volume cache
  uint8_t volL = 20;
  uint8_t volR = 20;
};
