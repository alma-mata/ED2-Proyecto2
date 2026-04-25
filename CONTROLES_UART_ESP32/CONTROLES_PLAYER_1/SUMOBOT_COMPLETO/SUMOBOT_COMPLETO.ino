#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/* **************************************************************************
 * ASIGNACIÓN DE HARDWARE 
 * ************************************************************************** */
#define M_MIXER   15  
#define M_PUMP    2   
#define PIN_OUT_NIVEL 19 
#define PIN_OUT_TEMP  25 

// Botones y Selectores
#define BTN_START    33
#define BTN_STOP     32
#define BTN_ESTOP    35
#define BTN_SELECTOR 34 

// Botones Manuales
#define BTN_MAN_V_A   39
#define BTN_MAN_V_B   36
#define BTN_MAN_HEAT  1
#define BTN_MAN_DRAIN 23

struct TanqueRGB { int r; int g; };
TanqueRGB tA = {13, 12}; 
TanqueRGB tB = {14, 27}; 
TanqueRGB tR = {4, 16};  
TanqueRGB tL = {17, 5};  

/* **************************************************************************
 * VARIABLES DE PROCESO
 * ************************************************************************** */
enum Estado { IDLE, AUTO_LLENADO, AUTO_MEZCLA, AUTO_VACIADO, MANUAL, EMERGENCIA };
Estado estadoActual = IDLE;

float tanque1 = 5.5, tanque2 = 5.5, principal = 0, almacen = 0;
float temperatura = 25.0;
unsigned long tReferencia = 0, lastUpdate = 0;

// Bandera para el botón selector
bool modoAuto = true;     
bool ultSelector = HIGH;  
unsigned long tDebounceSelector = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setRGB(TanqueRGB t, int rVal, int gVal) {
  ledcWrite(t.r, rVal);
  ledcWrite(t.g, gVal);
}

void actualizarLED_Proporcional(TanqueRGB t, float nivel, float maxL) {
  long n = constrain(nivel * 100, 0, maxL * 100);
  long m = maxL * 100;
  
  int r = map(n, 0, m, 255, 0);
  int g = map(n, 0, m, 0, 255);
  
  setRGB(t, constrain(r, 0, 255), constrain(g, 0, 255));
}

void actualizarLEDs() {
  actualizarLED_Proporcional(tA, tanque1, 5.5);
  actualizarLED_Proporcional(tB, tanque2, 5.5);
  actualizarLED_Proporcional(tR, principal, 2.5);
  actualizarLED_Proporcional(tL, almacen, 2.5);
}

void setup() {
  lcd.init(); lcd.backlight();
  
  // ANCLAJE DE SEGURIDAD PARA MOTORES
  ledcAttach(M_MIXER, 5000, 8); 
  ledcWrite(M_MIXER, 0); 
  
  ledcAttach(M_PUMP, 5000, 8); 
  ledcWrite(M_PUMP, 0);
  
  // ANCLAJE DE SEGURIDAD PARA SALIDAS PLC
  ledcAttach(PIN_OUT_NIVEL, 2000, 8);
  ledcWrite(PIN_OUT_NIVEL, 0);
  
  ledcAttach(PIN_OUT_TEMP, 2000, 8);
  ledcWrite(PIN_OUT_TEMP, 0);
  
  // ANCLAJE PARA LEDS
  int pinsVisual[] = {13, 12, 14, 27, 4, 16, 17, 5};
  for(int p : pinsVisual) {
    ledcAttach(p, 2000, 8);
    ledcWrite(p, 0);
  }

  int inPins[] = {33, 32, 35, 34, 39, 36, 1, 23};
  for(int p : inPins) pinMode(p, INPUT);

  lastUpdate = millis();
}

