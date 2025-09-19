/**
 * @file wifi_config.h
 * @brief WiFi configuration for radio web server
 * @details Modify these settings for your WiFi network
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// WiFi network credentials
// Change these to match your WiFi network
#define WIFI_SSID "Your_WiFi_Network_Name"
#define WIFI_PASSWORD "Your_WiFi_Password"

// Web server settings
#define WEB_SERVER_PORT 80

// Access Point mode settings (if you want to create a hotspot)
#define AP_MODE_ENABLED false
#define AP_SSID "FM_Radio_Control"
#define AP_PASSWORD "radio123"

#endif
