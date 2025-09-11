#include "AudioManager.h"
#include <Arduino.h>
#include "MySi4703.h"
#include "audioTask.h"
#include <WiFi.h>
#include <HTTPClient.h>
static HTTPClient g_http;
static WiFiClient g_streamClient;

#ifndef SCI_MODE
#define SCI_MODE 0x00
#endif

/**
 * @brief Initialize the AudioManager with complete setup
 *
 * This method handles all initialization - hardware setup and applying settings from flash.
 * Call this after setupDriver has loaded the settings from flash.
 */
void AudioManager::begin()
    Serial.println("AudioManager: Starting initialization...");

// Audio queue setup
audioCommandQueue = audioQueue;

// I2C init (shared by FM radio & display)
Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);

// VS1053 reset & SPI init
for (int pin : {VS1003B_RST_PIN, VS1003B_CS_PIN})
  pinMode(pin, OUTPUT);
digitalWrite(VS1003B_RST_PIN, LOW);
delay(100);
digitalWrite(VS1003B_RST_PIN, HIGH);
delay(100);
SPI.setHwCs(true);
SPI.begin(VS1003B_CLK_PIN, VS1003B_MISO_PIN, VS1003B_MOSI_PIN);
player.begin();

// SI4703 reset sequence
pinMode(RST_PIN, OUTPUT);
digitalWrite(RST_PIN, LOW);
delay(1);
digitalWrite(RST_PIN, HIGH);
digitalWrite(42, HIGH);
delay(10);

// FM Radio init
{
  std::lock_guard<std::mutex> lock(radioMutex);
  radio.start();
  radio.setMono(false);
  radio.setVolume(Setup.volume);
  radio.setChannel(Setup.lastFrequency);
}

// SD Card & VS1053 init
SD_MMC.setPins(PIN_SD_MMC_CLK, PIN_SD_MMC_CMD, PIN_SD_MMC_D0);
if (SD_MMC.begin("/sdmmc", true, false, 20000))
{

    sdSetVolume(Setup.sdVolume);
    setEqualizerPreset(1); // Setup.eqPreset
}
else
{
  Serial.println("AudioManager: SD Card init failed");
}

// Activate saved source
activateSavedMusicSource();

Serial.println("AudioManager: Initialization complete");
}

// Adjust to match your VS1053 line-in/monitor patch bit
static const uint16_t SM_LINE1_BIT = 0x0020;

AudioManager::AudioManager(Adafruit_VS1053_FilePlayer &player)
    : vsPlayer(player) {}

void AudioManager::setControlMode(ControlMode mode) { controlMode = mode; }
ControlMode AudioManager::getControlMode() const { return controlMode; }

void AudioManager::setSource(SourceMode source)
{
  sourceMode = source;

  if (source == SOURCE_LINEIN || source == SOURCE_RADIO)
  {
    vsPlayer.stopPlaying();
    enableLineIn(true);
    webConnected = false;
  }
  else
  {
    enableLineIn(false);
    if (source != SOURCE_WEB)
      webConnected = false;
  }
}
SourceMode AudioManager::getSource() const { return sourceMode; }

void AudioManager::setVolume(uint8_t left, uint8_t right)
{
  volL = left;
  volR = right;
  vsPlayer.setVolume(left, right);
}

static inline bool mp3Sync(uint8_t b0, uint8_t b1)
{
  return (b0 == 0xFF) && ((b1 & 0xE0) == 0xE0);
}

AudioFormat AudioManager::detectFormat(File &file, uint8_t *buf)
{
  file.seek(0);
  size_t n = file.read(buf, 44);
  if (n >= 12 && memcmp(buf, "RIFF", 4) == 0 && memcmp(buf + 8, "WAVE", 4) == 0)
  {
    uint16_t fmt = buf[20] | (buf[21] << 8);
    return (fmt == 0x0011) ? FORMAT_WAV_IMA : FORMAT_WAV_PCM;
  }
  if (n >= 4 && memcmp(buf, "OggS", 4) == 0)
    return FORMAT_OGG;
  if (n >= 2 && buf[0] == 0xFF && (buf[1] & 0xF0) == 0xF0)
  {
    return (buf[1] & 0x06) ? FORMAT_AAC : FORMAT_MP3;
  }
  for (size_t i = 0; i + 1 < n; ++i)
    if (mp3Sync(buf[i], buf[i + 1]))
      return FORMAT_MP3;
  return FORMAT_UNKNOWN;
}

