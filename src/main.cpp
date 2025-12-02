#include <Arduino.h>
// Include the required libraries
#include <WiFi.h>                // Provides functions to connect the ESP32 to a WiFi network
#include <WiFiClientSecure.h>    // Enables secure (SSL/TLS) communication, required for AWS IoT
#include <PubSubClient.h>        // Handles MQTT protocol (publish-subscribe model)
#include "secrets.h"             // Custom header file that stores WiFi and AWS credentials (keeps them separate from main code)
#include <ArduinoJson.h>         // Creating and parsing JSON Document

#define AWS_IOT_PUBLISH_TOPIC "devices/" AWS_IOT_CLIENT_ID "/data"
#define AWS_IOT_SUBSCRIBE_TOPIC  "devices/" AWS_IOT_CLIENT_ID "/commands"


#define AWS_IOT_SUBSCRIBE

WiFiClientSecure net;
PubSubClient client(net);

// Ultrasonic sensor pins
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

// LED pin
const int LED_PIN = 2;

// Distance threshold in cm
const int DISTANCE_THRESHOLD = 50;

// Variables to store sensor data
float distance = 0.0;


unsigned long lastPublishTime = 0;       // Stores last publish timestamp
const long publishInterval = 2000;       // Interval in milliseconds (2 seconds)
                                          // Minimum interval for DHT22: ~0.5Hz = every 2s

void connectToWiFi() {
    // Print a message to the Serial Monitor to indicate connection attempt
    Serial.print("Connecting to WiFi");

    // Set the ESP32 to Station mode (connects to an existing WiFi network)
    WiFi.mode(WIFI_STA);

    // Begin WiFi connection using SSID and Password defined in secrets.h
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Keep checking WiFi status until the ESP32 is connected to the network
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);          // Wait half a second before checking again
        Serial.print(".");   // Print progress indicator
    }

    // Once connected, print confirmation
    Serial.println(" Connected!");
}

// Configure and connect to AWS IoT Core using certificates
void connectToAWS() {
    // Notify user that the certificate setup is starting
    Serial.println("Configuring certificates...");
  
    // Assign AWS Root Certificate Authority (CA) to the secure WiFi client
    net.setCACert(AWS_CERT_CA);
  
    // Assign the device's own certificate (used to identify the device)
    net.setCertificate(AWS_CERT_CRT);
  
    // Assign the device's private key (used to prove the device owns the certificate)
    net.setPrivateKey(AWS_CERT_PRIVATE);

    // Configure the MQTT client to connect to the AWS IoT Core endpoint on port 8883 (secure MQTT)
    client.setServer(AWS_IOT_ENDPOINT, 8883);

    // Print a message to indicate we are now connecting to AWS IoT
    Serial.print("Connecting to AWS IoT");

    // Attempt to connect to AWS IoT using the device's "Thing Name"
    while (!client.connect(AWS_IOT_CLIENT_ID)) {
        Serial.print(".");  // Show progress
        delay(100);         // Brief pause between connection attempts
    }

    // If we still aren't connected after trying, print an error and return
    if (!client.connected()) {
        Serial.println(" Connection failed (timeout).");
        return;
    }

    // If connected successfully, notify the user
    Serial.println("\nConnected to AWS IoT!");

    // Publish a test message to a specific topic to confirm functionality
    // client.publish(("devices/" AWS_IOT_CLIENT_ID "/data"), "Hello from ESP32!");

    client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);  //Subscribes to incoming topic
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

    // Control LED based on distance threshold
    if (distance <= DISTANCE_THRESHOLD && distance > 0) {
        digitalWrite(LED_PIN, HIGH);  // Turn LED on
        Serial.println("LED: ON (Object detected within 50 cm)");
    } else {
        digitalWrite(LED_PIN, LOW);   // Turn LED off
        Serial.println("LED: OFF");
    }
    Serial.println("---");
}

void publishMessage() {
    JsonDocument doc;
    doc["distance"] = distance;
    doc["led_status"] = (distance <= DISTANCE_THRESHOLD && distance > 0) ? "ON" : "OFF";

    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);
    client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}


void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.print("Incoming message on topic: ");
  Serial.println(topic);

  // Create a JSON document with 200 bytes capacity
  JsonDocument doc;

  // Deserialize the payload into the JSON document
  // deserializeJson is a function from the ArduinoJson library that converts JSON string data into a structured format.
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Parse and handle command for LED control
  if (doc["command"].is<const char*>()) {
    const char* command = doc["command"];
    Serial.print("Command received: ");
    Serial.println(command);

    // Handle LED control commands
    if (strcmp(command, "led_on") == 0) {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("Actuator: LED turned ON via command");
    } else if (strcmp(command, "led_off") == 0) {
      digitalWrite(LED_PIN, LOW);
      Serial.println("Actuator: LED turned OFF via command");
    } else {
      Serial.print("Unknown command: ");
      Serial.println(command);
    }
  }

  // If there's a 'message' key, print its value
  if (doc["message"].is<const char*>()) {
    const char* msg = doc["message"]; // Extract the message string
    Serial.print("Message: ");
    Serial.println(msg);
  }
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

    // Call function to set up certificates and connect to AWS IoT Core
    connectToAWS();

    client.setCallback(messageHandler); // Register the callback function for incoming messages
}

void loop() {
  client.loop();  // Always keep MQTT connection alive

  // Check if it's time to read sensor and publish again
  if (millis() - lastPublishTime >= publishInterval) {

    // Read distance from ultrasonic sensor and control LED
    readSensorData();

    if(client.connected()) {
        publishMessage();
    } else {
        Serial.println("MQTT not connected, skipping publish.");

    }

    // Update lastPublishTime
    lastPublishTime = millis();
  }

  // No blocking delays — loop keeps running fast
}