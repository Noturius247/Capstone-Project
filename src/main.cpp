#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.h"
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <time.h>

#define AWS_IOT_PUBLISH_TOPIC "devices/" AWS_IOT_CLIENT_ID "/data"
#define AWS_IOT_SUBSCRIBE_TOPIC  "devices/" AWS_IOT_CLIENT_ID "/commands"

WiFiClientSecure net;
PubSubClient client(net);
WiFiManager wifiManager;
AsyncWebServer server(80);

bool manualLEDControl = false;

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int LED_PIN = 2;
const int DISTANCE_THRESHOLD = 50;

float distance = 0.0;

bool awsConnected = false;
unsigned long lastAWSReconnectAttempt = 0;
const long awsReconnectInterval = 5000;

unsigned long lastPublishTime = 0;
const long publishInterval = 2000;

void publishCloudAcknowledgment(const char* command, const char* status);
void publishMessage();
void messageHandler(char* topic, byte* payload, unsigned int length);
void reconnectAWS();
void connectToAWS();
void setupWebServer();
void readSensorData();
float readUltrasonicDistance();

void connectToWiFi() {
    Serial.println("\n=== WiFi Configuration ===");
    WiFi.mode(WIFI_STA);

    wifiManager.setConfigPortalTimeout(180);
    wifiManager.setConnectTimeout(30);
    wifiManager.setMinimumSignalQuality(20);
    wifiManager.setShowPassword(true);
    wifiManager.setCustomHeadElement("<style>body{font-family:Arial,sans-serif;}</style>");
    wifiManager.setRemoveDuplicateAPs(true);

    String apName = "ESP32-AWS-Setup";
    String apPassword = "12345678";

    Serial.println("\n╔════════════════════════════════════════════════╗");
    Serial.println("║     WiFi Configuration Portal Instructions     ║");
    Serial.println("╚════════════════════════════════════════════════╝");
    Serial.println("\nAttempting to connect to saved WiFi...");
    Serial.println("\nIf no WiFi configured or connection fails:");
    Serial.println("┌────────────────────────────────────────────────┐");
    Serial.println("│ STEP 1: Connect to Configuration Portal       │");
    Serial.println("│   • WiFi Network: " + apName + "               │");
    Serial.println("│   • Password: " + apPassword + "                      │");
    Serial.println("├────────────────────────────────────────────────┤");
    Serial.println("│ STEP 2: Open Configuration Page               │");
    Serial.println("│   • Open browser and go to: http://192.168.4.1│");
    Serial.println("│   • Or use: http://esp32.local                │");
    Serial.println("├────────────────────────────────────────────────┤");
    Serial.println("│ STEP 3: Configure WiFi                        │");
    Serial.println("│   • Click 'Configure WiFi'                     │");
    Serial.println("│   • The page will scan and show available WiFi│");
    Serial.println("│   • Select your WiFi network from the list    │");
    Serial.println("│   • Enter your WiFi password                   │");
    Serial.println("│   • Click 'Save'                               │");
    Serial.println("└────────────────────────────────────────────────┘");
    Serial.println("\nWaiting for configuration...\n");

    if (!wifiManager.autoConnect(apName.c_str(), apPassword.c_str())) {
        Serial.println("\n✗ Failed to connect to WiFi and timeout reached.");
        Serial.println("Restarting ESP32 in 3 seconds...");
        delay(3000);
        ESP.restart();
    }

    Serial.println("\n╔════════════════════════════════════════════════╗");
    Serial.println("║          ✓ WiFi Connected Successfully!        ║");
    Serial.println("╚════════════════════════════════════════════════╝");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("Signal Strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println("════════════════════════════════════════════════\n");
}

void setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 IoT Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
        }
        .header {
            background: white;
            padding: 25px;
            border-radius: 15px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            margin-bottom: 20px;
            text-align: center;
        }
        .header h1 {
            color: #667eea;
            margin-bottom: 5px;
        }
        .header p {
            color: #666;
            font-size: 14px;
        }
        .card {
            background: white;
            padding: 25px;
            border-radius: 15px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            margin-bottom: 20px;
        }
        .card h2 {
            color: #333;
            margin-bottom: 20px;
            font-size: 20px;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }
        .sensor-data {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }
        .data-item {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 20px;
            border-radius: 10px;
            color: white;
            text-align: center;
        }
        .data-value {
            font-size: 36px;
            font-weight: bold;
            margin: 10px 0;
        }
        .data-label {
            font-size: 14px;
            opacity: 0.9;
        }
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
        }
        .status-on { background: #4CAF50; }
        .status-off { background: #f44336; }
        .controls {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
        }
        .btn {
            flex: 1;
            min-width: 120px;
            padding: 15px 25px;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.3s;
        }
        .btn-on {
            background: #4CAF50;
            color: white;
        }
        .btn-on:hover {
            background: #45a049;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(76,175,80,0.4);
        }
        .btn-off {
            background: #f44336;
            color: white;
        }
        .btn-off:hover {
            background: #da190b;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(244,67,54,0.4);
        }
        .btn-auto {
            background: #2196F3;
            color: white;
        }
        .btn-auto:hover {
            background: #0b7dda;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(33,150,243,0.4);
        }
        .info-grid {
            display: grid;
            gap: 10px;
        }
        .info-row {
            display: flex;
            justify-content: space-between;
            padding: 10px;
            background: #f5f5f5;
            border-radius: 5px;
        }
        .info-label {
            font-weight: bold;
            color: #666;
        }
        .info-value {
            color: #333;
        }
        @media (max-width: 600px) {
            .data-value { font-size: 28px; }
            .btn { min-width: 100%; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🌐 ESP32 IoT Dashboard</h1>
            <p>Real-time Monitoring & Control</p>
        </div>

        <div class="card">
            <h2>📊 Sensor Data</h2>
            <div class="sensor-data">
                <div class="data-item">
                    <div class="data-label">Distance</div>
                    <div class="data-value" id="distance">--</div>
                    <div class="data-label">centimeters</div>
                </div>
                <div class="data-item">
                    <div class="data-label">LED Status</div>
                    <div class="data-value" id="ledStatus">--</div>
                    <div class="data-label">current state</div>
                </div>
            </div>
        </div>

        <div class="card">
            <h2>🎮 Manual LED Control</h2>
            <div class="controls">
                <button class="btn btn-on" onclick="controlLED('on')">Turn ON</button>
                <button class="btn btn-off" onclick="controlLED('off')">Turn OFF</button>
                <button class="btn btn-auto" onclick="controlLED('auto')">Auto Mode</button>
            </div>
            <p style="margin-top: 15px; color: #666; font-size: 14px;">
                <span class="status-indicator" id="modeIndicator"></span>
                <span id="modeText">Mode: Loading...</span>
            </p>
        </div>

        <div class="card">
            <h2>ℹ️ System Information</h2>
            <div class="info-grid">
                <div class="info-row">
                    <span class="info-label">IP Address:</span>
                    <span class="info-value" id="ipAddress">--</span>
                </div>
                <div class="info-row">
                    <span class="info-label">WiFi SSID:</span>
                    <span class="info-value" id="ssid">--</span>
                </div>
                <div class="info-row">
                    <span class="info-label">Signal Strength:</span>
                    <span class="info-value" id="rssi">--</span>
                </div>
                <div class="info-row">
                    <span class="info-label">AWS IoT:</span>
                    <span class="info-value" id="awsStatus">--</span>
                </div>
            </div>
        </div>
    </div>

    <script>
        function updateData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('distance').textContent = data.distance.toFixed(1);
                    document.getElementById('ledStatus').textContent = data.led_status;
                    document.getElementById('ipAddress').textContent = data.ip;
                    document.getElementById('ssid').textContent = data.ssid;
                    document.getElementById('rssi').textContent = data.rssi + ' dBm';
                    document.getElementById('awsStatus').textContent = data.aws_connected ? 'Connected' : 'Disconnected';

                    const modeIndicator = document.getElementById('modeIndicator');
                    const modeText = document.getElementById('modeText');
                    if (data.manual_mode) {
                        modeIndicator.className = 'status-indicator status-on';
                        modeText.textContent = 'Mode: Manual Control';
                    } else {
                        modeIndicator.className = 'status-indicator status-off';
                        modeText.textContent = 'Mode: Automatic (Distance-based)';
                    }
                })
                .catch(error => console.error('Error:', error));
        }

        function controlLED(action) {
            fetch('/led?action=' + action)
                .then(response => response.text())
                .then(data => {
                    console.log(data);
                    updateData();
                })
                .catch(error => console.error('Error:', error));
        }

        setInterval(updateData, 1000);
        updateData();
    </script>
</body>
</html>
)rawliteral";
        request->send(200, "text/html", html);
    });

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{";
        json += "\"distance\":" + String(distance) + ",";
        json += "\"led_status\":\"" + String(digitalRead(LED_PIN) ? "ON" : "OFF") + "\",";
        json += "\"manual_mode\":" + String(manualLEDControl ? "true" : "false") + ",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"ssid\":\"" + WiFi.SSID() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"aws_connected\":" + String(client.connected() ? "true" : "false");
        json += "}";
        request->send(200, "application/json", json);
    });

    server.on("/led", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("action")) {
            String action = request->getParam("action")->value();

            if (action == "on") {
                manualLEDControl = true;
                digitalWrite(LED_PIN, HIGH);
                request->send(200, "text/plain", "LED turned ON (Manual Mode)");

                if (client.connected()) {
                    publishCloudAcknowledgment("WEB_LED_ON", "SUCCESS");
                }
            }
            else if (action == "off") {
                manualLEDControl = true;
                digitalWrite(LED_PIN, LOW);
                request->send(200, "text/plain", "LED turned OFF (Manual Mode)");

                if (client.connected()) {
                    publishCloudAcknowledgment("WEB_LED_OFF", "SUCCESS");
                }
            }
            else if (action == "auto") {
                manualLEDControl = false;
                request->send(200, "text/plain", "LED set to Auto Mode (Distance-based)");

                if (client.connected()) {
                    publishCloudAcknowledgment("WEB_LED_AUTO", "SUCCESS");
                }
            }
            else {
                request->send(400, "text/plain", "Invalid action");
            }
        } else {
            request->send(400, "text/plain", "Missing action parameter");
        }
    });

    server.begin();
    Serial.println("✓ Web Server Started!");
    Serial.println("Access dashboard at: http://" + WiFi.localIP().toString());
}

