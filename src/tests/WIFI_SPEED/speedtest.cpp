#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

const char* SSID = "MKEF";
const char* PSK = "1234567cbe";
const char* STREAM_URL = "http://cast3.radiohost.ovh:8352/"; // Change to your station

WiFiClient client;
WiFiClientSecure secureClient;
WiFiClient* clientPtr = nullptr;

void connectToStream(const char* url) {
    Serial.printf("Connecting to stream: %s\n", url);

    // Parse host and path
    String surl(url);
    String host, path;
    int port = 80;

    if (surl.startsWith("https://")) {
        surl = surl.substring(8);
        port = 443;
    } else if (surl.startsWith("http://")) {
        surl = surl.substring(7);
        port = 80;
    }

    int slash = surl.indexOf('/');
    host = surl.substring(0, slash);
    path = surl.substring(slash);

    if (port == 443) {
        secureClient.setInsecure(); // Accept all certificates (for testing)
        clientPtr = &secureClient;
    } else {
        clientPtr = &client;
    }

    if (!clientPtr->connect(host.c_str(), port)) {
        Serial.println("Connection failed!");
        while (1) delay(1000);
    }

    // Send HTTP GET with more headers to mimic a real player
    clientPtr->print(String("GET ") + path + " HTTP/1.1\r\n" +
    "Host: " + host + "\r\n" +
    "User-Agent: VLC/3.0.16 LibVLC/3.0.16\r\n" + // Try VLC's UA
    "Accept: */*\r\n" +
    "Icy-MetaData: 1\r\n" +
    "Connection: keep-alive\r\n\r\n");

    // Skip HTTP headers
    unsigned long startWait = millis();
    String redirectURL;
    while (clientPtr->connected()) {
        String line = clientPtr->readStringUntil('\n');
        Serial.println(line); // Print each header line
        if (line.startsWith("Location: ")) {
            redirectURL = line.substring(10);
            redirectURL.trim();
        }
        if (line == "\r") break;
        if (millis() - startWait > 5000) {
            Serial.println("Header skip timeout!");
            break;
        }
    }
    if (redirectURL.length() > 0) {
        Serial.print("Redirecting to: ");
        Serial.println(redirectURL);
        clientPtr->stop(); // Close current connection
        delay(100);
        connectToStream(redirectURL.c_str());
        return;
    }
    Serial.println("Connected and headers skipped. Measuring stream rate...");

    // Wait for data to become available
    startWait = millis();
    while (clientPtr->connected() && !clientPtr->available() && millis() - startWait < 20000) {
        delay(10);
    }
    if (clientPtr->available()) {
        Serial.println("Data is available from stream!");
    } else {
        Serial.println("No data received from stream after 5 seconds.");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Connecting to WiFi...");
    WiFi.begin(SSID, PSK);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");

    connectToStream(STREAM_URL);
}

void loop() {
    static unsigned long lastMeasure = 0;
    static size_t bytesRead = 0;

    // Read available bytes from stream
    while (clientPtr && clientPtr->connected() && clientPtr->available()) {
        uint8_t buf[256];
        size_t n = clientPtr->read(buf, sizeof(buf));
        bytesRead += n;
    }

    // Every second, print the flow rate
    if (millis() - lastMeasure > 1000) {
        lastMeasure = millis();
        float kbps = (bytesRead * 8.0) / 1000.0; // kilobits per second
        int rssi = WiFi.RSSI();
        Serial.printf("Web stream flow rate: %.2f kbps | RSSI: %d dBm\n", kbps, rssi);
        bytesRead = 0;
    }
}