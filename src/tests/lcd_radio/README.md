# SI4703 FM Radio with Web Interface

This project adds web server capabilities to your SI4703 FM radio controller, allowing you to control the radio through a web browser in addition to the physical controls.

## Features

### Physical Controls (Original)
- ? Seek up/down with NEXT/PREVIOUS buttons
- ? Volume control with UP/DOWN buttons  
- ? Memory preset recall and store via opto-isolators
- ? On/Off control via opto-isolator
- ? OLED display showing frequency, volume, and RSSI

### Web Interface (New)
- ?? **Web-based radio control** - Control everything through your browser
- ?? **Mobile-friendly interface** - Responsive design works on phones and tablets
- ?? **Real-time updates** - Frequency, volume, and RSSI update automatically
- ?? **Seek controls** - Seek up/down buttons just like physical interface
- ?? **Volume control** - Slider and buttons for volume adjustment
- ?? **Memory presets** - 6 memory buttons (M1-M6) with long-press to store
- ?? **Mute control** - Toggle mute on/off
- ?? **Modern UI** - Clean, modern interface with visual feedback

## Setup Instructions

### 1. Configure WiFi
Edit `wifi_config.h` and update your WiFi credentials:
```cpp
#define WIFI_SSID "Your_WiFi_Network_Name"
#define WIFI_PASSWORD "Your_WiFi_Password"
```

### 2. Required Libraries
Add these libraries to your PlatformIO project:
```ini
lib_deps = 
    adafruit/Adafruit SSD1306
    adafruit/Adafruit GFX Library
    ; Your existing SI470X library
    ArduinoJson
```

### 3. Upload and Connect
1. Upload the code to your ESP32
2. Open Serial Monitor to see the assigned IP address
3. Connect to the same WiFi network with your device
4. Open a web browser and navigate to the IP address shown

## Web Interface Usage

### Basic Controls
- **Seek Buttons**: Click "? SEEK" or "SEEK ?" to seek stations
- **Volume Buttons**: Use "VOL -" and "VOL +" for quick volume changes
- **Volume Slider**: Drag the slider for precise volume control
- **Mute Button**: Click to toggle mute on/off

### Memory Presets
- **Recall**: Quick click on M1-M6 buttons to recall stored frequencies
- **Store**: Long press (hold for 1 second) on M1-M6 to store current frequency
- The button will flash green when successfully stored

### Status Display
- **Frequency**: Large digital display shows current frequency
- **RSSI**: Signal strength indicator
- **Volume**: Shows current volume level and percentage

## API Endpoints

The web server provides a REST API for advanced integration:

### Status
- `GET /api/status` - Get current radio status (frequency, volume, RSSI, mute)

### Control
- `POST /api/seek/up` - Seek to next station
- `POST /api/seek/down` - Seek to previous station
- `POST /api/volume/up` - Increase volume
- `POST /api/volume/down` - Decrease volume
- `POST /api/volume/set/{0-15}` - Set specific volume level
- `POST /api/mute/toggle` - Toggle mute state

### Memory
- `POST /api/memory/recall/{0-11}` - Recall memory preset
- `POST /api/memory/store/{0-11}` - Store current frequency to memory

## Technical Details

### Architecture
- **Main Controller**: ESP32 running original radio control logic
- **Web Server**: Built-in HTTP server on port 80
- **API Layer**: RESTful JSON API for all radio functions
- **Callback System**: Web requests trigger the same functions as physical controls

### Memory Mapping
The web interface uses the first 6 memory slots (0-5) for the M1-M6 buttons, but the system supports up to 12 memory presets total, matching your opto-isolator inputs.

### Network Configuration
- **Station Mode**: Connects to your existing WiFi network
- **IP Address**: Automatically assigned by DHCP
- **Port**: 80 (standard HTTP)
- **Concurrent Connections**: Supports multiple simultaneous web clients

## Troubleshooting

### WiFi Connection Issues
1. Verify SSID and password in `wifi_config.h`
2. Check that your WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
3. Monitor Serial output for connection status

### Web Interface Not Loading
1. Confirm the IP address from Serial Monitor
2. Ensure your device is on the same WiFi network
3. Try accessing `http://[IP_ADDRESS]` directly

### Controls Not Working
1. Check Serial Monitor for error messages
2. Verify that the radio is properly initialized
3. Ensure callback functions are properly set

## Future Enhancements

Potential improvements you could add:
- ?? **WebSocket support** for real-time updates without polling
- ??? **RDS data display** if your SI4703 supports it
- ?? **Station scanning** and automatic preset population
- ?? **Custom themes** and color schemes
- ?? **Signal quality graphs** and station logging
- ?? **Authentication** for secure access

## File Structure

```
??? radio_screen.cpp      # Main application with web integration
??? webserver.h          # Web server interface
??? webserver.cpp        # Web server implementation  
??? wifi_config.h        # WiFi configuration
??? README.md           # This file
```

The web interface provides the same functionality as your physical controls while adding the convenience of remote access through any web browser on your network.