void connectToAWS() {
    Serial.println("\n=== AWS IoT Cloud Configuration ===");
    Serial.println("Synchronizing time with NTP server...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    int retries = 0;
    while (now < 8 * 3600 * 2 && retries < 20) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        retries++;
    }

    if (now < 8 * 3600 * 2) {
        Serial.println("\n❌ Failed to get time from NTP server!");
        Serial.println("⚠️ SSL/TLS may fail without accurate time.");
    } else {
        Serial.println("\n✓ Time synchronized successfully!");
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        Serial.print("Current time: ");
        Serial.println(asctime(&timeinfo));
    }

    Serial.println("Configuring certificates...");
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);
    client.setServer(AWS_IOT_ENDPOINT, 8883);
    client.setKeepAlive(60);

    Serial.print("Connecting to AWS IoT Cloud");
    Serial.println();
    Serial.print("Endpoint: ");
    Serial.println(AWS_IOT_ENDPOINT);
    Serial.print("Client ID: ");
    Serial.println(AWS_IOT_CLIENT_ID);
    Serial.print("Port: 8883");
    Serial.println();

    int attempts = 0;
    while (!client.connect(AWS_IOT_CLIENT_ID) && attempts < 50) {
        Serial.print(".");
        delay(200);
        attempts++;

        if (attempts % 10 == 0) {
            int state = client.state();
            Serial.println();
            Serial.print("MQTT State: ");
            Serial.print(state);
            Serial.print(" - ");
            switch (state) {
                case -4: Serial.println("MQTT_CONNECTION_TIMEOUT"); break;
                case -3: Serial.println("MQTT_CONNECTION_LOST"); break;
                case -2: Serial.println("MQTT_CONNECT_FAILED"); break;
                case -1: Serial.println("MQTT_DISCONNECTED"); break;
                case 0: Serial.println("MQTT_CONNECTED"); break;
                case 1: Serial.println("MQTT_CONNECT_BAD_PROTOCOL"); break;
                case 2: Serial.println("MQTT_CONNECT_BAD_CLIENT_ID"); break;
                case 3: Serial.println("MQTT_CONNECT_UNAVAILABLE"); break;
                case 4: Serial.println("MQTT_CONNECT_BAD_CREDENTIALS"); break;
                case 5: Serial.println("MQTT_CONNECT_UNAUTHORIZED"); break;
                default: Serial.println("UNKNOWN_ERROR"); break;
            }
            Serial.print("Continuing... ");
        }
    }

    if (!client.connected()) {
        Serial.println("\n❌ AWS IoT connection failed (timeout).");
        int state = client.state();
        Serial.print("Final MQTT State Code: ");
        Serial.println(state);
        Serial.println("\n⚠️ Possible causes:");
        Serial.println("  1. Incorrect AWS IoT endpoint");
        Serial.println("  2. Certificate/key format issues");
        Serial.println("  3. Network blocking port 8883");
        Serial.println("  4. Policy not attached to certificate in AWS");
        Serial.println("  5. Certificate not activated in AWS IoT Core");
        Serial.println("  6. Time synchronization failed");
        Serial.println("\n⚠️ System will continue with local functionality.");
        Serial.println("🔄 Will retry connection in the background...");
        awsConnected = false;
        return;
    }

    Serial.println("\n╔════════════════════════════════════════════════╗");
    Serial.println("║     ✓ Connected to AWS IoT Cloud!             ║");
    Serial.println("╚════════════════════════════════════════════════╝");
    Serial.print("Endpoint: ");
    Serial.println(AWS_IOT_ENDPOINT);
    Serial.print("Client ID: ");
    Serial.println(AWS_IOT_CLIENT_ID);
    Serial.print("Publish Topic: ");
    Serial.println(AWS_IOT_PUBLISH_TOPIC);
    Serial.print("Subscribe Topic: ");
    Serial.println(AWS_IOT_SUBSCRIBE_TOPIC);
    Serial.println("════════════════════════════════════════════════\n");

    if (client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC)) {
        Serial.println("✓ Subscribed to command topic");
    } else {
        Serial.println("❌ Failed to subscribe to command topic");
    }

    JsonDocument doc;
    doc["device_id"] = AWS_IOT_CLIENT_ID;
    doc["status"] = "CONNECTED";
    doc["message"] = "Device connected to AWS IoT Cloud";
    doc["ip_address"] = WiFi.localIP().toString();

    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);
    client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);

    awsConnected = true;
}

