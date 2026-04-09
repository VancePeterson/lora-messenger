// LoRa Range Test - Transmitter
// Sends BLINK command every 2 seconds

#include <Arduino.h>

#define LORA_RX 33  // ESP32 RX <- RYLR998 TX
#define LORA_TX 32  // ESP32 TX -> RYLR998 RX
#define LED_PIN 2   // Onboard LED

HardwareSerial LoRa(2);

void setup() {
  Serial.begin(115200);
  LoRa.begin(115200, SERIAL_8N1, LORA_RX, LORA_TX);
  pinMode(LED_PIN, OUTPUT);

  delay(1000);

  // Configure RYLR998
  LoRa.println("AT+ADDRESS=1");  // Set this device as address 1
  delay(100);
  LoRa.println("AT+NETWORKID=5");  // Network ID (must match receiver)
  delay(100);

  Serial.println("Transmitter ready - sending BLINK every 2 seconds");
}

void loop() {
  // Send BLINK command to address 2
  // Format: AT+SEND=<address>,<length>,<data>
  LoRa.println("AT+SEND=2,5,BLINK");

  // Flash LED to show we sent
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Sent: BLINK");

  // Check for any responses
  while (LoRa.available()) {
    String response = LoRa.readStringUntil('\n');
    Serial.println("Response: " + response);
  }

  delay(2000);
}
