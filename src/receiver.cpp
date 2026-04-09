// LoRa Range Test - Receiver
// Blinks LED when it receives BLINK command

#include <Arduino.h>

#define LORA_RX 33 // ESP32 RX <- RYLR998 TX
#define LORA_TX 32 // ESP32 TX -> RYLR998 RX
#define LED_PIN 25 // External LED

HardwareSerial LoRa(2);

unsigned long lastReceived = 0;

void setup()
{
  Serial.begin(115200);
  LoRa.begin(115200, SERIAL_8N1, LORA_RX, LORA_TX);
  pinMode(LED_PIN, OUTPUT);

  delay(1000);

  // Configure RYLR998
  LoRa.println("AT+ADDRESS=2"); // Set this device as address 2
  delay(100);
  LoRa.println("AT+NETWORKID=5"); // Network ID (must match transmitter)
  delay(100);

  Serial.println("Receiver ready - waiting for BLINK commands");
}

void loop()
{
  if (LoRa.available())
  {
    String message = LoRa.readStringUntil('\n');
    Serial.println("Received: " + message);

    // RYLR998 format: +RCV=<address>,<length>,<data>,<RSSI>,<SNR>
    if (message.indexOf("BLINK") >= 0)
    {
      lastReceived = millis();

      // Blink LED 3 times
      for (int i = 0; i < 3; i++)
      {
        digitalWrite(LED_PIN, HIGH);
        delay(150);
        digitalWrite(LED_PIN, LOW);
        delay(150);
      }

      // Parse and display signal strength
      int rssiStart = message.lastIndexOf(",");
      if (rssiStart > 0)
      {
        int snrStart = message.lastIndexOf(",", rssiStart - 1);
        if (snrStart > 0)
        {
          String rssi = message.substring(snrStart + 1, rssiStart);
          String snr = message.substring(rssiStart + 1);
          Serial.println("RSSI: " + rssi + " dBm, SNR: " + snr);
        }
      }
    }
  }

  // Slow blink if no message received in 10 seconds (out of range indicator)
  if (millis() - lastReceived > 10000 && lastReceived > 0)
  {
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    delay(950);
  }
}
