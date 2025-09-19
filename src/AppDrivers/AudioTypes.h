/**
 * @file AudioTypes.h
 * @brief Shared audio type definitions for multi-source audio system
 * @author ESP32 Audio System
 * @date 2025
 * 
 * This header contains shared enums and constants used by both AudioManager
 * and AudioTask to avoid circular dependencies.
 */

#pragma once

/**
 * @brief Audio type priority levels for interruption control
 */
enum AudioType {
    AUDIO_TYPE_MUSIC = 0,        ///< Lowest priority - can be interrupted
    AUDIO_TYPE_ANNOUNCEMENT = 1, ///< Medium priority - interrupts music
    AUDIO_TYPE_ALARM = 2         ///< Highest priority - interrupts everything
};