bool AudioManager::playFile(const char *filename)
{
  if (sourceMode != SOURCE_SD)
    setSource(SOURCE_SD);
  if (currentFile)
    currentFile.close();
  currentFile = SD.open(filename, FILE_READ);
  if (!currentFile)
    return false;
  currentFormat = detectFormat(currentFile, wavHeader);
  resumePos = 0;
  // We'll feed manually via resumeStream(); start decoder clean
  vsPlayer.softReset();
  vsPlayer.setVolume(volL, volR);
  return true;
}

void AudioManager::pause()
{
  if (sourceMode == SOURCE_SD)
  {
    if (currentFile)
      resumePos = currentFile.position();
    vsPlayer.stopPlaying(); // stops any active helper playback
  }
  else if (sourceMode == SOURCE_WEB)
  {
    stopWebStream();
  }
  else if (sourceMode == SOURCE_LINEIN || sourceMode == SOURCE_RADIO)
  {
    enableLineIn(false);
  }
}

void AudioManager::resume()
{
  if (sourceMode == SOURCE_SD)
  {
    vsPlayer.softReset();
    vsPlayer.setVolume(volL, volR);
    if (currentFormat == FORMAT_WAV_IMA)
      resumePos = (resumePos / 512) * 512;
    else if (resumePos >= 2048)
      resumePos -= 2048;
    if (currentFile)
      currentFile.seek(resumePos);
  }
  else if (sourceMode == SOURCE_WEB)
  {
    startWebStream();
  }
  else if (sourceMode == SOURCE_LINEIN || sourceMode == SOURCE_RADIO)
  {
    enableLineIn(true);
  }
}

void AudioManager::resumeStream()
{
  if (sourceMode != SOURCE_SD || !currentFile)
    return;

  // Feed raw file bytes to VS1053
  while (currentFile.available() && vsReady())
  {
    uint8_t buf[64];
    size_t n = currentFile.read(buf, sizeof(buf));
    if (n == 0)
      break;
    vsPlayer.sdiWrite(buf, n);
  }

  if (!currentFile.available())
  {
    currentFile.close();
    playNext();
  }
}

void AudioManager::playNext()
{
  if (playlist.empty())
    return;
  currentIndex = (currentIndex + 1) % playlist.size();
  playFile(playlist[currentIndex].c_str());
}

void AudioManager::scanRootFolder()
{
  playlist.clear();
  File root = SD.open("/");
  if (!root || !root.isDirectory())
    return;

  for (File entry = root.openNextFile(); entry; entry = root.openNextFile())
  {
    if (!entry.isDirectory())
    {
      uint8_t headerBuf[44] = {0};
      AudioFormat fmt = detectFormat(entry, headerBuf);
      if (fmt != FORMAT_UNKNOWN)
      {
        playlist.push_back(std::string(entry.name()));
      }
    }
    entry.close();
  }
  root.close();
  currentIndex = 0;
}

/**
 * @brief Set radio channel/frequency and configure VS1003 for FM radio
 */
void AudioManager::radioSetChannel(int freq)
{
  std::lock_guard<std::mutex> lock(radioMutex);
  radio.setChannel(freq);
  Setup.lastFrequency = freq;
  markSetupDirty();

  Serial.printf("AudioManager: Set FM frequency to %.1f MHz\n", freq / 100.0);
}

int AudioManager::radioSeekUp()
{
  std::lock_guard<std::mutex> lock(radioMutex);
  int newFreq = radio.seekUp();
  if (newFreq > 0)
  {
    Setup.lastFrequency = newFreq;
    markSetupDirty();
  }
  return newFreq;
}

int AudioManager::radioSeekDown()
{
  std::lock_guard<std::mutex> lock(radioMutex);
  int newFreq = radio.seekDown();
  if (newFreq > 0)
  {
    Setup.lastFrequency = newFreq;
    markSetupDirty();
  }
  return newFreq;
}