void reconnectAWS() {
    if (millis() - lastAWSReconnectAttempt > awsReconnectInterval) {
        lastAWSReconnectAttempt = millis();
        Serial.println("🔄 Attempting to reconnect to AWS IoT Cloud...");

        if (client.connect(AWS_IOT_CLIENT_ID)) {
            Serial.println("✓ Reconnected to AWS IoT Cloud!");
            client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
            awsConnected = true;

            JsonDocument doc;
            doc["device_id"] = AWS_IOT_CLIENT_ID;
            doc["status"] = "RECONNECTED";
            doc["message"] = "Device reconnected to AWS IoT Cloud";

            char jsonBuffer[256];
            serializeJson(doc, jsonBuffer);
            client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
        } else {
            Serial.println("❌ AWS IoT reconnection failed. Will retry...");
            awsConnected = false;
        }
    }
}

float readUltrasonicDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration == 0) {
        return -1;
    }
    float dist = duration * 0.0343 / 2;
    return dist;
}

void readSensorData() {
    distance = readUltrasonicDistance();
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    if (!manualLEDControl) {
        if (distance <= DISTANCE_THRESHOLD && distance > 0) {
            digitalWrite(LED_PIN, HIGH);
            Serial.println("LED: ON (Object detected within 50 cm)");
        } else {
            digitalWrite(LED_PIN, LOW);
            Serial.println("LED: OFF");
        }
    } else {
        Serial.println("LED: " + String(digitalRead(LED_PIN) ? "ON" : "OFF") + " (Manual Mode)");
    }
    Serial.println("---");
}

