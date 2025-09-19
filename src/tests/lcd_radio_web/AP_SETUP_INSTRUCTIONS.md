# FM Radio Control - Access Point Setup

## How to Use

Your ESP32 FM Radio now creates its own WiFi network that you can connect to directly!

### Step-by-Step Instructions:

1. **Upload the code** to your ESP32 using PlatformIO with the `lcd_radio_web` environment
2. **Look for the WiFi network** called `FM_Radio_Control` on your device (phone, tablet, laptop)
3. **Connect to the network** using password: `radio123`
4. **Open a web browser** and navigate to: `http://192.168.4.1`
5. **Control your radio** through the web interface!

### Features:

- ? Real-time WebSocket communication
- ? No internet connection required
- ? Works on any device with WiFi and a web browser
- ? Multiple devices can connect simultaneously
- ? Instant synchronization between physical controls and web interface

### Network Details:

- **Network Name (SSID):** FM_Radio_Control
- **Password:** radio123
- **Radio Web Interface:** http://192.168.4.1
- **ESP32 IP Address:** 192.168.4.1 (default Access Point IP)

### Customization:

You can change the network name and password by modifying the values in `wifi_config.h`:

```cpp
#define AP_SSID "Your_Custom_Network_Name"
#define AP_PASSWORD "your_custom_password"
```

### Troubleshooting:

- Make sure you're connected to the `FM_Radio_Control` network
- Try refreshing the browser page if the interface doesn't load
- Check the serial monitor for connection status and IP address
- The ESP32 Access Point has a range of about 30-50 meters depending on environment