void loop() {
  unsigned long now = millis();
  float dt = (now - lastUpdate) / 1000.0;
  lastUpdate = now;

  if (digitalRead(BTN_ESTOP) == LOW) estadoActual = EMERGENCIA;

  // Lógica de Bandera para el Selector
  bool lecSelector = digitalRead(BTN_SELECTOR);
  if (lecSelector == LOW && ultSelector == HIGH && (now - tDebounceSelector > 200)) {
    modoAuto = !modoAuto; 
    estadoActual = IDLE;  
    lcd.clear();
    tDebounceSelector = now;
  }
  ultSelector = lecSelector;

  switch (estadoActual) {
    case IDLE:
      ledcWrite(M_MIXER, 0); ledcWrite(M_PUMP, 0);
      lcd.setCursor(0,0); lcd.print("   Mezcladora   ");
      lcd.setCursor(0,1); lcd.print(modoAuto ? "   AUTO READY   " : "  MANUAL READY  ");
      
      if (digitalRead(BTN_START) == LOW) {
        tanque1 = 5.5; tanque2 = 5.5; principal = 0; almacen = 0;
        temperatura = 25.0; lcd.clear();
        estadoActual = modoAuto ? AUTO_LLENADO : MANUAL;
      }
      break;

    case AUTO_LLENADO:
      if (principal < 2.5) {
        float tasa = 0.08 * dt; 
        if (tanque1 > 0) tanque1 -= tasa; 
        if (tanque2 > 0) tanque2 -= tasa; 
        principal += (tasa * 2);
      } else {
        estadoActual = AUTO_MEZCLA;
        tReferencia = 0;
      }
      lcd.setCursor(0,0); lcd.print("T1:" + String(tanque1,1) + " T2:" + String(tanque2,1) + "L ");
      lcd.setCursor(0,1); lcd.print("PRINCIPAL: " + String(principal,1) + "L ");
      if (digitalRead(BTN_STOP) == LOW) estadoActual = IDLE;
      break;

    case AUTO_MEZCLA:
      if (temperatura < 60.0) {
        temperatura += 1.3 * dt;
        
        int tempInt = constrain((int)temperatura, 25, 60);
        int velMixer = map(tempInt, 25, 60, 120, 255); 
        ledcWrite(M_MIXER, velMixer); 
        
        lcd.setCursor(0,0); lcd.print("TEMP: " + String(temperatura,1) + " C   ");
        lcd.setCursor(0,1); lcd.print("MOTOR: " + String(velMixer) + " PWM  "); 
      } else {
        ledcWrite(M_MIXER, 255); 
        if (tReferencia == 0) tReferencia = millis();
        int cuenta = 10 - (millis() - tReferencia) / 1000;
        lcd.setCursor(0,0); lcd.print("TEMP: 60.0 C OK ");
        lcd.setCursor(0,1); lcd.print("ESPERA: " + String(cuenta) + "s    ");
        if (millis() - tReferencia >= 10000) estadoActual = AUTO_VACIADO;
      }
      if (digitalRead(BTN_STOP) == LOW) estadoActual = IDLE;
      break;

    case AUTO_VACIADO:
      ledcWrite(M_PUMP, 255); // Enciende la bomba de vaciado
      ledcWrite(M_MIXER, 0);  // APAGA EL AGITADOR INMEDIATAMENTE
      
      if (principal > 0.05) {
        float tasaV = 0.28 * dt;
        principal -= tasaV; almacen += tasaV;
        lcd.setCursor(0,0); lcd.print("VACIADO...      ");
        lcd.setCursor(0,1); lcd.print("ALM:" + String(almacen,1) + " MIX:" + String(principal,1));
      } else {
        ledcWrite(M_PUMP, 0); ledcWrite(M_MIXER, 0);
        estadoActual = IDLE;
        lcd.clear(); lcd.print(" PROCESO FIN "); delay(3000);
      }
      if (digitalRead(BTN_STOP) == LOW) estadoActual = IDLE;
      break;

    case MANUAL: { 
      if (modoAuto) { estadoActual = IDLE; lcd.clear(); break; }

      bool calentando = (digitalRead(BTN_MAN_HEAT) == LOW);
      bool maxAlcanzado = (principal >= 2.5 && (digitalRead(BTN_MAN_V_A) == LOW || digitalRead(BTN_MAN_V_B) == LOW));
      bool drenando = (digitalRead(BTN_MAN_DRAIN) == LOW);

      // Calculamos la velocidad dinámica para la LCD
      int velMixerMan = 0;
      if (calentando && !drenando) { // Solo calcula velocidad si no está drenando
        int tempIntMan = constrain((int)temperatura, 25, 60);
        velMixerMan = map(tempIntMan, 25, 60, 120, 255);
      }

      if (maxAlcanzado) {
        lcd.setCursor(0,0); lcd.print("!!! ATENCION !!!");
        lcd.setCursor(0,1); lcd.print("REACTOR AL MAX  ");
      } else if (calentando) {
        lcd.setCursor(0,0); lcd.print("TEMP: " + String(temperatura,1) + " C   ");
        lcd.setCursor(0,1); lcd.print("MOT: " + String(velMixerMan) + " PWM    ");
      } else {
        lcd.setCursor(0,0); lcd.print("T1:" + String(tanque1,1) + " T2:" + String(tanque2,1) + "L ");
        lcd.setCursor(0,1); lcd.print("P:" + String(principal,1) + "L A:" + String(almacen,1) + "L ");
      }

      // Lógica de llenado manual
      if (digitalRead(BTN_MAN_V_A) == LOW && tanque1 > 1.0 && principal < 2.5) {
        tanque1 -= 0.4 * dt; principal += 0.4 * dt;
      }
      if (digitalRead(BTN_MAN_V_B) == LOW && tanque2 > 1.0 && principal < 2.5) {
        tanque2 -= 0.4 * dt; principal += 0.4 * dt;
      }
      
      // Lógica de Vaciado (Prioridad)
      if (drenando && principal > 0 && almacen < 2.5) {
        ledcWrite(M_PUMP, 255); 
        ledcWrite(M_MIXER, 0); // Prioridad: Apaga agitador si se está drenando
        principal -= 0.5 * dt; almacen += 0.5 * dt;
      } else { 
        ledcWrite(M_PUMP, 0); 
        // Si no está drenando, permite el agitador/calor
        if (calentando) {
          if (temperatura < 60.0) temperatura += 2.0 * dt; 
          ledcWrite(M_MIXER, velMixerMan); 
        } else { 
          ledcWrite(M_MIXER, 0); 
        }
      }

      if (digitalRead(BTN_STOP) == LOW) estadoActual = IDLE;
      break;
    }

    case EMERGENCIA:
      ledcWrite(M_MIXER, 0); ledcWrite(M_PUMP, 0);
      lcd.setCursor(0,0); lcd.print("!!! E-STOP !!!  ");
      lcd.setCursor(0,1); lcd.print("REVISAR BOTON   ");
      if (digitalRead(BTN_ESTOP) == HIGH) estadoActual = IDLE;
      break;
  }

  actualizarLEDs();
  
  ledcWrite(PIN_OUT_NIVEL, map(constrain(principal, 0, 2.5), 0, 2.5, 0, 255));
  ledcWrite(PIN_OUT_TEMP, map(constrain(temperatura, 25, 60), 25, 60, 0, 255));
  
  delay(30);
}