/**
 * @brief Set equalizer preset directly to VS1053
 */
void AudioManager::setEqualizerPreset(uint8_t preset)
{
  if (preset >= EQ_PRESET_COUNT)
    preset = 0;

  Setup.eqPreset = preset;
  markSetupDirty();

  // Use VS1053 library method for tone control
  if (audioTaskInitialized)
  {
    uint8_t toneSettings[4];
    memcpy(toneSettings, eqPresets[preset], 4);
    player.setTone(toneSettings);
    Serial.printf("AudioManager: Set EQ preset %d (%s)\n",
                  preset, eqPresetNames[preset]);
  }
}

// Direct radio access functions (no setup updates but still thread-safe)
int AudioManager::radioGetRSSI()
{
  std::lock_guard<std::mutex> lock(radioMutex);
  return radio.getRSSI();
}

void AudioManager::radioSetMute(bool en)
{
  std::lock_guard<std::mutex> lock(radioMutex);
  radio.setMute(en);
}

bool AudioManager::radioGetMute()
{
  std::lock_guard<std::mutex> lock(radioMutex);
  return radio.getMute();
}

bool AudioManager::radioGetStereo()
{
  std::lock_guard<std::mutex> lock(radioMutex);
  return radio.getST();
}

/**
 * @brief Save radio preset
 */
void AudioManager::radioSavePreset(int presetNum, int freq)
{
  if (presetNum < 0 || presetNum >= 20)
  {
    Serial.printf("AudioManager: Invalid preset %d (0-19 allowed)\n", presetNum);
    return;
  }

  Setup.freqMemories[presetNum] = freq;
  markSetupDirty(); // Call setupDriver directly

  Serial.printf("AudioManager: Saved preset %d: %.1f MHz\n", presetNum + 1, freq / 100.0);
}

int AudioManager::radioLoadPreset(int presetNum)
{
  if (presetNum < 0 || presetNum >= 20)
  {
    Serial.printf("AudioManager: Invalid preset %d (0-19 allowed)\n", presetNum);
    return -1;
  }

  return Setup.freqMemories[presetNum];
}

void AudioManager::radioSelectPreset(int presetNum)
{
  if (presetNum < 0 || presetNum >= 20)
  {
    Serial.printf("AudioManager: Invalid preset number %d\n", presetNum);
    return;
  }

  int freq = Setup.freqMemories[presetNum];
  radioSetChannel(freq); // This will handle VS1003 mode and setup updates

  Serial.printf("AudioManager: Selected preset %d: %.1f MHz\n", presetNum + 1, freq / 100.0);
}

//==============================================================================
// Additional AudioManager Functions (not shown in UXF but still needed)
//==============================================================================

int AudioManager::radioGetChannel()
{
  std::lock_guard<std::mutex> lock(radioMutex);
  return radio.getChannel();
}

int AudioManager::radioGetVolume()
{
  std::lock_guard<std::mutex> lock(radioMutex);
  return radio.getVolume();
}

void AudioManager::radioSetVolume(int volume)
{
  std::lock_guard<std::mutex> lock(radioMutex);
  radio.setVolume(volume);
  Setup.volume = volume;
  markSetupDirty(); // Call setupDriver directly
}

uint8_t AudioManager::getCurrentEqualizerPreset()
{
  return (Setup.eqPreset >= EQ_PRESET_COUNT) ? 0 : Setup.eqPreset;
}

const char *AudioManager::getEqualizerPresetName(uint8_t preset)
{
  //@brief Start playback for a specific audio source with complete setup
  return (preset >= EQ_PRESET_COUNT) ? "Unknown" : eqPresetNames[preset];
}

void AudioManager::resetEqualizer()
{
  setEqualizerPreset(0);
}

void AudioManager::setWebStream(const char *url) { webUrl = url ? url : ""; }

void AudioManager::startWebStream()
{
#if defined(ARDUINO_ARCH_ESP32)
  if (webUrl.isEmpty() || webConnected)
    return;
  g_http.end();
  if (!g_http.begin(g_streamClient, webUrl))
    return;
  int code = g_http.GET();
  if (code == HTTP_CODE_OK)
    webConnected = true;
  else
    g_http.end();
#endif
}

