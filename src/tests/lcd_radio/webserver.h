/**
 * @file webserver.h
 * @brief Web server interface for SI4703 radio control with WebSocket support
 * @details Provides web-based control interface with real-time WebSocket communication
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

/**
 * @brief Initialize and start the web server
 * @param ssid WiFi network name
 * @param password WiFi password
 * @return true if successful, false otherwise
 */
bool initWebServer(const char* ssid, const char* password);

/**
 * @brief Handle web server requests (call in main loop)
 */
void handleWebServer();

/**
 * @brief Get current WiFi connection status
 * @return true if connected, false otherwise
 */
bool isWiFiConnected();

/**
 * @brief Get the local IP address
 * @return IP address as string
 */
String getLocalIP();

/**
 * @brief Update web clients with current radio status
 * @details Broadcasts current status to all connected WebSocket clients
 */
void broadcastRadioStatus();

/**
 * @brief Send status update to specific WebSocket client
 * @param clientId WebSocket client ID
 */
void sendStatusToClient(uint8_t clientId);

/**
 * @brief Set callback functions for radio control
 * @param seekUp Function to seek up
 * @param seekDown Function to seek down
 * @param volumeUp Function to increase volume
 * @param volumeDown Function to decrease volume
 * @param setFreq Function to set frequency
 * @param getFreq Function to get current frequency
 * @param getVol Function to get current volume
 * @param getRssi Function to get RSSI
 * @param setMute Function to set mute state
 */
void setRadioCallbacks(
    void (*seekUp)(),
    void (*seekDown)(),
    void (*volumeUp)(),
    void (*volumeDown)(),
    void (*setFreq)(uint16_t),
    uint16_t (*getFreq)(),
    uint8_t (*getVol)(),
    uint8_t (*getRssi)(),
    void (*setMute)(bool)
);

/**
 * @brief Set additional radio callbacks for direct control
 * @param setVolume Function to set volume directly
 * @param recallMemory Function to recall memory preset
 * @param storeMemory Function to store memory preset
 */
void setAdditionalRadioCallbacks(
    void (*setVolume)(uint8_t),
    void (*recallMemory)(uint8_t),
    void (*storeMemory)(uint8_t)
);

#endif
