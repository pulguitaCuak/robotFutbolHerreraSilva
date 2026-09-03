# Robot de Fútbol — ESP32 + PS3 Controller

Robot de tracción diferencial controlado con un joystick de PlayStation 3 vía Bluetooth. La velocidad de cada motor es proporcional a la fuerza aplicada al stick, e incluye monitoreo de batería integrado.

## Índice

- [Descripción](#descripción)
- [Características](#características)
- [Hardware necesario](#hardware-necesario)
- [Conexiones](#conexiones)
- [Instalación](#instalación)
- [Emparejamiento del control PS3](#emparejamiento-del-control-ps3)
- [Uso](#uso)
- [Estructura del código](#estructura-del-código)
- [Configuración](#configuración)
- [Roadmap](#roadmap)
- [Licencia](#licencia)

## Descripción

Este proyecto controla un robot de 2 motores DC con caja reductora (configuración diferencial) usando un ESP32 y un control DualShock 3 conectado por Bluetooth. El movimiento se maneja con un solo joystick de forma proporcional: cuanto más se inclina el stick, más rápido giran los motores, permitiendo avance, retroceso, giro en el lugar y curvas en cualquier diagonal desde un único control analógico.

Incluye también un módulo de monitoreo de batería (LiPo 2S) que mide el voltaje real mediante un divisor de tensión y lo muestra en una barra de 4 LEDs.

## Características

- Control proporcional con un joystick PS3 vía Bluetooth, sin dongle, conexión directa al ESP32
- Modo turbo (velocidad máxima) activable con el gatillo R2
- Freno de emergencia con el botón X (cross)
- Monitor de batería en tiempo real (divisor de tensión + barra de 4 LEDs)
- Parada automática de motores si se pierde la conexión con el control
- DIP switch cableado y reservado para funcionalidades futuras

## Hardware necesario

| Componente | Cantidad | Notas |
|---|---|---|
| ESP32 DevKit (WROOM-32) | 1 | Necesita Bluetooth Classic |
| Driver TB6612FNG | 1 | Driver de motores dual |
| Motor DC con caja reductora | 2 | Configuración diferencial |
| Control DualShock 3 (PS3) | 1 | Ver sección de emparejamiento |
| Batería LiPo 2S (7.4V) | 1 | Alimentación de motores |
| Resistencia 100kΩ | 1 | Divisor de tensión (R1) |
| Resistencia 22kΩ | 1 | Divisor de tensión (R2) |
| LED | 4 | Indicador de nivel de batería |
| Resistencia 220-330Ω | 4 | Limitadoras para los LEDs |
| DIP switch (2 posiciones) | 1 | Reservado para uso futuro |

## Conexiones

### Driver de motores (TB6612FNG → ESP32)

| Pin TB6612 | Pin ESP32 | Función |
|---|---|---|
| PWMA | GPIO 14 | Velocidad motor izquierdo |
| AIN1 | GPIO 27 | Dirección motor izquierdo |
| AIN2 | GPIO 26 | Dirección motor izquierdo |
| BIN1 | GPIO 25 | Dirección motor derecho |
| BIN2 | GPIO 33 | Dirección motor derecho |
| PWMB | GPIO 32 | Velocidad motor derecho |
| STBY | GPIO 13 | Habilita el driver (HIGH obligatorio) |

### Monitor de batería

```
Batería(+) ---[ R1 = 100kΩ ]---+---[ R2 = 22kΩ ]--- GND
                                 |
                              GPIO 34
```

| Componente | Pin ESP32 |
|---|---|
| Divisor de tensión (lectura) | GPIO 34 |
| LED 25% | GPIO 16 |
| LED 50% | GPIO 17 |
| LED 75% | GPIO 18 |
| LED 100% | GPIO 19 |
| DIP switch bit 0 (reservado) | GPIO 4 |
| DIP switch bit 1 (reservado) | GPIO 5 |

Nota: el divisor de tensión (100kΩ / 22kΩ) soporta baterías de hasta ~18V sin superar los 3.3V seguros del ADC del ESP32. Si se cambia de batería, hay que recalcular las resistencias.

## Instalación

1. Instalar Arduino IDE con soporte para ESP32.
2. Instalar la librería Ps3Controller (jvpernis) desde el Library Manager, buscando "PS3 Controller Host", o desde su repositorio oficial (github.com/jvpernis/esp32-ps3).
3. Clonar este repositorio:
   ```bash
   git clone https://github.com/pulguitaCuak/robot-futbol-esp32.git
   ```
4. Abrir el `.ino` en Arduino IDE, seleccionar la placa ESP32 correspondiente y el puerto serie.
5. Compilar y subir.

## Emparejamiento del control PS3

A diferencia de otras librerías (como Bluepad32), Ps3Controller no hace pairing libre: el DualShock 3 tiene grabada la MAC del último dispositivo al que se conectó (normalmente una PS3 o una notebook). Para conectarlo al ESP32:

1. Subir el código al ESP32 y abrir el Monitor Serie — va a imprimir su propia dirección MAC Bluetooth.
2. Conectar el control por cable USB a una PC con Linux.
3. Usar una herramienta tipo sixaxispairer para reprogramar la MAC "host" grabada en el control, apuntándola a la MAC del ESP32 obtenida en el paso 1.
4. Desconectar el USB y presionar el botón PS del control — a partir de ahora se conecta directo al ESP32.

Este paso se hace una sola vez por control.

## Uso

| Control | Acción |
|---|---|
| Stick izquierdo | Movimiento proporcional (avance/retroceso/giro/curvas) |
| Gatillo R2 | Turbo (100% de velocidad) |
| Botón X (cross) | Freno de emergencia |

Si se pierde la conexión Bluetooth con el control, los motores se detienen automáticamente por seguridad.

## Estructura del código

```
robot_futbol_ps3lib_2s.ino
├── Configuración de pines (motores, batería, LEDs, DIP switch)
├── Configuración de PWM (LEDC)
├── onConnect()             callback al conectar el control
├── detenerMotores()        frena ambos motores
├── motorIzq() / motorDer() control individual por motor (dirección + PWM)
├── avanzar/retroceder/girar  movimientos con nombre (utilitarios)
├── procesarJoystick()      lee el stick y calcula velocidad proporcional
├── procesarBotones()       turbo y freno de emergencia
├── leerVoltajeBateria()    lee el ADC y calcula el voltaje real
├── calcularPorcentaje()    convierte voltaje a porcentaje de carga
├── actualizarLedsBateria() actualiza la barra de LEDs
├── chequearBateria()       orquesta la lectura periódica (cada 2s)
├── setup()                 inicialización de todo el hardware
└── loop()                  lee el control y actualiza motores/batería
```

## Configuración

Los valores principales se pueden ajustar al inicio del código:

```cpp
const int DEADZONE   = 10;     // zona muerta del joystick
const float NORMAL_MULT = 0.6; // velocidad normal (60%)
const float TURBO_MULT  = 1.0; // velocidad turbo (100%)
const int CELDAS = 2;          // cantidad de celdas de la batería LiPo
```

## Roadmap

- Definir y programar la función del DIP switch (actualmente reservado, solo se lee su estado por Serial)
- Calibrar el divisor de tensión con multímetro
- Chasis y ensamblaje final del robot

## Licencia

Este proyecto está bajo la licencia MIT — libre para usar, modificar y distribuir.

---

Desarrollado por [@pulguitaCuak](https://github.com/pulguitaCuak)
