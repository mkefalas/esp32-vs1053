/**
 * @file webserver.cpp
 * @brief Web server implementation for SI4703 radio control with WebSocket support
 */

#include "webserver.h"

// Web server and WebSocket instances
WebServer server(80);
WebSocketsServer webSocket(81);  // WebSocket on port 81

// Radio control callback functions
void (*cb_seekUp)() = nullptr;
void (*cb_seekDown)() = nullptr;
void (*cb_volumeUp)() = nullptr;
void (*cb_volumeDown)() = nullptr;
void (*cb_setFreq)(uint16_t) = nullptr;
void (*cb_setVolume)(uint8_t) = nullptr;  // Added volume setter
uint16_t (*cb_getFreq)() = nullptr;
uint8_t (*cb_getVol)() = nullptr;
uint8_t (*cb_getRssi)() = nullptr;
void (*cb_setMute)(bool) = nullptr;
void (*cb_recallMemory)(uint8_t) = nullptr;  // Added memory recall
void (*cb_storeMemory)(uint8_t) = nullptr;   // Added memory store

// WiFi and WebSocket state
String wifi_ssid = "";
String wifi_password = "";
bool wifiConnected = false;
bool muteState = false;

/**
 * @brief WebSocket event handler
 * @param num Client number
 * @param type Event type
 * @param payload Message payload
 * @param length Message length
 */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("WebSocket client #%u disconnected\n", num);
            break;
            
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("WebSocket client #%u connected from %d.%d.%d.%d\n", 
                         num, ip[0], ip[1], ip[2], ip[3]);
            // Send current status to new client
            sendStatusToClient(num);
            break;
        }
        
        case WStype_TEXT: {
            Serial.printf("WebSocket received: %s\n", payload);
            
            // Parse JSON command
            DynamicJsonDocument doc(200);
            DeserializationError error = deserializeJson(doc, payload);
            
            if (error) {
                Serial.print("JSON parse error: ");
                Serial.println(error.c_str());
                return;
            }
            
            String command = doc["cmd"];
            
            // Handle commands
            if (command == "seekUp" && cb_seekUp) {
                cb_seekUp();
                broadcastRadioStatus();
            }
            else if (command == "seekDown" && cb_seekDown) {
                cb_seekDown();
                broadcastRadioStatus();
            }
            else if (command == "volumeUp" && cb_volumeUp) {
                cb_volumeUp();
                broadcastRadioStatus();
            }
            else if (command == "volumeDown" && cb_volumeDown) {
                cb_volumeDown();
                broadcastRadioStatus();
            }
            else if (command == "setVolume" && cb_setVolume) {
                uint8_t vol = doc["value"];
                if (vol <= 15) {
                    cb_setVolume(vol);
                    broadcastRadioStatus();
                }
            }
            else if (command == "toggleMute" && cb_setMute) {
                muteState = !muteState;
                cb_setMute(muteState);
                broadcastRadioStatus();
            }
            else if (command == "recallMemory" && cb_recallMemory) {
                uint8_t slot = doc["slot"];
                if (slot < 12) {
                    cb_recallMemory(slot);
                    broadcastRadioStatus();
                }
            }
            else if (command == "storeMemory" && cb_storeMemory) {
                uint8_t slot = doc["slot"];
                if (slot < 12) {
                    cb_storeMemory(slot);
                    // Send confirmation
                    DynamicJsonDocument response(100);
                    response["type"] = "memoryStored";
                    response["slot"] = slot;
                    String responseStr;
                    serializeJson(response, responseStr);
                    webSocket.sendTXT(num, responseStr);
                }
            }
            else if (command == "getStatus") {
                sendStatusToClient(num);
            }
            break;
        }
        
        default:
            break;
    }
}

/**
 * @brief Broadcast current radio status to all connected clients
 */
void broadcastRadioStatus() {
    if (!cb_getFreq || !cb_getVol || !cb_getRssi) return;
    
    DynamicJsonDocument doc(300);
    doc["type"] = "status";
    doc["frequency"] = cb_getFreq();
    doc["volume"] = cb_getVol();
    doc["rssi"] = cb_getRssi();
    doc["muted"] = muteState;
    
    String message;
    serializeJson(doc, message);
    webSocket.broadcastTXT(message);
}