void publishMessage() {
    JsonDocument doc;
    doc["device_id"] = AWS_IOT_CLIENT_ID;
    doc["distance"] = distance;
    doc["led_status"] = digitalRead(LED_PIN) ? "ON" : "OFF";
    doc["manual_mode"] = manualLEDControl;
    doc["threshold"] = DISTANCE_THRESHOLD;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["uptime"] = millis() / 1000;
    doc["ip_address"] = WiFi.localIP().toString();
    doc["timestamp"] = millis();

    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);
    bool published = client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);

    if (!published) {
        Serial.println("❌ Publish failed");
    }
}

void messageHandler(char* topic, byte* payload, unsigned int length) {
    Serial.print("☁️ Incoming AWS IoT message on topic: ");
    Serial.println(topic);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        Serial.print("❌ Failed to parse JSON: ");
        Serial.println(error.c_str());
        return;
    }

    if (doc["command"].is<const char*>()) {
        const char* cmd = doc["command"];
        Serial.print("📡 Cloud Command Received: ");
        Serial.println(cmd);

        if (strcmp(cmd, "LED_ON") == 0) {
            manualLEDControl = true;
            digitalWrite(LED_PIN, HIGH);
            Serial.println("✓ LED turned ON via AWS IoT Cloud");
            publishCloudAcknowledgment("LED_ON", "SUCCESS");
        }
        else if (strcmp(cmd, "LED_OFF") == 0) {
            manualLEDControl = true;
            digitalWrite(LED_PIN, LOW);
            Serial.println("✓ LED turned OFF via AWS IoT Cloud");
            publishCloudAcknowledgment("LED_OFF", "SUCCESS");
        }
        else if (strcmp(cmd, "LED_AUTO") == 0) {
            manualLEDControl = false;
            Serial.println("✓ LED set to AUTO mode via AWS IoT Cloud");
            publishCloudAcknowledgment("LED_AUTO", "SUCCESS");
        }
        else if (strcmp(cmd, "GET_STATUS") == 0) {
            Serial.println("✓ Status request from AWS IoT Cloud");
            publishMessage();
        }
        else {
            Serial.println("⚠️ Unknown command from cloud");
            publishCloudAcknowledgment(cmd, "UNKNOWN_COMMAND");
        }
    }

    if (doc["message"].is<const char*>()) {
        const char* msg = doc["message"];
        Serial.print("💬 Cloud Message: ");
        Serial.println(msg);
    }

    if (doc["threshold"].is<int>()) {
        int newThreshold = doc["threshold"];
        Serial.print("⚙️ Distance threshold updated from cloud: ");
        Serial.print(newThreshold);
        Serial.println(" cm");
    }
}

