# Smart Patient Monitoring with IoT and RFID

A three-node ESP32 prototype for local-network patient monitoring using RFID-based identification, temperature sensing, heart-rate/SpO2 sensing, and a browser dashboard.

## Repository Contents

This repository is intentionally flat for simple GitHub upload:

- `patient_node_1.ino` — ESP32 sensor node for Patient 1
- `patient_node_2.ino` — ESP32 sensor node for Patient 2
- `rfid_receiver_dashboard.ino` — ESP32 RFID receiver and central browser dashboard
- `ARDUINO_LIBRARIES.txt` — required Arduino libraries
- `README.md`
- `LICENSE`
- `.gitignore`

## System Overview

Each patient node uses:

- ESP32
- DS18B20-compatible temperature sensor
- MAX30102 heart-rate / pulse-oximetry sensor
- Wi-Fi

The central node uses:

- ESP32
- MFRC522 RFID reader
- Wi-Fi
- Embedded HTTP dashboard

The receiver maps an RFID card to one of the patient nodes, requests the latest sensor values over the local Wi-Fi network, and presents them in a browser.

## Default Local-Network Layout

| Device | Default IP |
|---|---|
| Patient Node 1 | `192.168.0.10` |
| Patient Node 2 | `192.168.0.20` |
| RFID Receiver / Dashboard | `192.168.0.30` |

Adjust these addresses, gateway values, and Wi-Fi settings to match your network.

## Before Uploading to ESP32

In all three sketches, replace:

```cpp
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";
```

In `rfid_receiver_dashboard.ino`, replace the demonstration RFID UIDs with the UIDs from your own cards:

```cpp
byte uid1[] = {0xDE, 0xAD, 0xBE, 0x01};
byte uid2[] = {0xDE, 0xAD, 0xBE, 0x02};
```

## Arduino Libraries

Install the libraries listed in `ARDUINO_LIBRARIES.txt` using the Arduino IDE Library Manager.

## Typical Wiring

### Patient Nodes

- DS18B20 data: GPIO 15
- MAX30102: ESP32 I2C pins
- Sensor power and ground according to the module specifications

### RFID Receiver

- MFRC522 `RST`: GPIO 22
- MFRC522 `SDA/SS`: GPIO 21
- SPI pins: use the ESP32 SPI interface configured by `SPI.begin()`

Verify voltage levels and the pinout of your exact board before wiring.

## How It Works

1. Each patient ESP32 connects to Wi-Fi and serves sensor readings.
2. The receiver ESP32 reads an RFID card.
3. The scanned UID selects Patient Node 1 or Patient Node 2.
4. The receiver requests sensor data over HTTP.
5. The web dashboard refreshes and displays the selected patient's values.

## Safety and Scope

This repository is an educational prototype, not a certified medical device.

The code uses simple threshold alerts for temperature, heart rate, and oxygen saturation. These alerts are not diagnoses and must not be used for clinical decisions. Sensor readings from hobby-grade modules also require calibration and validation before any real-world medical use.

The original project files contained hard-coded Wi-Fi credentials, RFID identifiers, and patient-identifying demo information. Those values were removed from this public GitHub version.

## Important Implementation Note

The accompanying academic report described features such as cloud storage, encryption, blood-pressure monitoring, and broader security mechanisms. Those features are not implemented in the three source-code files included here, so this repository documents only the functionality supported by the code.

## License

MIT License. See `LICENSE`.
