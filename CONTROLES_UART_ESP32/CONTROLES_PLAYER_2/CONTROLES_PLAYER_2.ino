/*
 * Proyecto: ESP32 Bluetooth a STM32 Bridge
 * Autor_1: Anthony Boteo 
 * Autor_2: Alma Mata
 * Nombre del Dispositivo: ESP_P2
 */

#include "BluetoothSerial.h"

// Configuración bluetooth
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` and enable it
#endif

BluetoothSerial SerialBT;

// Pines de configuración
#define RXD2 16
#define TXD2 17

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  // Nombre del control y jugador a controlar
  SerialBT.begin("ESP_P2"); 
  
}

void loop() {
  if (SerialBT.available()) {
    char data = SerialBT.read();
    char conv_minus = tolower(data)
    Serial2.print(data);       
  }

  if (Serial2.available()) {
    char dataFromSTM = Serial2.read();
  }
  
  delay(10);
}