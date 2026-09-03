
#include <Ps3Controller.h>

const int PWMA = 14;
const int AIN1 = 27;
const int AIN2 = 26;
const int BIN1 = 25;
const int BIN2 = 33;
const int PWMB = 32;
const int STBY = 13;

const int PWM_FREQ = 5000;
const int PWM_RES  = 8;
const int CH_A = 0;
const int CH_B = 1;

const int DEADZONE   = 10;
const int MAX_JOY    = 128;
bool turboActivo = false;
const float TURBO_MULT  = 1.0;
const float NORMAL_MULT = 0.6;

void onConnect() {
  Serial.println("Control PS3 conectado!");
}

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

void procesarJoystick() {
  int y = -Ps3.data.analog.stick.ly; // invertido: arriba = negativo
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
  turboActivo = Ps3.data.analog.button.r2 > 100; // R2 analogico: 0-255

  if (Ps3.data.button.cross) {
    detenerMotores();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);

  ledcSetup(CH_A, PWM_FREQ, PWM_RES);
  ledcAttachPin(PWMA, CH_A);
  ledcSetup(CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(PWMB, CH_B);

  detenerMotores();

  Ps3.attach(nullptr);
  Ps3.attachOnConnect(onConnect);
  Ps3.begin();
  Serial.print("MAC del ESP32 (usar para emparejar el control): ");
  Serial.println(Ps3.getAddress());

  Serial.println("Esperando control PS3...");
}

void loop() {
  if (Ps3.isConnected()) {
    procesarBotones();
    procesarJoystick();
  } else {
    detenerMotores();
  }
  delay(20);
}
