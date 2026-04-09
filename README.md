# LoRa Messenger

A web-based text messenger using ESP32 and RYLR998 LoRa modules. No internet required - communicate directly between devices over long range.

## Hardware

- 2x ESP32 dev boards
- 2x RYLR998 LoRa modules

### Wiring

| ESP32 | RYLR998 |
|-------|---------|
| GPIO 33 | TX |
| GPIO 32 | RX |
| 3.3V | VCC |
| GND | GND |

## Setup

1. Install [PlatformIO](https://platformio.org/)

2. Build and upload to both ESP32s:
   ```bash
   pio run -e messenger -t upload
   ```

3. Configure each device:
   - Connect your phone to the ESP32's WiFi (default: "LoRa-Chat")
   - Open `192.168.4.1` in a browser
   - Go to Settings:
     - **Device 1:** Set Address to `1`, Target Address to `2`
     - **Device 2:** Set Address to `2`, Target Address to `1`, change Device Name to "LoRa-Chat-2"
   - Save & Reboot

4. Connect to either device's WiFi and start chatting

## Features

- Web-based chat interface (works on any phone/device with WiFi and a browser)
- Real-time messaging via WebSocket
- Configurable LoRa parameters:
  - Device address & target address
  - Network ID
  - Frequency band (915 MHz US, 868 MHz EU, 433 MHz)
  - Spreading factor (SF7-SF12)
  - Bandwidth
  - TX power
- All settings persist across reboots
- Shows RSSI (signal strength) for incoming messages

## Project Structure

```
├── src/
│   ├── messenger.cpp    # Main application
│   ├── transmitter.cpp  # Simple range test (legacy)
│   └── receiver.cpp     # Simple range test (legacy)
└── platformio.ini
```

## License

MIT
