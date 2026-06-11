# Distributed Air Quality Monitoring System

A distributed IoT air quality monitoring system built on ESP32-S3 sensor nodes communicating via MQTT to a central Flask dashboard.

---

## System Overview

The system consists of multiple independent sensor nodes, each measuring CO2 concentration, temperature, and humidity. Data is transmitted wirelessly via MQTT to a central hub running a Mosquitto broker, Python subscriber, SQLite database, and Flask web dashboard.

---

## Requirements

### Hardware
- ESP32-S3-N16R8 microcontroller (one per node)
- MQ135 gas sensor module
- DHT22 temperature and humidity sensor
- MB102 breadboard power supply module
- 10kΩ and 20kΩ resistors (voltage divider)
- Solderless breadboard and DuPont cables

### Software — Central Hub
- Python 3.x
- Mosquitto MQTT broker
- pip packages: `flask`, `paho-mqtt`

### Firmware — Nodes
- ESP-IDF v6.0.1
- Xtensa toolchain (installed via ESP-IDF installer)

---

## Installation

### 1. Install Mosquitto

**Arch Linux:**
```bash
sudo pacman -S mosquitto
```

**Ubuntu/Debian:**
```bash
sudo apt install mosquitto
```

**macOS:**
```bash
brew install mosquitto
```

### 2. Install Python dependencies

```bash
pip install flask paho-mqtt --break-system-packages
```

### 3. Clone the repository

```bash
git clone <repository-url>
cd air-quality-monitoring
```

---

## Configuration

### WiFi credentials

Edit `components/wifi_manager/secrets.h`:

```c
#define WIFI_SSID     "your_network_name"
#define WIFI_PASSWORD "your_password"
```

For open networks leave the password empty:

```c
#define WIFI_PASSWORD ""
```

### MQTT broker address

Edit `components/mqtt_manager/secrets.h`:

```c
#define MQTT_BROKER_URL "mqtt://laptop.local:1883"
```

Replace `laptop.local` with the hostname or IP address of the central hub machine.

---

## Running the Server

### Start Mosquitto broker

```bash
sudo systemctl start mosquitto
sudo systemctl start avahi-daemon
```

### Start the dashboard

```bash
cd air-quality-monitoring
python server/dashboard/app.py
```

The subscriber starts automatically as a background thread.

Open `http://localhost:5000` in a browser. Other devices on the same network can access the dashboard at `http://<hub-ip>:5000`.

---

## Flashing the Firmware

### Build and flash

```bash
cd air-quality-monitoring
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Replace `/dev/ttyACM0` with the correct port for your system. On macOS this will be `/dev/cu.usbserial-*`.

### First boot

On first boot the node initialises with ID 0 and awaits assignment. Assign an ID from the hub:

```bash
python server/assign_node.py --id 3
```

The node will reboot with the assigned ID and begin publishing.

---

## Node Management

### Assign node ID

```bash
python server/assign_node.py --id <node_id>
```

### Update RZERO calibration

```bash
python server/config_update.py --node <node_id> --rzero <value>
```

### Update read interval

```bash
python server/config_update.py --node <node_id> --interval <milliseconds>
```

Minimum interval is 2000ms. Maximum is 300000ms (5 minutes).

### OTA firmware update

```bash
python server/ota_update.py --node <node_id> --url http://<server>/<firmware>.bin
```

---

## Sensor Calibration

Each MQ135 unit requires individual calibration. Calibration should be performed outdoors under clean air conditions (20-25°C, 40-60% humidity) after a minimum 30 minute warmup.

### Calibration procedure

1. Power the node and wait 30 minutes for thermal stabilisation
2. Collect RS values from the MQTT calibration topic
3. Average a minimum of 10 RS readings
4. Calculate RZERO using:

```
RZERO = RS_avg × (424.0 / 116.6020682)^(1.0 / 2.769034857)
```

5. Apply the calibrated value:

```bash
python server/config_update.py --node <node_id> --rzero <calculated_value>
```

### Calibrated RZERO values (this deployment)

| Node | RZERO |
|------|-------|
| 3    | 65.43 |
| 4    | 62.98 |
| 5    | 87.35 |

---

## Project Structure

```
air-quality-monitoring/
├── components/
│   ├── config_manager/     NVS configuration storage
│   ├── dht22/              DHT22 sensor driver
│   ├── mqtt_manager/       MQTT client and event handler
│   ├── mq135/              MQ135 ADC driver and PPM formula
│   ├── ota_manager/        OTA firmware update handler
│   └── wifi_manager/       WiFi station mode manager
├── main/
│   └── main.c              Application entry point
├── server/
│   ├── dashboard/
│   │   ├── app.py          Flask web application
│   │   └── templates/      HTML templates
│   ├── database/
│   │   └── airquality.db   SQLite database
│   ├── assign_node.py      Node ID assignment script
│   ├── config_update.py    Remote configuration script
│   ├── mosquitto.conf      Portable Mosquitto configuration
│   ├── ota_update.py       OTA update trigger script
│   └── subscriber.py       MQTT subscriber and database writer
├── partitions.csv          Custom partition table
└── README.md
```

---

## MQTT Topic Structure

| Topic | Direction | Description |
|-------|-----------|-------------|
| `airquality/nodeX/sensors` | Node → Hub | Sensor readings JSON |
| `airquality/nodeX/config` | Hub → Node | Configuration updates |
| `airquality/nodeX/ota` | Hub → Node | OTA firmware URL |
| `airquality/nodeX/status` | Node → Hub | Acknowledgement messages |
| `airquality/unassigned/config` | Hub → Node | Node ID assignment |
| `airquality/unassigned/status` | Node → Hub | Assignment confirmation |

---

## Sensor Data Format

```json
{
  "node": 3,
  "timestamp": 1780776623,
  "ppm": 463.10,
  "temperature": 28.40,
  "humidity": 52.70
}
```

---

## Known Limitations

- The MQ135 is a consumer grade sensor with ±10-15% accuracy. Readings represent indicative air quality trends rather than precise measurements.
- MQTT broker and dashboard operate without authentication or encryption. Suitable for trusted local networks only.
- All nodes and the central hub must be on the same local WiFi network.
- Calibration must be redone if the power supply changes.
- RZERO changes require a node restart to take effect.

---

