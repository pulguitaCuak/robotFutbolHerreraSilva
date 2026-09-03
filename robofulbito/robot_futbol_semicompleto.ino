#include <Ps3Controller.h>

// ---------- PINES MOTORES ----------
const int PWMA = 14;
const int AIN1 = 27;
const int AIN2 = 26;
const int BIN1 = 25;
const int BIN2 = 33;
const int PWMB = 32;
const int STBY = 13;

// ---------- PINES BATERIA ----------
const int PIN_BATERIA = 34; 
const int PIN_LED1 = 16;     
const int PIN_LED2 = 17;     
const int PIN_LED3 = 18;     
const int PIN_LED4 = 19;     
const int PIN_DIP_BIT0 = 4;  
const int PIN_DIP_BIT1 = 5;  

// ---------- PWM (LEDC) ----------
const int PWM_FREQ = 5000;
const int PWM_RES  = 8;      
const int CH_A = 0;
const int CH_B = 1;

// ---------- CON
const int DEADZONE   = 10;
const int MAX_JOY    = 128;
bool turboActivo = false;
const float TURBO_MULT  = 1.0;
const float NORMAL_MULT = 0.6;

// ---------- CONFIG BATERIA (2S fija) ----------
const float DIVIDER_RATIO = 22.0 / (100.0 + 22.0);
const int CELDAS = 2;             
const float V_MAX_CELDA = 4.20;   
const float V_MIN_CELDA = 3.30;  
unsigned long ultimaLecturaBateria = 0;
const unsigned long INTERVALO_BATERIA_MS = 2000;

// ---------- CALLBACK DE CONEXION ----------
void onConnect() {
  Serial.println("Control PS3 conectado!");
}

// ---------- FUNCIONES DE MOTOR ----------
void detenerMotores() {
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW);
  ledcWrite(CH_A, 0);
  ledcWrite(CH_B, 0);
}

void motorIzq(int vel) {
  if (vel > 0) { digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW); }
  else if (vel < 0) { digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH); }
  else { digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW); }
  ledcWrite(CH_A, abs(vel));
}

void motorDer(int vel) {
  if (vel > 0) { digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW); }
  else if (vel < 0) { digitalWrite(BIN1, LOW); digitalWrite(BIN2, HIGH); }
  else { digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW); }
  ledcWrite(CH_B, abs(vel));
}

void avanzar(int vel)      { motorIzq(vel);  motorDer(vel); }
void retroceder(int vel)   { motorIzq(-vel); motorDer(-vel); }
void girarDerecha(int vel) { motorIzq(vel);  motorDer(-vel); }
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
  float vMax = V_MAX_CELDA * CELDAS;   // bateria llena(8,4v)
  float vMin = V_MIN_CELDA * CELDAS;   // bateria vacia(6,6v)
  float pct = (vBateria - vMin) / (vMax - vMin) * 100.0;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (int)pct;
}

void actualizarLedsBateria(int pct) {
  digitalWrite(PIN_LED1, pct > 0  ? HIGH : LOW);
  digitalWrite(PIN_LED2, pct > 25 ? HIGH : LOW);
  digitalWrite(PIN_LED3, pct > 50 ? HIGH : LOW);
  digitalWrite(PIN_LED4, pct > 75 ? HIGH : LOW);
}

void chequearBateria() {
  unsigned long ahora = millis();
  if (ahora - ultimaLecturaBateria < INTERVALO_BATERIA_MS) return;
  ultimaLecturaBateria = ahora;

  float vBat = leerVoltajeBateria();
  int pct = calcularPorcentaje(vBat);
  actualizarLedsBateria(pct);

  bool bit0 = digitalRead(PIN_DIP_BIT0);
  bool bit1 = digitalRead(PIN_DIP_BIT1);

  Serial.printf("Bateria: %.2fV (2S) -> %d%% | DIP: bit1=%d bit0=%d\n",
                vBat, pct, bit1, bit0);
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);

  // Motores
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);

  ledcSetup(CH_A, PWM_FREQ, PWM_RES);
  ledcAttachPin(PWMA, CH_A);
  ledcSetup(CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(PWMB, CH_B);

  detenerMotores();

  // Bateria
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_LED3, OUTPUT);
  pinMode(PIN_LED4, OUTPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATERIA, ADC_11db);

  // DIP switch (guardado para el futuro creo xD)
  pinMode(PIN_DIP_BIT0, INPUT_PULLUP);
  pinMode(PIN_DIP_BIT1, INPUT_PULLUP);

  // PS3
  Ps3.attach(nullptr);       
  Ps3.attachOnConnect(onConnect);
  Ps3.begin();
  Serial.print("MAC del ESP32 (usar para emparejar el control): ");
  Serial.println(Ps3.getAddress());

  Serial.println("Esperando control PS3...");
}

// ---------- LOOP ----------
void loop() {
  if (Ps3.isConnected()) {
    procesarBotones();
    procesarJoystick();
  } else {
    detenerMotores();
  }

  chequearBateria(); //esta controlado con millis por lo q no deberia afectar el rendimiento

  delay(20);
}
