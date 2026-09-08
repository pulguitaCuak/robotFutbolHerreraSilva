#include <Ps3Controller.h>

// ---------- PINES MOTORES ----------
const int IN1 = 16; 
const int IN2 = 17; 
const int IN3 = 18; 
const int IN4 = 19; 

// ---------- PINES BATERIA ----------
const int PIN_BATERIA = 32;
const int PIN_DIN1 = 36;      
const int PIN_DIN2 = 39;      
const int PIN_DIN3 = 34;      
const int PIN_DIN4 = 35;      

// ---------- PINES INDICADORES ----------
const int PIN_BUZZER = 21;    
const int PIN_RGB_R  = 3;      
const int PIN_RGB_G  = 22;    
const int PIN_RGB_B  = 23;    

// ---------- PWM (LEDC) ----------
const int PWM_FREQ = 5000;
const int PWM_RES  = 8;
const int CH_IN1 = 0;
const int CH_IN2 = 1;
const int CH_IN3 = 2;
const int CH_IN4 = 3;
const int CH_RGB_R = 4;
const int CH_RGB_G = 5;
const int CH_RGB_B = 6;
const int CH_BUZZER = 7;

// ---------- CONTROL ----------
const int DEADZONE   = 10;
const int MAX_JOY    = 128;
bool turboActivo = false;
const float TURBO_MULT  = 1.0;
const float NORMAL_MULT = 0.6;

// ---------- CONFIG BATERIA ----------
// Divisor con R8=100k  y R9=47k
const float DIVIDER_RATIO = 47.0 / (100.0 + 47.0);
const int CELDAS = 2;
const float V_MAX_CELDA = 4.20;   
const float V_NOM_CELDA = 3.65;  
const float V_MIN_CELDA = 3.30;    
unsigned long ultimaLecturaBateria = 0;
const unsigned long INTERVALO_BATERIA_MS = 2000;

// ---------- BUZZER ----------
const int BUZZER_FREQS[3] = {1800, 2400, 3200};
const unsigned long BUZZER_ON_MS  = 90;
const unsigned long BUZZER_OFF_MS = 60;
int buzzerPaso = 0;
unsigned long buzzerCambioEn = 0;

// ---------- CALLBACK DE CONEXION ----------
void onConnect() {
  Serial.println("Control PS3 conectado!");
}

// ---------- FUNCIONES DE MOTOR ----------
void detenerMotores() {
  ledcWrite(CH_IN1, 0); ledcWrite(CH_IN2, 0);
  ledcWrite(CH_IN3, 0); ledcWrite(CH_IN4, 0);
}

void motorIzq(int vel) {
  if (vel > 0)      { ledcWrite(CH_IN1, constrain(vel, 0, 255)); ledcWrite(CH_IN2, 0); }
  else if (vel < 0) { ledcWrite(CH_IN1, 0); ledcWrite(CH_IN2, constrain(-vel, 0, 255)); }
  else              { ledcWrite(CH_IN1, 0); ledcWrite(CH_IN2, 0); }
}

void motorDer(int vel) {
  if (vel > 0)      { ledcWrite(CH_IN3, constrain(vel, 0, 255)); ledcWrite(CH_IN4, 0); }
  else if (vel < 0) { ledcWrite(CH_IN3, 0); ledcWrite(CH_IN4, constrain(-vel, 0, 255)); }
  else              { ledcWrite(CH_IN3, 0); ledcWrite(CH_IN4, 0); }
}

void avanzar(int vel)       { motorIzq(vel);  motorDer(vel); }
void retroceder(int vel)    { motorIzq(-vel); motorDer(-vel); }
void girarDerecha(int vel)  { motorIzq(vel);  motorDer(-vel); }
void girarIzquierda(int vel){ motorIzq(-vel); motorDer(vel); }

// ---------- CONTROL PRINCIPAL ----------
void procesarJoystick() {
  int y = -Ps3.data.analog.stick.ly;
  int x =  Ps3.data.analog.stick.lx;

  if (abs(y) < DEADZONE) y = 0;
  if (abs(x) < DEADZONE) x = 0;

  int velIzq = y + x;
  int velDer = y - x;

  velIzq = map(constrain(velIzq, -MAX_JOY, MAX_JOY), -MAX_JOY, MAX_JOY, -255, 255);
  velDer = map(constrain(velDer, -MAX_JOY, MAX_JOY), -MAX_JOY, MAX_JOY, -255, 255);

  float mult = turboActivo ? TURBO_MULT : NORMAL_MULT;
  velIzq = (int)(velIzq * mult);
  velDer = (int)(velDer * mult);

  motorIzq(velIzq);
  motorDer(velDer);
}

void iniciarBuzzer() {
  buzzerPaso = 1;
  ledcWriteTone(CH_BUZZER, BUZZER_FREQS[0]);
  buzzerCambioEn = millis() + BUZZER_ON_MS;
}

