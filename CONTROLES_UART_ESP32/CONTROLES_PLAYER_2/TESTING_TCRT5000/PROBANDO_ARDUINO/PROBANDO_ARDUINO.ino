int a, b, c;
const int irLed = 6;      // IR LED
const int sensorPin = A3; // Photodiode
const int detectLed = 2;  // LED indicador
const int threshold = -20; // Umbral entre blanco y negro

void setup() {
  Serial.begin(9600);
  pinMode(irLed, OUTPUT);
  pinMode(detectLed, OUTPUT);
}

void loop() {
  // Lectura con IR encendido
  digitalWrite(irLed, HIGH);    
  delayMicroseconds(500);  
  a = analogRead(sensorPin);    
  
  // Lectura con IR apagado
  digitalWrite(irLed, LOW);     
  delayMicroseconds(500);  
  b = analogRead(sensorPin);    
  
  // Señal filtrada
  c = a - b;  
  Serial.println(c);

  // Blanco = más negativo
  if (c < threshold) {
    digitalWrite(detectLed, HIGH);  // LED ON
  } else {
    digitalWrite(detectLed, LOW);   // LED OFF
  }

  delay(50); // suaviza el serial plot
}
