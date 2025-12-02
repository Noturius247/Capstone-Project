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
2. Create a `secrets.h` file in the `for actual setup` folder with your AWS credentials:

   ```cpp
   #define AWS_IOT_ENDPOINT "your-endpoint.iot.region.amazonaws.com"
   #define AWS_IOT_CLIENT_ID "your-thing-name"
   #define AWS_CERT_CA "your-root-ca-certificate"
   #define AWS_CERT_CRT "your-device-certificate"
   #define AWS_CERT_PRIVATE "your-private-key"
   ```

3. Update the `upload_port` in [platformio.ini](platformio.ini) to match your COM port
4. Upload the code from `for actual setup/main.cpp` to your ESP32

#### WiFi Configuration Portal UI

The device features an intuitive WiFi configuration portal that automatically launches when no WiFi credentials are saved or connection fails:

**How to Access the Configuration Portal:**

1. **Power on the ESP32** - If no WiFi is configured, it will create a WiFi Access Point
2. **Connect to the AP:**
   - Network Name: `ESP32-AWS-Setup`
   - Password: `12345678`

3. **Open the Configuration Page:**
   - Your browser should automatically open the portal
   - If not, navigate to: `http://192.168.4.1`
   - Alternative: `http://esp32.local`

4. **Configure WiFi:**
   - Click **"Configure WiFi"** button
   - The portal will automatically **scan for available WiFi networks**
   - You'll see a list of networks with signal strength indicators
   - Click on your desired network from the list
   - Enter your WiFi password in the password field
   - Click **"Save"** to connect

**Portal Features:**

- Automatic WiFi network scanning and display
- Signal strength indicators for each network
- Password visibility toggle
- Network quality filtering (removes weak signals)
- Duplicate network removal
- Displays up to 10 strongest networks
- 3-minute timeout (device restarts if not configured)
- Mobile-friendly responsive design

**Note:** Once WiFi credentials are saved, the ESP32 will automatically connect on future boots. To reset WiFi settings, uncomment `wifiManager.resetSettings();` in the code (line 47).

#### Web Dashboard UI

After WiFi configuration, the device hosts a real-time web dashboard for monitoring and control:

**Accessing the Dashboard:**

1. **Find the IP Address** - Check the Serial Monitor after WiFi connection, it displays the IP address
2. **Open in Browser** - Navigate to `http://[ESP32-IP-ADDRESS]` (e.g., `http://192.168.1.100`)
3. **Dashboard loads automatically** - No login required

**Dashboard Features:**

**📊 Real-Time Sensor Monitoring:**

- Live distance readings from ultrasonic sensor (updates every second)
- Current LED status display
- Visual data cards with color-coded information

**🎮 Manual LED Control:**

- **Turn ON** - Manually turn LED on (overrides automatic control)
- **Turn OFF** - Manually turn LED off (overrides automatic control)
- **Auto Mode** - Return to automatic distance-based LED control
- Mode indicator shows current control mode (Manual/Automatic)

**ℹ️ System Information:**

- Device IP address
- Connected WiFi network (SSID)
- WiFi signal strength (RSSI in dBm)
- AWS IoT connection status

**Design:**

- Mobile-responsive design (works on phones, tablets, and desktops)
- Modern gradient UI with smooth animations
- Auto-refreshing data (no manual refresh needed)
- Clean, intuitive interface

**API Endpoints:**

- `GET /` - Main dashboard (HTML)
- `GET /data` - JSON sensor data
- `GET /led?action=on|off|auto` - LED control

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