void actualizarBuzzer() {
  if (buzzerPaso == 0 || millis() < buzzerCambioEn) return;

  switch (buzzerPaso) {
    case 1:
      ledcWrite(CH_BUZZER, 0);
      buzzerCambioEn = millis() + BUZZER_OFF_MS;
      buzzerPaso = 2;
      break;
    case 2:
      ledcWriteTone(CH_BUZZER, BUZZER_FREQS[1]);
      buzzerCambioEn = millis() + BUZZER_ON_MS;
      buzzerPaso = 3;
      break;
    case 3:
      ledcWrite(CH_BUZZER, 0);
      buzzerCambioEn = millis() + BUZZER_OFF_MS;
      buzzerPaso = 4;
      break;
    case 4:
      ledcWriteTone(CH_BUZZER, BUZZER_FREQS[2]);
      buzzerCambioEn = millis() + BUZZER_ON_MS;
      buzzerPaso = 5;
      break;
    case 5:
      ledcWrite(CH_BUZZER, 0);
      buzzerPaso = 0;
      break;
  }
}

void procesarBotones() {
  turboActivo = Ps3.data.analog.button.r2 > 100;

  // boton de freno de emergencia
  if (Ps3.data.button.cross) {
    detenerMotores();
  }
}

// ---------- MODULO DE BATERIA ----------
float leerVoltajeBateria() {
  uint32_t mv = analogReadMilliVolts(PIN_BATERIA);
  float vPin = mv / 1000.0;
  return vPin / DIVIDER_RATIO;
}

int calcularPorcentaje(float vBateria) {
  float vMax = V_MAX_CELDA * CELDAS;   // bateria llena (8.4V)
  float vMin = V_MIN_CELDA * CELDAS;   // bateria vacia (6.6V)
  float pct = (vBateria - vMin) / (vMax - vMin) * 100.0;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (int)pct;
}

void actualizarRgbBateria(int pct) {
  if (pct > 66) {           // verde
    ledcWrite(CH_RGB_R, 0);   ledcWrite(CH_RGB_G, 255); ledcWrite(CH_RGB_B, 0);
  } else if (pct > 33) {    // amarillo
    ledcWrite(CH_RGB_R, 255); ledcWrite(CH_RGB_G, 255); ledcWrite(CH_RGB_B, 0);
  } else {                  // rojo
    ledcWrite(CH_RGB_R, 255); ledcWrite(CH_RGB_G, 0);   ledcWrite(CH_RGB_B, 0);
  }
}

void apagarRgb() {
  ledcWrite(CH_RGB_R, 0); ledcWrite(CH_RGB_G, 0); ledcWrite(CH_RGB_B, 0);
}

void chequearBateria() {
  unsigned long ahora = millis();
  if (ahora - ultimaLecturaBateria < INTERVALO_BATERIA_MS) return;
  ultimaLecturaBateria = ahora;

  bool d1 = digitalRead(PIN_DIN1); // DIP1: mostrar bateria
  bool d2 = digitalRead(PIN_DIN2); // DIP2: mostrar si esta vinculado el joystick
  bool d3 = digitalRead(PIN_DIN3);
  bool d4 = digitalRead(PIN_DIN4);

  if (d1) {
    float vBat = leerVoltajeBateria();
    int pct = calcularPorcentaje(vBat);
    actualizarRgbBateria(pct);
    Serial.printf("Bateria: %.2fV (2S) -> %d%%\n", vBat, pct);
  } else {
    apagarRgb();
  }

  if (d2) {
    Serial.printf("Joystick vinculado: %s\n", Ps3.isConnected() ? "SI" : "NO");
  }

  Serial.printf("DIP: %d%d%d%d\n", d1, d2, d3, d4);
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);

  // Motores (L298N)
  ledcSetup(CH_IN1, PWM_FREQ, PWM_RES); ledcAttachPin(IN1, CH_IN1);
  ledcSetup(CH_IN2, PWM_FREQ, PWM_RES); ledcAttachPin(IN2, CH_IN2);
  ledcSetup(CH_IN3, PWM_FREQ, PWM_RES); ledcAttachPin(IN3, CH_IN3);
  ledcSetup(CH_IN4, PWM_FREQ, PWM_RES); ledcAttachPin(IN4, CH_IN4);
  detenerMotores();

  // RGB
  ledcSetup(CH_RGB_R, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_RGB_R, CH_RGB_R);
  ledcSetup(CH_RGB_G, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_RGB_G, CH_RGB_G);
  ledcSetup(CH_RGB_B, PWM_FREQ, PWM_RES); ledcAttachPin(PIN_RGB_B, CH_RGB_B);

  // Buzzer
  ledcSetup(CH_BUZZER, BUZZER_FREQS[0], PWM_RES);
  ledcAttachPin(PIN_BUZZER, CH_BUZZER);
  ledcWrite(CH_BUZZER, 0);

  // Bateria
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATERIA, ADC_11db);

  // DIP switch (guardado para el futuro)
  pinMode(PIN_DIN1, INPUT);
  pinMode(PIN_DIN2, INPUT);
  pinMode(PIN_DIN3, INPUT);
  pinMode(PIN_DIN4, INPUT);

  // PS3
  Ps3.attach(nullptr);
  Ps3.attachOnConnect(onConnect);
  Ps3.begin();
  Serial.print("MAC del ESP32 (usar para emparejar el control): ");
  Serial.println(Ps3.getAddress());

  Serial.println("Esperando control PS3...");

  iniciarBuzzer();
}

// ---------- LOOP ----------
void loop() {
  if (Ps3.isConnected()) {
    procesarBotones();
    procesarJoystick();
  } else {
    detenerMotores();
  }

  chequearBateria();
  actualizarBuzzer();

  delay(20);
}