void AudioManager::stopWebStream()
{
#if defined(ARDUINO_ARCH_ESP32)
  if (webConnected)
  {
    g_http.end();
    webConnected = false;
  }
#endif
}

void AudioManager::feedWebStream()
{
#if defined(ARDUINO_ARCH_ESP32)
  if (!webConnected)
  {
    startWebStream();
    if (!webConnected)
      return;
  }
  if (vsReady() && g_streamClient.available())
  {
    uint8_t buf[128];
    int n = g_streamClient.read(buf, sizeof(buf));
    if (n > 0)
      vsPlayer.sdiWrite(buf, n);
  }
#endif
}

void AudioManager::enableLineIn(bool en)
{
  uint16_t modeReg = vsPlayer.readRegister(SCI_MODE);
  if (en)
    modeReg |= SM_LINE1_BIT;
  else
    modeReg &= ~SM_LINE1_BIT;
  vsPlayer.writeRegister(SCI_MODE, modeReg);
}

bool AudioManager::vsReady() const
{
  return vsPlayer.readyForData();
}

void switchAudioSource(SourceMode targetSource, const String &pathOrUrl = "")
{
  // Always pause current source first
  audio.pause();

  // Set the new source
  audio.setSource(targetSource);

  // Handle playback/init based on target
  switch (targetSource)
  {
  case SOURCE_SD:
    if (!pathOrUrl.isEmpty())
    {
      playSdFile(pathOrUrl); // e.g. "/music/song.mp3"
    }
    break;

  case SOURCE_WEB:
    if (!pathOrUrl.isEmpty())
    {
      setWebUrl(pathOrUrl); // e.g. "http://stream-url"
    }
    startWeb();
    break;

  case SOURCE_RADIO:
    enterRadioMode(); // assumes tuner is preconfigured
    break;

  case SOURCE_SPIFFS:
    if (!pathOrUrl.isEmpty())
    {
      setAnnouncementSPIFFS("/announcements/en", pathOrUrl); // e.g. "welcome.mp3"
    }
    playAnnouncementFromSPIFFS_Selected();
    break;

  default:
    break;
  }
}

bool copySdFolderToSPIFFS(const String &sdFolderName, size_t maxSizeBytes = 512000)
{
  File sdDir = SD.open(sdFolderName);
  if (!sdDir || !sdDir.isDirectory())
  {
    Serial.println("��� SD folder not found or not a directory.");
    return false;
  }

  // Calculate total size
  size_t totalSize = 0;
  File file = sdDir.openNextFile();
  while (file)
  {
    if (!file.isDirectory())
      totalSize += file.size();
    file = sdDir.openNextFile();
  }

  if (totalSize > maxSizeBytes)
  {
    Serial.println("��� Folder size exceeds allowed limit.");
    return false;
  }

  // Clear existing files in SPIFFS /voice
  File voiceDir = SPIFFS.open("/voice");
  if (voiceDir && voiceDir.isDirectory())
  {
    File oldFile = voiceDir.openNextFile();
    while (oldFile)
    {
      String path = String("/voice/") + String(oldFile.name()).substring(7); // remove "/voice/"
      SPIFFS.remove(path);
      oldFile = voiceDir.openNextFile();
    }
  }
  else
  {
    SPIFFS.mkdir("/voice");
  }

  // Copy files
  sdDir = SD.open(sdFolderName); // reopen to reset iterator
  file = sdDir.openNextFile();
  while (file)
  {
    if (!file.isDirectory())
    {
      String destPath = "/voice/" + String(file.name()).substring(sdFolderName.length() + 1);
      File destFile = SPIFFS.open(destPath, FILE_WRITE);
      if (!destFile)
      {
        Serial.println("��� Failed to create file in SPIFFS: " + destPath);
        file.close();
        return false;
      }

      while (file.available())
      {
        uint8_t buf[128];
        size_t n = file.read(buf, sizeof(buf));
        destFile.write(buf, n);
      }

      destFile.close();
      Serial.println("��� Copied: " + destPath);
    }
    file = sdDir.openNextFile();
  }

  Serial.println("��� All files copied to SPIFFS /voice.");
  return true;
}