void publishCloudAcknowledgment(const char* command, const char* status) {
    if (!client.connected()) return;

    JsonDocument doc;
    doc["device_id"] = AWS_IOT_CLIENT_ID;
    doc["command"] = command;
    doc["status"] = status;
    doc["timestamp"] = millis();

    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);

    String ackTopic = "devices/" + String(AWS_IOT_CLIENT_ID) + "/ack";
    client.publish(ackTopic.c_str(), jsonBuffer);

    Serial.println("📤 Acknowledgment sent to cloud");
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting ESP32 AWS IoT connection...");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    connectToWiFi();
    setupWebServer();
    connectToAWS();

    client.setCallback(messageHandler);
}

void loop() {
    if (!client.connected()) {
        awsConnected = false;
        reconnectAWS();
    } else {
        awsConnected = true;
        client.loop();
    }

    if (millis() - lastPublishTime >= publishInterval) {
        readSensorData();

        if (client.connected()) {
            Serial.print("☁️ AWS IoT Status: CONNECTED | ");
            publishMessage();
            Serial.println("✅ Published successfully");
        } else {
            Serial.println("⚠️ AWS IoT Status: DISCONNECTED");
            Serial.println("   Data not published to cloud.");
            Serial.println("💾 Local functionality continues (Sensor + LED + Web UI)");
        }

        lastPublishTime = millis();
    }
}
