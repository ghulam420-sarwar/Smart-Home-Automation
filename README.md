# Smart Home Automation System

An ESP32-based home automation controller that reads temperature, humidity, light level, and motion, runs local automation rules, and publishes telemetry to an MQTT broker. Relay outputs control real AC loads. Compatible with **Home Assistant** and **Node-RED**.

![Circuit diagram](https://github.com/ghulam420-sarwar/Smart-Home-Automation/blob/main/circuit_diagram.png)

## Features

| Feature | Detail |
|---|---|
| Auto fan | ON when temp > 28 °C, OFF below 26 °C |
| Auto light | ON on motion if dark, OFF after 2 min of no motion |
| Remote relay control | Publish `ON`/`OFF` to `ghulam/home/relay/<1-4>/set` |
| Telemetry | JSON to `ghulam/home/telemetry` every 5 s |
| Home Assistant | Auto-discovered via MQTT discovery (configurable) |

## Hardware

| Component         | Role                          | Pin       |
|-------------------|-------------------------------|-----------|
| ESP32 DevKit v1   | MCU + Wi-Fi                   | —         |
| DHT22             | Temperature + Humidity        | GPIO4     |
| LDR + 10 kΩ div.  | Light level (ADC)             | GPIO34    |
| PIR HC-SR501      | Motion detection              | GPIO35    |
| 4-ch relay board  | Switch AC loads (active LOW)  | GPIO26/27/14/12 |

> **Safety**: Always use properly rated relay modules with isolation between the ESP32 side and the AC side. Never touch AC wiring while powered.

## Build & Flash

```bash
# Edit WIFI_SSID and WIFI_PASS in src/main.cpp first
pio run -t upload
pio device monitor
```

## MQTT Topics

| Topic | Direction | Payload |
|---|---|---|
| `ghulam/home/telemetry` | Published | JSON telemetry |
| `ghulam/home/relay/1/set` | Subscribed | `ON` or `OFF` |
| `ghulam/home/relay/2/set` | Subscribed | `ON` or `OFF` |
| `ghulam/home/relay/3/set` | Subscribed | `ON` or `OFF` |
| `ghulam/home/relay/4/set` | Subscribed | `ON` or `OFF` |

### Example telemetry payload
```json
{
  "temp": 27.4,
  "hum": 55.2,
  "lux": 134,
  "motion": true,
  "r1": true,
  "r2": false,
  "r3": false,
  "r4": false
}
```

## Node-RED Dashboard

Import the flow from `docs/nodered_flow.json` to get a live dashboard with gauges for temperature, humidity, and relay toggle buttons.

## What I Learned

- MQTT retained messages to restore relay state after reboot
- Hysteresis in the fan control loop to avoid rapid on/off cycling
- Active-LOW relay board wiring — a common beginner mistake
- ADC non-linearity on the ESP32 GPIO34 pin and how to compensate

## License

MIT © Ghulam Sarwar