/**
 * @brief Send status to specific WebSocket client
 * @param clientId Client ID to send to
 */
void sendStatusToClient(uint8_t clientId) {
    if (!cb_getFreq || !cb_getVol || !cb_getRssi) return;
    
    DynamicJsonDocument doc(300);
    doc["type"] = "status";
    doc["frequency"] = cb_getFreq();
    doc["volume"] = cb_getVol();
    doc["rssi"] = cb_getRssi();
    doc["muted"] = muteState;
    
    String message;
    serializeJson(doc, message);
    webSocket.sendTXT(clientId, message);
}
/**
 * @brief HTML page for radio control interface with WebSocket support
 */
const char* radioHTML = R"(
<!DOCTYPE html>
<html>
<head>
    <title>FM Radio Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            margin: 20px; 
            background-color: #f0f0f0; 
        }
        .container { 
            max-width: 400px; 
            margin: 0 auto; 
            background: white; 
            padding: 20px; 
            border-radius: 10px; 
            box-shadow: 0 4px 6px rgba(0,0,0,0.1); 
        }
        .title { 
            text-align: center; 
            color: #333; 
            margin-bottom: 20px; 
        }
        .connection-status {
            text-align: center;
            padding: 5px;
            border-radius: 3px;
            margin-bottom: 15px;
            font-weight: bold;
        }
        .connected { background: #d4edda; color: #155724; }
        .disconnected { background: #f8d7da; color: #721c24; }
        .freq-display { 
            font-size: 32px; 
            text-align: center; 
            background: #000; 
            color: #0f0; 
            padding: 15px; 
            border-radius: 5px; 
            margin: 20px 0; 
            font-family: monospace; 
        }
        .controls { 
            display: grid; 
            grid-template-columns: 1fr 1fr; 
            gap: 10px; 
            margin: 20px 0; 
        }
        .btn { 
            padding: 15px; 
            font-size: 16px; 
            border: none; 
            border-radius: 5px; 
            cursor: pointer; 
            background: #007bff; 
            color: white; 
            transition: all 0.2s; 
        }
        .btn:hover { 
            background: #0056b3; 
            transform: translateY(-1px);
        }
        .btn:active { 
            background: #004085; 
            transform: translateY(0);
        }
        .btn:disabled { 
            background: #6c757d; 
            cursor: not-allowed; 
            transform: none;
        }
        .status { 
            display: flex; 
            justify-content: space-between; 
            margin: 15px 0; 
            padding: 10px; 
            background: #f8f9fa; 
            border-radius: 5px; 
        }
        .volume-container { 
            display: flex; 
            align-items: center; 
            gap: 10px; 
            margin: 15px 0; 
        }
        .volume-slider { 
            flex: 1; 
            height: 6px; 
            border-radius: 3px; 
            background: #ddd; 
            outline: none; 
            -webkit-appearance: none; 
        }
        .volume-slider::-webkit-slider-thumb { 
            -webkit-appearance: none; 
            appearance: none; 
            width: 20px; 
            height: 20px; 
            border-radius: 50%; 
            background: #007bff; 
            cursor: pointer; 
        }
        .memory-buttons { 
            display: grid; 
            grid-template-columns: repeat(3, 1fr); 
            gap: 8px; 
            margin: 20px 0; 
        }
        .memory-btn { 
            padding: 10px; 
            font-size: 14px; 
            border: 1px solid #007bff; 
            background: white; 
            color: #007bff; 
            border-radius: 3px; 
            cursor: pointer; 
            transition: all 0.2s;
            position: relative;
        }
        .memory-btn:hover { 
            background: #007bff; 
            color: white; 
        }
        .memory-btn.storing {
            background: #ffc107;
            color: #212529;
            border-color: #ffc107;
        }
        .memory-btn.stored {
            background: #28a745;
            color: white;
            border-color: #28a745;
        }
        .mute-btn { 
            background: #dc3545; 
            grid-column: span 2; 
        }
        .mute-btn:hover { 
            background: #c82333; 
        }
        .mute-btn.muted { 
            background: #28a745; 
        }
        .mute-btn.muted:hover { 
            background: #218838; 
        }
    </style>
</head>
<body>
    <div class="container">
        <h1 class="title">?? FM Radio Control</h1>
        
        <div id="connectionStatus" class="connection-status disconnected">
            ?? Connecting...
        </div>
        
        <div class="freq-display" id="frequency">---.-- MHz</div>
        
        <div class="controls">
            <button class="btn" id="seekDownBtn" onclick="seekDown()" disabled>? SEEK</button>
            <button class="btn" id="seekUpBtn" onclick="seekUp()" disabled>SEEK ?</button>
            <button class="btn" id="volDownBtn" onclick="volumeDown()" disabled>VOL -</button>
            <button class="btn" id="volUpBtn" onclick="volumeUp()" disabled>VOL +</button>
        </div>
        
        <div class="volume-container">
            <span>Volume:</span>
            <input type="range" min="0" max="15" value="5" class="volume-slider" 
                   id="volumeSlider" onchange="setVolume(this.value)" disabled>
            <span id="volumePercent">---%</span>
        </div>
        
        <div class="status">
            <span>RSSI: <span id="rssi">--</span></span>
            <span>Volume: <span id="volume">--</span>/15</span>
        </div>
        
        <div class="memory-buttons">
            <button class="memory-btn" onclick="recallMemory(0)" disabled>M1</button>
            <button class="memory-btn" onclick="recallMemory(1)" disabled>M2</button>
            <button class="memory-btn" onclick="recallMemory(2)" disabled>M3</button>
            <button class="memory-btn" onclick="recallMemory(3)" disabled>M4</button>
            <button class="memory-btn" onclick="recallMemory(4)" disabled>M5</button>
            <button class="memory-btn" onclick="recallMemory(5)" disabled>M6</button>
        </div>
        
        <button class="btn mute-btn" id="muteBtn" onclick="toggleMute()" disabled>?? UNMUTED</button>
    </div>

    <script>
        let ws;
        let isConnected = false;
        let isMuted = false;
        
        // WebSocket connection
        function connectWebSocket() {
            const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = protocol + '//' + location.hostname + ':81';
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = function(event) {
                console.log('WebSocket connected');
                isConnected = true;
                updateConnectionStatus();
                enableControls();
                // Request initial status
                sendCommand('getStatus');
            };
            
            ws.onmessage = function(event) {
                const data = JSON.parse(event.data);
                handleWebSocketMessage(data);
            };
            
            ws.onclose = function(event) {
                console.log('WebSocket disconnected');
                isConnected = false;
                updateConnectionStatus();
                disableControls();
                // Attempt to reconnect after 3 seconds
                setTimeout(connectWebSocket, 3000);
            };
            
            ws.onerror = function(error) {
                console.log('WebSocket error:', error);
            };
        }
        
        // Handle incoming WebSocket messages
        function handleWebSocketMessage(data) {
            if (data.type === 'status') {
                updateDisplay(data);
            } else if (data.type === 'memoryStored') {
                showMemoryStored(data.slot);
            }
        }
        
        // Update display with current status
        function updateDisplay(data) {
            document.getElementById('frequency').textContent = 
                (data.frequency / 100).toFixed(2) + ' MHz';
            document.getElementById('volume').textContent = data.volume;
            document.getElementById('volumePercent').textContent = 
                Math.round(data.volume * 100 / 15) + '%';
            document.getElementById('volumeSlider').value = data.volume;
            document.getElementById('rssi').textContent = data.rssi;
            
            const muteBtn = document.getElementById('muteBtn');
            if (data.muted !== isMuted) {
                isMuted = data.muted;
                if (isMuted) {
                    muteBtn.textContent = '?? MUTED';
                    muteBtn.classList.add('muted');
                } else {
                    muteBtn.textContent = '?? UNMUTED';
                    muteBtn.classList.remove('muted');
                }
            }
        }
        
        // Update connection status indicator
        function updateConnectionStatus() {
            const statusEl = document.getElementById('connectionStatus');
            if (isConnected) {
                statusEl.textContent = '?? Connected';
                statusEl.className = 'connection-status connected';
            } else {
                statusEl.textContent = '?? Disconnected';
                statusEl.className = 'connection-status disconnected';
            }
        }
        
        // Enable/disable controls based on connection
        function enableControls() {
            document.querySelectorAll('button, input').forEach(el => {
                el.disabled = false;
            });
        }
        
        function disableControls() {
            document.querySelectorAll('button, input').forEach(el => {
                el.disabled = true;
            });
        }
        
        // Send command via WebSocket
        function sendCommand(cmd, value = null, slot = null) {
            if (!isConnected) return;
            
            const message = { cmd: cmd };
            if (value !== null) message.value = value;
            if (slot !== null) message.slot = slot;
            
            ws.send(JSON.stringify(message));
        }
        
        // Control functions
        function seekUp() {
            sendCommand('seekUp');
        }
        
        function seekDown() {
            sendCommand('seekDown');
        }
        
        function volumeUp() {
            sendCommand('volumeUp');
        }
        
        function volumeDown() {
            sendCommand('volumeDown');
        }
        
        function setVolume(vol) {
            sendCommand('setVolume', parseInt(vol));
        }
        
        function toggleMute() {
            sendCommand('toggleMute');
        }
        
        function recallMemory(slot) {
            sendCommand('recallMemory', null, slot);
        }
        
        function storeMemory(slot) {
            sendCommand('storeMemory', null, slot);
        }
        
        function showMemoryStored(slot) {
            const btn = document.querySelectorAll('.memory-btn')[slot];
            btn.classList.add('stored');
            setTimeout(() => btn.classList.remove('stored'), 1000);
        }
        
        // Long press for memory store
        let pressTimers = {};
        document.querySelectorAll('.memory-btn').forEach((btn, index) => {
            btn.addEventListener('mousedown', (e) => {
                btn.classList.add('storing');
                pressTimers[index] = setTimeout(() => {
                    storeMemory(index);
                    btn.classList.remove('storing');
                }, 1000);
            });
            
            btn.addEventListener('mouseup', () => {
                if (pressTimers[index]) {
                    clearTimeout(pressTimers[index]);
                    btn.classList.remove('storing');
                }
            });
            
            btn.addEventListener('mouseleave', () => {
                if (pressTimers[index]) {
                    clearTimeout(pressTimers[index]);
                    btn.classList.remove('storing');
                }
            });
            
            // Touch events for mobile
            btn.addEventListener('touchstart', (e) => {
                e.preventDefault();
                btn.classList.add('storing');
                pressTimers[index] = setTimeout(() => {
                    storeMemory(index);
                    btn.classList.remove('storing');
                }, 1000);
            });
            
            btn.addEventListener('touchend', (e) => {
                e.preventDefault();
                if (pressTimers[index]) {
                    clearTimeout(pressTimers[index]);
                    btn.classList.remove('storing');
                }
            });
        });
        
        // Initialize WebSocket connection
        connectWebSocket();
    </script>
</body>
</html>
)";

/**
 * @brief Handle root page request
 */
void handleRoot() {
    server.send(200, "text/html", radioHTML);
}

bool initWebServer(const char* ssid, const char* password) {
    wifi_ssid = String(ssid);
    wifi_password = String(password);
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi connection failed!");
        return false;
    }
    
    wifiConnected = true;
    Serial.println();
    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
    
    // Setup web server routes (keep minimal for basic functionality)
    server.on("/", handleRoot);
    
    // Start servers
    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    Serial.println("Web server started on port 80");
    Serial.println("WebSocket server started on port 81");
    
    return true;
}

void handleWebServer() {
    server.handleClient();
    webSocket.loop();  // Handle WebSocket events
}

bool isWiFiConnected() {
    return wifiConnected && (WiFi.status() == WL_CONNECTED);
}

String getLocalIP() {
    if (isWiFiConnected()) {
        return WiFi.localIP().toString();
    }
    return "Not connected";
}

void updateWebClients() {
    // This function is now replaced by broadcastRadioStatus()
    broadcastRadioStatus();
}

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
) {
    cb_seekUp = seekUp;
    cb_seekDown = seekDown;
    cb_volumeUp = volumeUp;
    cb_volumeDown = volumeDown;
    cb_setFreq = setFreq;
    cb_getFreq = getFreq;
    cb_getVol = getVol;
    cb_getRssi = getRssi;
    cb_setMute = setMute;
}

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
) {
    cb_setVolume = setVolume;
    cb_recallMemory = recallMemory;
    cb_storeMemory = storeMemory;
}
