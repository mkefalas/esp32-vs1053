#include <Arduino.h>
#include <WiFi.h>

// Wi-Fi credentials (replace with your actual credentials)
const char* WIFI_SSID = "MKEF";
const char* WIFI_PASS = "1234567cbe";

void printWiFiStatus() {
  Serial.print("WiFi Status: ");
  switch (WiFi.status()) {
    case WL_IDLE_STATUS:
      Serial.println("Idle");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("Connect Failed");
      break;
    case WL_CONNECTED:
      Serial.println("Connected");
      break;
    case WL_DISCONNECTED:
      Serial.println("Disconnected");
      break;
    case WL_NO_SSID_AVAIL:
      Serial.println("SSID Not Available");
      break;
    default:
      Serial.printf("Unknown Status: %d\n", WiFi.status());
      break;
  }
}

void scanNetworks() {
  Serial.println("\n=== WiFi Scan ===");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found!");
  } else {
    Serial.printf("%d networks found:\n", n);
    for (int i = 0; i < n; i++) {
      Serial.printf("  %2d: %s (RSSI %d dB) %s\n",
                    i + 1,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i),
                    WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "(Open)" : ""
                   );
    }
  }
}

void connectWiFi() {
  Serial.println("\n=== WiFi Connect ===");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();   // clear any old state
  delay(100);

  Serial.printf("Connecting to %s …\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("? Connected! IP = %s\n", WiFi.localIP().toString().c_str());
    printWiFiStatus();
  } else {
    Serial.printf("? Failed to connect, status=%d\n", WiFi.status());
    printWiFiStatus();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  scanNetworks(); // Initial scan on startup
  connectWiFi();  // Initial connection attempt on startup
}

void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'r' || command == 'R') {
      Serial.println("\n--- Rescanning and Reconnecting (triggered by 'r' key) ---");
      scanNetworks();
      connectWiFi();
    }
  }
  delay(100); // Small delay to avoid busy-waiting on serial input
}