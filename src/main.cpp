#include <Arduino.h>
// Include the required libraries
#include <WiFi.h>                // Provides functions to connect the ESP32 to a WiFi network
#include <WiFiClientSecure.h>    // Enables secure (SSL/TLS) communication, required for AWS IoT
#include <PubSubClient.h>        // Handles MQTT protocol (publish-subscribe model)
#include "secrets.h"             // Custom header file that stores WiFi and AWS credentials (keeps them separate from main code)
#include <ArduinoJson.h>         // Creating and parsing JSON Document
#include <WiFiManager.h>         // WiFi configuration portal library
#include <Preferences.h>         // For storing WiFi credentials in flash memory
#include <ESPAsyncWebServer.h>   // Async web server for hosting web UI
#include <AsyncTCP.h>            // Required for ESPAsyncWebServer

#define AWS_IOT_PUBLISH_TOPIC "devices/" AWS_IOT_CLIENT_ID "/data"
#define AWS_IOT_SUBSCRIBE_TOPIC  "devices/" AWS_IOT_CLIENT_ID "/commands"


#define AWS_IOT_SUBSCRIBE

WiFiClientSecure net;
PubSubClient client(net);
WiFiManager wifiManager;  // WiFiManager instance for WiFi configuration portal
AsyncWebServer server(80);  // Web server on port 80

// Manual LED override flag
bool manualLEDControl = false;
bool manualLEDState = false;

// Ultrasonic sensor pins
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

// LED pin
const int LED_PIN = 2;

// Distance threshold in cm
const int DISTANCE_THRESHOLD = 50;

// Variables to store sensor data
float distance = 0.0;

// AWS IoT connection status
bool awsConnected = false;
unsigned long lastAWSReconnectAttempt = 0;
const long awsReconnectInterval = 5000;  // Try to reconnect every 5 seconds

unsigned long lastPublishTime = 0;       // Stores last publish timestamp
const long publishInterval = 2000;       // Interval in milliseconds (2 seconds)
                                          // Minimum interval for DHT22: ~0.5Hz = every 2s

// Forward declarations
void publishCloudAcknowledgment(const char* command, const char* status);
void publishMessage();
void messageHandler(char* topic, byte* payload, unsigned int length);
void reconnectAWS();
void connectToAWS();
void setupWebServer();
void readSensorData();
float readUltrasonicDistance();

