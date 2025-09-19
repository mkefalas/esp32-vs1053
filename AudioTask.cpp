#include "AudioTask.h"
#include "Arduino.h"
#include "pins.h"
#include "setupDriver.h"

AudioTask audioTask;

AudioTask::AudioTask()
{
}

void AudioTask::begin()
{
    Serial.println("AudioManager: Starting initialization...");

    // I2C init (shared by FM radio & display)
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);

    // Reset VS1053 pins
    pinMode(VS1003B_RST_PIN, OUTPUT);
    pinMode(VS1003B_CS_PIN, OUTPUT);

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

    fmradio.start();
    fmradio.setMono(false);
    fmradio.setVolume(14);
    fmradio.setChannel(Setup.lastFrequency);

    // SD Card & VS1053 init
    SD_MMC.setPins(PIN_SD_MMC_CLK, PIN_SD_MMC_CMD, PIN_SD_MMC_D0);

    // Load volumes
    musicVolume = Setup.musicVolume;
    announcementVolume = Setup.announcementVolume;
    setHWVolume(musicVolume);

    // Load retrigger mode
    loadRetriggerMode();

    // Create queue & start task
    cmdQueue = xQueueCreate(QUEUE_LEN, sizeof(AudioCommand));
    xTaskCreate(taskEntry, "AudioTask", 8192, nullptr, configMAX_PRIORITIES - 1, nullptr);
}

void AudioTask::taskEntry(void *pv)
{
    audioTask.taskLoop();
}

