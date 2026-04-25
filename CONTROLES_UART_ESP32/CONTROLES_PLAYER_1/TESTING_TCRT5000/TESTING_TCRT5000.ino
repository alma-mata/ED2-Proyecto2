 int a, b, c;

const int sensorPin = 26;   // GPIO14 = ADC

const int irLed = 14;       // GPIO33 = IR LED

const int detectLed = 4;    // GPIO4  = LED indicador

int threshold = -20;       // Ajusta después de probar


void setup() {

  Serial.begin(115200);

  pinMode(irLed, OUTPUT);

  pinMode(detectLed, OUTPUT);

}


void loop() {

  // IR encendido

  digitalWrite(irLed, HIGH);

  delayMicroseconds(500);

  a = analogRead(sensorPin);


  // IR apagado

  digitalWrite(irLed, LOW);

  delayMicroseconds(500);

  b = analogRead(sensorPin);


  // Señal filtrada

  c = a - b;

  Serial.println(c);


  // Blanco (más negativo) → LED ON

  if (c < threshold) {

    digitalWrite(detectLed, HIGH);

  } else {

    digitalWrite(detectLed, LOW);

  }


delay(50);


} 