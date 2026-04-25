/*
 * CÓDIGO COMPLETO PARA LEER 4 SENSORES (ADC1 Y ADC2) EN ESP32
 *
 * SOLUCIÓN:
 * Se inicializan los 4 pines de sensores en el setup() con
 * analogSetPinAttenuation() para evitar el conflicto entre ADC1 y ADC2.
 */

// --- Definiciones Sensor 1 ---
const int sensorPin1 = 26;   // <-- ¡AJUSTA ESTE PIN!
const int irLed1 = 14;       // <-- ¡AJUSTA ESTE PIN!
const int detectLed1 = 2;      // <-- ¡AJUSTA ESTE PIN!
int a1, b1, c1;

// --- Definiciones Sensor 2 ---
const int sensorPin2 = 25;   // <-- ¡AJUSTA ESTE PIN!
const int irLed2 = 27;       // <-- ¡AJUSTA ESTE PIN!
const int detectLed2 = 4;      // <-- ¡AJUSTA ESTE PIN!
int a2, b2, c2;

// --- Definiciones Sensor 3 ---
const int sensorPin3 = 33;   // <-- ¡AJUSTA ESTE PIN! (Ejemplo ADC1)
const int irLed3 = 22;       // <-- ¡AJUSTA ESTE PIN!
const int detectLed3 = 16;     // <-- ¡AJUSTA ESTE PIN!
int a3, b3, c3;

// --- Definiciones Sensor 4 ---
const int sensorPin4 = 32;   // <-- ¡AJUSTA ESTE PIN! (Ejemplo ADC2)
const int irLed4 = 23;       // <-- ¡AJUSTA ESTE PIN!
const int detectLed4 = 17;     // <-- ¡AJUSTA ESTE PIN!
int a4, b4, c4;


// --- Umbral de detección ---
int threshold = -75;         // Ajusta este umbral después de probar

void setup() {
  Serial.begin(115200);

  // Configurar pines de salida (LEDs)
  pinMode(irLed1, OUTPUT);
  pinMode(detectLed1, OUTPUT);
  
  pinMode(irLed2, OUTPUT);
  pinMode(detectLed2, OUTPUT);
  
  pinMode(irLed3, OUTPUT);
  pinMode(detectLed3, OUTPUT);
  
  pinMode(irLed4, OUTPUT);
  pinMode(detectLed4, OUTPUT);

  // --- INICIO DE LA SOLUCIÓN (Para 4 Sensores) ---
  // Inicializamos TODOS los pines analógicos, sin importar si
  // son ADC1 o ADC2. Esto los estabiliza.
  
  analogSetPinAttenuation(sensorPin1, ADC_11db);
  analogSetPinAttenuation(sensorPin2, ADC_11db);
  analogSetPinAttenuation(sensorPin3, ADC_11db);
  analogSetPinAttenuation(sensorPin4, ADC_11db);
  
  // --- FIN DE LA SOLUCIÓN ---
}

void loop() {
  
  // --- LECTURA SENSOR 1 ---
  digitalWrite(irLed1, HIGH);
  delayMicroseconds(500);
  a1 = analogRead(sensorPin1);
  digitalWrite(irLed1, LOW);
  delayMicroseconds(500);
  b1 = analogRead(sensorPin1);
  c1 = a1 - b1; // Valor final Sensor 1

  // --- LECTURA SENSOR 2 ---
  digitalWrite(irLed2, HIGH);
  delayMicroseconds(500);
  a2 = analogRead(sensorPin2);
  digitalWrite(irLed2, LOW);
  delayMicroseconds(500);
  b2 = analogRead(sensorPin2);
  c2 = a2 - b2; // Valor final Sensor 2

  // --- LECTURA SENSOR 3 ---
  digitalWrite(irLed3, HIGH);
  delayMicroseconds(500);
  a3 = analogRead(sensorPin3);
  digitalWrite(irLed3, LOW);
  delayMicroseconds(500);
  b3 = analogRead(sensorPin3);
  c3 = a3 - b3; // Valor final Sensor 3

  // --- LECTURA SENSOR 4 ---
  digitalWrite(irLed4, HIGH);
  delayMicroseconds(500);
  a4 = analogRead(sensorPin4);
  digitalWrite(irLed4, LOW);
  delayMicroseconds(500);
  b4 = analogRead(sensorPin4);
  c4 = a4 - b4; // Valor final Sensor 4

  // --- Imprimir todos para depurar ---
  Serial.print("S1: "); Serial.print(c1);
  Serial.print("\t| S2: "); Serial.print(c2);
  Serial.print("\t| S3: "); Serial.print(c3);
  Serial.print("\t| S4: "); Serial.println(c4);

  // --- Lógica de detección (versión corta) ---
  // Esta línea hace lo mismo que tu if/else, pero en una sola línea
  digitalWrite(detectLed1, (c1 < threshold) ? HIGH : LOW);
  digitalWrite(detectLed2, (c2 < threshold) ? HIGH : LOW);
  digitalWrite(detectLed3, (c3 < threshold) ? HIGH : LOW);
  digitalWrite(detectLed4, (c4 < threshold) ? HIGH : LOW);

  delay(50);
}