void AudioTask::taskLoop()
{
    AudioCommand cmd;

    for (;;)
    {
        // 1) Non-blocking queue check
        if (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE)
        {
            handleCommand(cmd);
        }

        // 2) State machine
        switch (state)
        {
        case PlayState::PlaybackInit:
            initPlayback();
            state = PlayState::PlaybackPlay;
            break;

        case PlayState::PlaybackPlay:
            if (!stepPlayback())
            {
                onPlaybackFinished();
            }
            break;

        case PlayState::AnnouncementInit:
            initAnnouncement();
            state = PlayState::AnnouncementPlay;
            break;

        case PlayState::AnnouncementPlay:
            if (!stepAnnouncement())
            {
                restorePreviousSource();
                state = PlayState::Idle;
            }
            break;

        default:
            break;
        }

        // 3) Yield
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Command dispatcher
void AudioTask::handleCommand(const AudioCommand &cmd)
{
    if (mode == Mode::Test)
    {
        switch (cmd.type)
        {
        case AudioCommandType::StartTest:
        case AudioCommandType::StopTest:
        case AudioCommandType::PlayTestSource:
        case AudioCommandType::SetMusicVol:
        case AudioCommandType::SetAnnounceVol:
        case AudioCommandType::MuteToggle:
            // only these commands allowed
            break;
        default:
            // ignore everything else
            return;
        }
    }

    switch (cmd.type)
    {
    case AudioCommandType::PlayMusic:
        currentType = PlaybackType::File;
        memcpy(nextParamBuf, cmd.param.str, sizeof(nextParamBuf));
        nextValue = 0;
        state = PlayState::PlaybackInit;
        return;

    case AudioCommandType::StreamMusic:
        currentType = PlaybackType::Stream;
        memcpy(nextParamBuf, cmd.param.str, sizeof(nextParamBuf));
        state = PlayState::PlaybackInit;
        return;

    case AudioCommandType::PlayRadio:
        currentType = PlaybackType::Radio;
        memcpy(nextParamBuf, cmd.param.str, sizeof(nextParamBuf));
        state = PlayState::PlaybackInit;
        return;

    case AudioCommandType::PlayAnnouncement:
        savedStateBeforeTest = currentState;
        memcpy(nextParamBuf, cmd.param.str, sizeof(nextParamBuf));
        state = PlayState::AnnouncementInit;
        return;

    case AudioCommandType::StartTest:
        saveTestState();
        mode = Mode::Test;
        return;

    case AudioCommandType::PlayTestSource:
        // decide type by string content
        if (strncmp(cmd.param.str, "http", 4) == 0)
        {
            currentType = PlaybackType::Stream;
        }
        else if (strchr(cmd.param.str, '.') == nullptr)
        {
            currentType = PlaybackType::Radio;
        }
        else
        {
            currentType = PlaybackType::File;
        }
        memcpy(nextParamBuf, cmd.param.str, sizeof(nextParamBuf));
        nextValue = 0;
        state = PlayState::PlaybackInit;
        return;

    case AudioCommandType::StopTest:
        player.stopSong();
        mode = Mode::Normal;
        restorePreviousSource();
        state = PlayState::Idle;
        return;

    case AudioCommandType::NextTrack:
        if (currentType == PlaybackType::File)
        {
            String next = PlaylistManager::getInstance().getNext();
            next.toCharArray(nextParamBuf, sizeof(nextParamBuf));
            nextValue = 0;
            state = PlayState::PlaybackInit;
        }
        return;

    case AudioCommandType::PrevTrack:
        if (currentType == PlaybackType::File)
        {
            String prev = PlaylistManager::getInstance().getPrev();
            prev.toCharArray(nextParamBuf, sizeof(nextParamBuf));
            nextValue = 0;
            state = PlayState::PlaybackInit;
        }
        return;

    case AudioCommandType::Pause:
        if (state == PlayState::PlaybackPlay && currentType == PlaybackType::File)
        {
            state = PlayState::Idle;
        }
        return;

    case AudioCommandType::Resume:
        if (!currentState.filePath.isEmpty())
        {
            currentType = PlaybackType::File;
            currentState.filePath.toCharArray(nextParamBuf, sizeof(nextParamBuf));
            nextValue = currentState.filePos;
            state = PlayState::PlaybackInit;
        }
        return;

    case AudioCommandType::Stop:
        player.stop();
        currentState = {"", 0, "", 0.0f};
        state = PlayState::Idle;
        return;

    case AudioCommandType::SetMusicVol:
        musicVolume = constrain(cmd.param.value, 0, 100);
        VolumeManager::getInstance().setMusicVolume(musicVolume);
        VolumeManager::getInstance().saveToFlash();
        if (!muted)
            setHWVolume(musicVolume);
        return;

    case AudioCommandType::SetAnnounceVol:
        announcementVolume = constrain(cmd.param.value, 0, 100);
        VolumeManager::getInstance().setAnnouncementVolume(announcementVolume);
        VolumeManager::getInstance().saveToFlash();
        return;

    case AudioCommandType::MuteToggle:
        toggleMute();
        return;

    default:
        return;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Unified playback init/step
void AudioTask::initPlayback()
{
    player.stop();
    setHWVolume(musicVolume);

    switch (currentType)
    {
    case PlaybackType::File:
        // open SD or SPIFFS
        if (strncmp(nextParamBuf, "/spiffs/", 8) == 0)
        {
            fileHandle = SPIFFS.open(nextParamBuf + 8);
        }
        else
        {
            fileHandle = SD_MMC.open(nextParamBuf);
        }
        if (fileHandle)
        {
            currentState = {String(nextParamBuf), nextValue, "", 0.0f};
            if (nextValue)
            {
                fileHandle.seek(nextValue);
            }
        }
        break;

    case PlaybackType::Stream: {
        String url = nextParamBuf;
        currentState = {"", 0, url, 0.0f};
        int idx = url.indexOf('/', 7);
        String host = url.substring(7, idx);
        String path = url.substring(idx);
        httpClient.connect(host.c_str(), 80);
        httpClient.print(String("GET ") + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n");
        while (httpClient.connected())
        {
            if (httpClient.read() == '\r' && httpClient.peek() == '\n')
            {
                httpClient.read();
                break;
            }
        }
        break;
    }

    case PlaybackType::Radio:
        currentState = {"", 0, "", atof(nextParamBuf)};
        fmRadio.setFrequency(currentState.radioFreq);
        player.sciWriteRegister(SCI_MODE, SM_LINE1_ENABLE | SM_SDINEW);
        break;

    default:
        break;
    }
}

bool AudioTask::stepPlayback()
{
    switch (currentType)
    {
    case PlaybackType::File:
        if (!fileHandle.available())
        {
            fileHandle.close();
            return false;
        }
        fileHandle.read(buf, BUF_SZ);
        player.playChunk(buf, BUF_SZ);
        currentState.filePos = fileHandle.position();
        return true;

    case PlaybackType::Stream:
        if (!httpClient.connected())
        {
            httpClient.stop();
            return false;
        }
        if (httpClient.available())
        {
            int n = httpClient.read(buf, BUF_SZ);
            player.playChunk(buf, n);
        }
        return true;

    case PlaybackType::Radio:
        return true;

    default:
        return false;
    }
}

void AudioTask::onPlaybackFinished()
{
    if (currentType == PlaybackType::File && mode == Mode::Normal)
    {
        String next = PlaylistManager::getInstance().getNext();
        next.toCharArray(nextParamBuf, sizeof(nextParamBuf));
        nextValue = 0;
        state = PlayState::PlaybackInit;
    }
    else
    {
        state = PlayState::Idle;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Announcement
void AudioTask::initAnnouncement()
{
    annHandle = SD.open(nextParamBuf);
    if (annHandle)
    {
        setHWVolume(announcementVolume);
        player.stopSong();
    }
}

bool AudioTask::stepAnnouncement()
{
    if (!annHandle.available())
    {
        annHandle.close();
        return false;
    }
    annHandle.read(buf, BUF_SZ);
    player.playChunk(buf, BUF_SZ);
    return true;
}

void AudioTask::restorePreviousSource()
{
    currentState = savedStateBeforeTest;
    switch (currentType)
    {
    case PlaybackType::Stream:
        memcpy(nextParamBuf, currentState.streamUrl.c_str(), sizeof(nextParamBuf));
        state = PlayState::PlaybackInit;
        break;
    case PlaybackType::Radio:
        snprintf(nextParamBuf, sizeof(nextParamBuf), "%f", currentState.radioFreq);
        state = PlayState::PlaybackInit;
        break;
    case PlaybackType::File:
        currentState.filePath.toCharArray(nextParamBuf, sizeof(nextParamBuf));
        nextValue = currentState.filePos;
        state = PlayState::PlaybackInit;
        break;
    default:
        state = PlayState::Idle;
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test Mode
void AudioTask::saveTestState()
{
    savedStateBeforeTest = currentState;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Volume & Mute
void AudioTask::setHWVolume(uint8_t vol)
{
    uint8_t v = map(vol, 0, 100, 255, 0);
    player.setVolume(v);
}

void AudioTask::toggleMute()
{
    if (muted)
    {
        muted = false;
        setHWVolume(backupMusicVol);
    }
    else
    {
        muted = true;
        backupMusicVol = musicVolume;
        backupAnnVol = announcementVolume;
        player.setVolume(255);
    }
}

bool AudioTask::isMuted() const
{
    return muted;
}

// ─────────────────────────────────────────────────────────────────────────────
//  RetriggerMode persistence
void AudioTask::loadRetriggerMode()
{
    retriggerMode = Setup.retriggerMode;
}

void AudioTask::saveRetriggerMode()
{
    Setup.retriggerMode = retriggerMode;
}

void AudioTask::setRetriggerMode(RetriggerMode m)
{
    if (m != retriggerMode)
    {
        retriggerMode = m;
        saveRetriggerMode();
    }
}

RetriggerMode AudioTask::getRetriggerMode() const
{
    return retriggerMode;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Queries
Mode AudioTask::getMode() const
{
    return mode;
}

PlaybackType AudioTask::getPlaybackType() const
{
    return currentType;
}

PlaybackState AudioTask::getCurrentState() const
{
    return currentState;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API wrappers
void AudioTask::playMusic(const char *p)
{
    AudioCommand cmd{AudioCommandType::PlayMusic};
    strncpy(cmd.param.str, p, sizeof(cmd.param.str));
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::streamMusic(const char *p)
{
    AudioCommand cmd{AudioCommandType::StreamMusic};
    strncpy(cmd.param.str, p, sizeof(cmd.param.str));
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::playRadio(const char *p)
{
    AudioCommand cmd{AudioCommandType::PlayRadio};
    strncpy(cmd.param.str, p, sizeof(cmd.param.str));
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::playAnnouncement(const char *p)
{
    AudioCommand cmd{AudioCommandType::PlayAnnouncement};
    strncpy(cmd.param.str, p, sizeof(cmd.param.str));
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::startTest()
{
    AudioCommand cmd{AudioCommandType::StartTest};
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::playTestSource(const char *p)
{
    AudioCommand cmd{AudioCommandType::PlayTestSource};
    strncpy(cmd.param.str, p, sizeof(cmd.param.str));
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::stopTest()
{
    AudioCommand cmd{AudioCommandType::StopTest};
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::nextTrack()
{
    AudioCommand cmd{AudioCommandType::NextTrack};
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::prevTrack()
{
    AudioCommand cmd{AudioCommandType::PrevTrack};
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::pause()
{
    AudioCommand cmd{AudioCommandType::Pause};
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::resume()
{
    AudioCommand cmd{AudioCommandType::Resume};
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::stop()
{
    AudioCommand cmd{AudioCommandType::Stop};
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::setMusicVolume(uint8_t v)
{
    AudioCommand cmd{AudioCommandType::SetMusicVol};
    cmd.param.value = v;
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::setAnnouncementVolume(uint8_t v)
{
    AudioCommand cmd{AudioCommandType::SetAnnounceVol};
    cmd.param.value = v;
    xQueueSend(cmdQueue, &cmd, 0);
}

void AudioTask::toggleMute()
{
    AudioCommand cmd{AudioCommandType::MuteToggle};
    xQueueSend(cmdQueue, &cmd, 0);
}

AudioFormat AudioTask::detectFormat(File &file, uint8_t *buf)
{
    // read up to 44 bytes at offset 0, then restore position
    auto pos = file.position();
    file.seek(0);
    auto n = file.read(buf, 44);
    file.seek(pos);

    if (n < 4)
        return FORMAT_UNKNOWN;

    // WAV: ��RIFF�� + ��WAVE��, then optional fmt tag
    if (n >= 12 && !memcmp(buf, "RIFF", 4) && !memcmp(buf + 8, "WAVE", 4))
    {
        if (n >= 22)
        {
            auto fmt = uint16_t(buf[20]) | uint16_t(buf[21]) << 8;
            return (fmt == 1) ? FORMAT_WAV_PCM : (fmt == 0x0011) ? FORMAT_WAV_IMA : FORMAT_UNKNOWN;
        }
        return FORMAT_WAV_PCM;
    }

    // OGG, ID3
    if (!memcmp(buf, "OggS", 4))
        return FORMAT_OGG;
    if (!memcmp(buf, "ID3", 3))
        return FORMAT_MP3;

    // MP3 sync+header check
    if (buf[0] == 0xFF && (buf[1] & 0xE0) == 0xE0)
    {
        auto v = (buf[1] >> 3) & 3, l = (buf[1] >> 1) & 3;
        if (v != 1 && l != 0)
            return FORMAT_MP3;
    }
    // fallback scan for sync in buffer
    for (size_t i = 0; i + 1 < n; ++i)
    {
        if (mp3Sync(buf[i], buf[i + 1]))
        {
            auto v = (buf[i + 1] >> 3) & 3, l = (buf[i + 1] >> 1) & 3;
            if (v != 1 && l != 0)
                return FORMAT_MP3;
        }
    }

    return FORMAT_UNKNOWN;
}

void AudioTask::scanRootFolder()
{
    playlist.clear();
    File root = SD_MMC.open("/");
    if (!root || !root.isDirectory())
        return;

    uint8_t buf[44];
    for (auto entry = root.openNextFile(); entry; entry = root.openNextFile())
    {
        if (!entry.isDirectory() && detectFormat(entry, buf) != FORMAT_UNKNOWN)
        {
            playlist.emplace_back(entry.name());
        }
        entry.close();
    }
    currentIndex = 0;
}
