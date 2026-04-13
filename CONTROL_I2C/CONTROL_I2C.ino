/****************************************/
// Datos
/****************************************/
// Encabezado
#include "Wire.h"     // Librería para manejo de I2C

/****************************************/
// Definiciones y Variables
#define SLAVE_ADDR 0x10
#define SDA_PIN 21
#define SCL_PIN 22

// Variables globales
uint8_t value = 0;

/****************************************/
// Configuración
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Serial listo");

  Wire.begin(SLAVE_ADDR, SDA_PIN, SCL_PIN, 100000); 
  Wire.onRequest(requestEvent);  // Función que responde cuando se hace un llamado

  Serial.println("ESP32 listo como Slave I2C");

}

/****************************************/
// Función principal
void loop() {
  // put your main code here, to run repeatedly:
  delay(100);
}

/****************************************/
// Subrutinas sin Interrupcion
void requestEvent() { // Cuando el STM32 pide datos
  Wire.write(value);  // enviar 1 byte
  value++;            // cambiar valor para probar
}

/****************************************/
// Subrutinas de Interrupcion