void connectToWiFi() {
    // Print a message to the Serial Monitor to indicate WiFi setup
    Serial.println("\n=== WiFi Configuration ===");

    // Set the ESP32 to Station mode (connects to an existing WiFi network)
    WiFi.mode(WIFI_STA);

    // Reset WiFi settings for testing (comment out after first use)
    // wifiManager.resetSettings();

    // Configure WiFiManager settings
    wifiManager.setConfigPortalTimeout(180); // 3 minutes timeout for config portal
    wifiManager.setConnectTimeout(30);       // 30 seconds timeout for connecting to WiFi
    wifiManager.setMinimumSignalQuality(20); // Filter networks with weak signal

    // Show WiFi password field as password (dots/asterisks)
    wifiManager.setShowPassword(true);

    // Set custom HTML for better UI experience
    wifiManager.setCustomHeadElement("<style>body{font-family:Arial,sans-serif;}</style>");

    // Enable removing duplicate networks
    wifiManager.setRemoveDuplicateAPs(true);

    // Set custom AP name and password for the configuration portal
    // When ESP32 can't connect to saved WiFi, it creates its own AP with this name
    String apName = "ESP32-AWS-Setup";
    String apPassword = "12345678";  // Minimum 8 characters for WPA2

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

    // Try to connect to saved WiFi credentials
    // If it fails, start config portal with AP name and password
    if (!wifiManager.autoConnect(apName.c_str(), apPassword.c_str())) {
        Serial.println("\n✗ Failed to connect to WiFi and timeout reached.");
        Serial.println("Restarting ESP32 in 3 seconds...");
        delay(3000);
        ESP.restart();  // Restart and try again
    }

    // If we reach here, WiFi is connected
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

// Setup web server with HTML dashboard
void setupWebServer() {
    // Root page - HTML Dashboard
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

        // Update data every 1 second
        setInterval(updateData, 1000);
        updateData(); // Initial call
    </script>
</body>
</html>
)rawliteral";
        request->send(200, "text/html", html);
    });

    // API endpoint for sensor data
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

    // API endpoint for LED control
    server.on("/led", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("action")) {
            String action = request->getParam("action")->value();

            if (action == "on") {
                manualLEDControl = true;
                manualLEDState = true;
                digitalWrite(LED_PIN, HIGH);
                request->send(200, "text/plain", "LED turned ON (Manual Mode)");

                // Sync with cloud
                if (client.connected()) {
                    publishCloudAcknowledgment("WEB_LED_ON", "SUCCESS");
                }
            }
            else if (action == "off") {
                manualLEDControl = true;
                manualLEDState = false;
                digitalWrite(LED_PIN, LOW);
                request->send(200, "text/plain", "LED turned OFF (Manual Mode)");

                // Sync with cloud
                if (client.connected()) {
                    publishCloudAcknowledgment("WEB_LED_OFF", "SUCCESS");
                }
            }
            else if (action == "auto") {
                manualLEDControl = false;
                request->send(200, "text/plain", "LED set to Auto Mode (Distance-based)");

                // Sync with cloud
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

// Configure and connect to AWS IoT Core using certificates
void connectToAWS() {
    // Notify user that the certificate setup is starting
    Serial.println("\n=== AWS IoT Cloud Configuration ===");
    Serial.println("Configuring certificates...");

    // Assign AWS Root Certificate Authority (CA) to the secure WiFi client
    net.setCACert(AWS_CERT_CA);

    // Assign the device's own certificate (used to identify the device)
    net.setCertificate(AWS_CERT_CRT);

    // Assign the device's private key (used to prove the device owns the certificate)
    net.setPrivateKey(AWS_CERT_PRIVATE);

    // Configure the MQTT client to connect to the AWS IoT Core endpoint on port 8883 (secure MQTT)
    client.setServer(AWS_IOT_ENDPOINT, 8883);

    // Set MQTT keepalive and timeout
    client.setKeepAlive(60);

    // Print a message to indicate we are now connecting to AWS IoT
    Serial.print("Connecting to AWS IoT Cloud");

    // Attempt to connect to AWS IoT using the device's "Thing Name"
    int attempts = 0;
    while (!client.connect(AWS_IOT_CLIENT_ID) && attempts < 50) {
        Serial.print(".");  // Show progress
        delay(200);         // Brief pause between connection attempts
        attempts++;
    }

    // If we still aren't connected after trying, print an error and return
    if (!client.connected()) {
        Serial.println("\n❌ AWS IoT connection failed (timeout).");
        Serial.println("⚠️ System will continue with local functionality.");
        Serial.println("🔄 Will retry connection in the background...");
        awsConnected = false;
        return;
    }

    // If connected successfully, notify the user
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

    // Subscribe to incoming command topic from cloud
    if (client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC)) {
        Serial.println("✓ Subscribed to command topic");
    } else {
        Serial.println("❌ Failed to subscribe to command topic");
    }

    // Send initial connection message to cloud
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

// Reconnect to AWS IoT if connection is lost
void reconnectAWS() {
    if (millis() - lastAWSReconnectAttempt > awsReconnectInterval) {
        lastAWSReconnectAttempt = millis();

        Serial.println("🔄 Attempting to reconnect to AWS IoT Cloud...");

        if (client.connect(AWS_IOT_CLIENT_ID)) {
            Serial.println("✓ Reconnected to AWS IoT Cloud!");
            client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
            awsConnected = true;

            // Send reconnection notification
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
    // Clear the trigger pin
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    // Send a 10 microsecond pulse to trigger pin
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Read the echo pin, returns the sound wave travel time in microseconds
    long duration = pulseIn(ECHO_PIN, HIGH);

    // Calculate distance in cm (speed of sound is 343 m/s or 0.0343 cm/µs)
    // Distance = (duration * 0.0343) / 2 (divide by 2 for one-way distance)
    float dist = duration * 0.0343 / 2;

    return dist;
}

void readSensorData() {
    distance = readUltrasonicDistance();
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    // Control LED based on distance threshold (only in auto mode)
    if (!manualLEDControl) {
        if (distance <= DISTANCE_THRESHOLD && distance > 0) {
            digitalWrite(LED_PIN, HIGH);  // Turn LED on
            Serial.println("LED: ON (Object detected within 50 cm)");
        } else {
            digitalWrite(LED_PIN, LOW);   // Turn LED off
            Serial.println("LED: OFF");
        }
    } else {
        Serial.println("LED: " + String(digitalRead(LED_PIN) ? "ON" : "OFF") + " (Manual Mode)");
    }
    Serial.println("---");
}

void publishMessage() {
    JsonDocument doc;

    // Sensor data
    doc["device_id"] = AWS_IOT_CLIENT_ID;
    doc["distance"] = distance;
    doc["led_status"] = digitalRead(LED_PIN) ? "ON" : "OFF";
    doc["manual_mode"] = manualLEDControl;
    doc["threshold"] = DISTANCE_THRESHOLD;

    // System information
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["uptime"] = millis() / 1000; // seconds
    doc["ip_address"] = WiFi.localIP().toString();
    doc["timestamp"] = millis();

    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);

    bool published = client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);

    if (published) {
        Serial.println("📤 Data published to AWS IoT Cloud");
    } else {
        Serial.println("❌ Failed to publish to AWS IoT Cloud");
    }
}


void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.print("☁️ Incoming AWS IoT message on topic: ");
  Serial.println(topic);

  // Create a JSON document with 200 bytes capacity
  JsonDocument doc;

  // Deserialize the payload into the JSON document
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("❌ Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Handle LED control commands from AWS IoT Cloud
  if (doc["command"].is<const char*>()) {
    const char* cmd = doc["command"];
    Serial.print("📡 Cloud Command Received: ");
    Serial.println(cmd);

    if (strcmp(cmd, "LED_ON") == 0) {
      manualLEDControl = true;
      manualLEDState = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("✓ LED turned ON via AWS IoT Cloud");

      // Send acknowledgment back to cloud
      publishCloudAcknowledgment("LED_ON", "SUCCESS");
    }
    else if (strcmp(cmd, "LED_OFF") == 0) {
      manualLEDControl = true;
      manualLEDState = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("✓ LED turned OFF via AWS IoT Cloud");

      // Send acknowledgment back to cloud
      publishCloudAcknowledgment("LED_OFF", "SUCCESS");
    }
    else if (strcmp(cmd, "LED_AUTO") == 0) {
      manualLEDControl = false;
      Serial.println("✓ LED set to AUTO mode via AWS IoT Cloud");

      // Send acknowledgment back to cloud
      publishCloudAcknowledgment("LED_AUTO", "SUCCESS");
    }
    else if (strcmp(cmd, "GET_STATUS") == 0) {
      // Cloud requesting current status
      Serial.println("✓ Status request from AWS IoT Cloud");
      publishMessage();  // Publish current sensor data
    }
    else {
      Serial.println("⚠️ Unknown command from cloud");
      publishCloudAcknowledgment(cmd, "UNKNOWN_COMMAND");
    }
  }

  // Handle general messages (backward compatibility)
  if (doc["message"].is<const char*>()) {
    const char* msg = doc["message"];
    Serial.print("💬 Cloud Message: ");
    Serial.println(msg);
  }

  // Handle threshold updates from cloud
  if (doc["threshold"].is<int>()) {
    int newThreshold = doc["threshold"];
    Serial.print("⚙️ Distance threshold updated from cloud: ");
    Serial.print(newThreshold);
    Serial.println(" cm");
    // Note: DISTANCE_THRESHOLD is const, so we'd need to make it mutable
    // For now, just log it. Can be enhanced to use a variable threshold.
  }
}

// Publish acknowledgment back to AWS IoT Cloud
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
    // Initialize serial communication for debugging at 115200 bits per second
    Serial.begin(115200);

    // Print an initial status message to the Serial Monitor
    Serial.println("Starting ESP32 AWS IoT connection...");

    // Initialize ultrasonic sensor pins
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Initialize LED pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);  // Start with LED off

    // Call function to connect the ESP32 to WiFi
    connectToWiFi();

    // Setup web server for dashboard
    setupWebServer();

    // Call function to set up certificates and connect to AWS IoT Core
    connectToAWS();

    client.setCallback(messageHandler); // Register the callback function for incoming messages
}

void loop() {
  // Check AWS IoT connection and reconnect if needed
  if (!client.connected()) {
    awsConnected = false;
    reconnectAWS();  // Attempt reconnection
  } else {
    awsConnected = true;
    client.loop();  // Keep MQTT connection alive
  }

  // Check if it's time to read sensor and publish again
  if (millis() - lastPublishTime >= publishInterval) {

    // Read distance from ultrasonic sensor and control LED
    readSensorData();

    // Publish to AWS IoT Cloud if connected
    if (client.connected()) {
        publishMessage();
    } else {
        Serial.println("⚠️ AWS IoT not connected, data not published to cloud.");
        Serial.println("💾 Local functionality continues (Sensor + LED + Web UI)");
    }

    // Update lastPublishTime
    lastPublishTime = millis();
  }

  // No blocking delays — loop keeps running fast
}