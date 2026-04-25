/*
 * CÓDIGO DE CALIBRACIÓN (ANTI-RUIDO)
 * Lee el sensor 20 veces muy rápido y saca el promedio
 * para obtener una lectura estable.
 */

// --- Pines ---
const int sensorSharpPin = 12; // Pin D12
const int ledPin = 2;          // Pin D2

// --- Calibración ---
int threshold = 1700; // Ajusta esto DESPUÉS de calibrar
const int NUM_LECTURAS = 20; // Número de muestras para promediar

void setup() {
  Serial.begin(115200);
  Serial.println("--- Calibrando Sensor Sharp (con promedio) ---");
  pinMode(ledPin, OUTPUT);
  analogSetPinAttenuation(sensorSharpPin, ADC_11db); 
}

/*
 * Esta función lee el sensor varias veces y devuelve el promedio
 * para eliminar el ruido.
 */
int leerPromedio() {
  long total = 0; // Usamos 'long' para evitar desbordamiento

  for (int i = 0; i < NUM_LECTURAS; i++) {
    total = total + analogRead(sensorSharpPin);
    delayMicroseconds(50); // Pequeña pausa entre lecturas
  }

  // Devuelve el promedio
  return total / NUM_LECTURAS;
}


void loop() {
  
  // --- Leer el valor PROMEDIADO ---
  int valorSensor = leerPromedio(); 

  // --- Imprimir el valor ---
  Serial.print("Valor (Estable): ");
  Serial.print(valorSensor);

  // --- Lógica del LED ---
  if (valorSensor > threshold) {
    digitalWrite(ledPin, HIGH);
    Serial.println(" <-- ¡OBJETO DETECTADO!");
  } else {
    digitalWrite(ledPin, LOW);
    Serial.println();
  }

  delay(100); // Espera normal del loop
}