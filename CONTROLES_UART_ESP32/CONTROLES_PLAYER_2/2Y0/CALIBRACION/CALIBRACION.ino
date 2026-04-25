/* Calibración y filtrado para GP2Y (Sharp) en ESP32
   - PIN_SENSOR: ADC (ej. 34)
   - PIN_CTRL: opcional para encender/apagar sensor (low-side switch)
   - Se hace calibración automática al arrancar midiendo "fondo"
   - Usa mediana + promedio para lecturas robustas
   - Usa histeresis y debounce por conteo para evitar parpadeo
*/

#include <BluetoothSerial.h>
BluetoothSerial SerialBT;

// Pines
const int PIN_SENSOR = 34;   // ADC
const int PIN_CTRL   = 35;   // controla transistor (HIGH = sensor ON) - opcional
const int LED_DET    = 15;

// Parámetros de filtrado/calibración
const int N_SAMPLES = 9;            // tomar N muestras (mejor si N es impar para mediana)
const int TIEMPO_ESTABILIZACION_MS = 60; // tiempo tras encender sensor
const float K_SIGMA = 6.0;         // multiplicador de sigma para umbral (ajustable)
const int REQ_CONS_SEC = 3;        // lecturas consecutivas requeridas para confirmar cambio
const int MIN_READ_DELAY = 4;      // ms entre muestras individuales

// Variables runtime
float fondo_mean = 0;
float fondo_sigma = 0;
int umbral_det = 1000; // valor inicial (se recalcula en calibración)
bool estadoDetectado = false;
int contDetect = 0, contNoDetect = 0;

// Helpers
void encenderSensor()  { digitalWrite(PIN_CTRL, HIGH);  delay(TIEMPO_ESTABILIZACION_MS); }
void apagarSensor()    { digitalWrite(PIN_CTRL, LOW); }

void setup() {
  Serial.begin(115200);
  SerialBT.begin("SumoBot");
  pinMode(PIN_SENSOR, INPUT);
  pinMode(PIN_CTRL, OUTPUT);
  pinMode(LED_DET, OUTPUT);
  digitalWrite(LED_DET, LOW);

  // Inicial sensor apagado
  digitalWrite(PIN_CTRL, LOW);
  delay(100);

  // Calibración automática
  calibrarFondo();

  Serial.printf("Calibracion OK: mean=%.1f sigma=%.2f umbral=%d\n", fondo_mean, fondo_sigma, umbral_det);
}

void loop() {
  // Encendemos sensor, leemos y procesamos
  encenderSensor();
  int lectura = leerRobusto();   // mediana+promedio
  apagarSensor(); // opcional: apagar si quieres ahorrar (si mantienes encendido quitalo)

  Serial.printf("Lectura: %d  umbral: %d\n", lectura, umbral_det);

  // Histeresis/debounce
  if (lectura > umbral_det) {
    contDetect++; contNoDetect = 0;
  } else if (lectura < (umbral_det - (int)(fondo_sigma*1.0))) {
    // margen inferior para volver a NO detectado
    contNoDetect++; contDetect = 0;
  } else {
    // zona intermedia: reduce contadores lentamente
    contDetect = max(0, contDetect - 1);
    contNoDetect = max(0, contNoDetect - 1);
  }

  if (!estadoDetectado && contDetect >= REQ_CONS_SEC) {
    estadoDetectado = true;
    onDetectado();
  } else if (estadoDetectado && contNoDetect >= REQ_CONS_SEC) {
    estadoDetectado = false;
    onNoDetectado();
  }

  digitalWrite(LED_DET, estadoDetectado ? HIGH : LOW);

  delay(80); // ciclo principal
}

// --- FUNCIONES --- //

void calibrarFondo() {
  // Mide el "fondo" cuando no hay objeto. Se asume que en startup no hay objeto.
  encenderSensor();
  const int CAL_SAMPLES = 200;
  long acc = 0;
  long acc2 = 0;
  for (int i = 0; i < CAL_SAMPLES; ++i) {
    int v = analogRead(PIN_SENSOR);
    acc += v;
    acc2 += (long)v * (long)v;
    delay(8);
  }
  apagarSensor();

  fondo_mean = (float)acc / CAL_SAMPLES;
  float var = ((float)acc2 / CAL_SAMPLES) - (fondo_mean * fondo_mean);
  fondo_sigma = (var > 0) ? sqrt(var) : 1.0;
  umbral_det = (int)(fondo_mean + K_SIGMA * fondo_sigma);

  // límites sensatos
  if (umbral_det < 100) umbral_det = 100;
  if (fondo_sigma < 1.0) fondo_sigma = 1.0;
}

// lectura robusta: tomar N_SAMPLES, calcular mediana y promediar los 3 centrales
int leerRobusto() {
  int samples[N_SAMPLES];
  for (int i = 0; i < N_SAMPLES; ++i) {
    samples[i] = analogRead(PIN_SENSOR);
    delay(MIN_READ_DELAY);
  }
  // ordenar (simple insertion sort - N pequeño)
  for (int i = 1; i < N_SAMPLES; ++i) {
    int key = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j+1] = samples[j];
      j--;
    }
    samples[j+1] = key;
  }
  // tomar promedio de los 3 centrales (reduce picos)
  int mid = N_SAMPLES / 2;
  int sum = 0;
  int count = 3;
  if (N_SAMPLES >= 3) {
    sum = samples[mid-1] + samples[mid] + samples[mid+1];
    return sum / 3;
  } else {
    // fallback
    sum = 0;
    for (int i=0;i<N_SAMPLES;i++) sum += samples[i];
    return sum / N_SAMPLES;
  }
}

void onDetectado() {
  Serial.println(">>> DETECTADO <<<");
  // aquí pones la acción: ir atrás, frenar, etc.
}

void onNoDetectado() {
  Serial.println(">>> NO detectado <<<");
  // acción opuesta
}
