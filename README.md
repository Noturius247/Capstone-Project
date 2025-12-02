# Capstone Project

## Setup Instructions

This project supports two different configurations:

### Real-World Hardware Setup

For actual hardware deployment and real-world scenarios, use the **main project files** in the root directory. This configuration is designed for physical ESP32 devices and real sensors.

#### Hardware Components Required

- ESP32 Development Board
- HC-SR04 Ultrasonic Sensor
- LED
- 220Ω Resistor (for LED)
- Jumper Wires
- Breadboard

#### Wiring Connections

**Ultrasonic Sensor (HC-SR04):**

- VCC → 5V (ESP32)
- GND → GND (ESP32)
- TRIG → GPIO 5 (ESP32)
- ECHO → GPIO 18 (ESP32)

**LED:**

- Anode (long leg) → GPIO 2 (ESP32) through 220Ω resistor
- Cathode (short leg) → GND (ESP32)

#### Configuration Steps

1. Connect all hardware components according to the wiring diagram above
2. Create a `secrets.h` file in the `for actual setup` folder with your credentials:

   ```cpp
   #define AWS_IOT_ENDPOINT "your-endpoint.iot.region.amazonaws.com"
   #define AWS_IOT_CLIENT_ID "your-thing-name"
   #define AWS_CERT_CA "your-root-ca-certificate"
   #define AWS_CERT_CRT "your-device-certificate"
   #define AWS_CERT_PRIVATE "your-private-key"
   ```

3. Update the `upload_port` in `platformio.ini` to match your COM port
4. Upload the code from `for actual setup/main.cpp` to your ESP32

### Wokwi Simulation Setup

For testing and simulation using Wokwi, use the files in the **`src` folder**. This configuration is optimized for the Wokwi online simulator environment.

#### Wokwi Configuration Files

The project includes pre-configured Wokwi simulation files:

- **diagram.json** - Defines the circuit diagram with components and connections
- **wokwi.toml** - Configuration file that links to the compiled firmware
- **src/main.cpp** - Main code for Wokwi simulation

#### Virtual Components

The Wokwi simulation includes:

- ESP32 DevKit C V4
- HC-SR04 Ultrasonic Sensor
- Red LED
- 220Ω Resistor

#### Virtual Wiring (Configured in diagram.json)

**Ultrasonic Sensor:**

- VCC → ESP32 3.3V
- GND → ESP32 GND
- TRIG → GPIO 5
- ECHO → GPIO 18

**LED:**

- Anode → GPIO 2
- Cathode → 220Ω Resistor → ESP32 GND

#### Running the Wokwi Simulation

1. Create a `secrets.h` file in the `src` folder with your WiFi and AWS credentials
2. Build the project using PlatformIO: `pio run`
3. Open the project in Wokwi online simulator
4. The firmware will be loaded from `.pio/build/esp32dev/firmware.bin`
5. Start the simulation to test the ultrasonic sensor and LED functionality
6. Use the Wokwi interface to adjust the ultrasonic sensor distance and observe LED behavior

## Quick Start

1. **For Real Hardware:**
   - Use PlatformIO with the main project configuration
   - Upload directly to your ESP32 device
   - Ensure all physical connections match your hardware setup

2. **For Wokwi Simulation:**
   - Navigate to the `src` folder
   - Use the Wokwi-specific configuration files
   - Run the simulation in the Wokwi environment

## Development

Make sure to select the appropriate configuration based on your target environment before building and uploading your code.
