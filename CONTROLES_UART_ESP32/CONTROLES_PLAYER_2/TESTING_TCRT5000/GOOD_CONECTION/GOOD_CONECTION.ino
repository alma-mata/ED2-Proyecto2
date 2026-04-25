#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

// ========== PINES ANALÓGICOS CORRECTOS ==========
#define TCRT_FRONT_LEFT 13    // GPIO32 - ADC1_CH4 (ANALÓGICO)
#define TCRT_FRONT_RIGHT 12   // GPIO33 - ADC1_CH5 (ANALÓGICO)  
#define TCRT_BACK_LEFT 14     // GPIO34 - ADC1_CH6 (ANALÓGICO) - CAMBIA D25 por 34
#define TCRT_BACK_RIGHT 27    // GPIO35 - ADC1_CH7 (ANALÓGICO) - CAMBIA D26 por 35

// LEDs
#define LED_FRONTAL 2
#define LED_TRASERO 15
#define LED_MODO 4

// Umbrales
int umbralFrontal = 2000;
int umbralTrasero = 2000;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("TCRT-Corregido");
  
  // Configurar LEDs
  pinMode(LED_FRONTAL, OUTPUT);
  pinMode(LED_TRASERO, OUTPUT);
  pinMode(LED_MODO, OUTPUT);
  
  SerialBT.println("SENSORES EN PINES ANALÓGICOS");
  SerialBT.println("32,33,34,35 = Analogicos");
  SerialBT.println("25,26 = Digitales (NO usar para analogRead)");
}

void loop() {
  // Leer sensores ANALÓGICOS (0-4095)
  int fl = analogRead(TCRT_FRONT_LEFT);   // Pin 32
  int fr = analogRead(TCRT_FRONT_RIGHT);  // Pin 33
  int bl = analogRead(TCRT_BACK_LEFT);    // Pin 34
  int br = analogRead(TCRT_BACK_RIGHT);   // Pin 35
  
  // Mostrar valores
  SerialBT.print("FL32:"); SerialBT.print(fl);
  SerialBT.print(" FR33:"); SerialBT.print(fr);
  SerialBT.print(" BL34:"); SerialBT.print(bl);
  SerialBT.print(" BR35:"); SerialBT.println(br);
  
  // Controlar LEDs
  digitalWrite(LED_FRONTAL, (fl > umbralFrontal) || (fr > umbralFrontal));
  digitalWrite(LED_TRASERO, (bl > umbralTrasero) || (br > umbralTrasero));
  
  delay(200);
}

// Calibración
void calibrarSensores() {
  SerialBT.println("Calibrando...");
  
  long sumaFrontal = 0, sumaTrasera = 0;
  
  for (int i = 0; i < 30; i++) {
    sumaFrontal += analogRead(32) + analogRead(33);
    sumaTrasera += analogRead(34) + analogRead(35);
    delay(50);
  }
  
  umbralFrontal = (sumaFrontal / 60) + 300;
  umbralTrasero = (sumaTrasera / 60) + 300;
  
  SerialBT.print("Umbral frontal: "); SerialBT.println(umbralFrontal);
  SerialBT.print("Umbral trasero: "); SerialBT.println(umbralTrasero);
}