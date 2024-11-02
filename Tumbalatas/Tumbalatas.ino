#include "driver/mcpwm.h"
#include <WebServer.h>
#include <WiFi.h>

// ----- Credenciales de WiFi -----
const char *ssid = "RobotinMS";    // Nombre de la red WiFi creada por la ESP32
const char *password = "12345678"; // Contraseña de la red WiFi

// ----- Pines de los Motores -----

// Right Wheel (Rueda Derecha)
#define B1A 5                 // Dirección 1
#define B2A 2                 // Dirección 2
#define RIGHT_PWM GPIO_NUM_12 // PWM para la Rueda Derecha

// Left Wheel (Rueda Izquierda)
#define A1B 4                // Dirección 1
#define A1A 18               // Dirección 2
#define LEFT_PWM GPIO_NUM_13 // PWM para la Rueda Izquierda

// ----- Pines del Sensor de Distancia -----
#define ECHO 19
#define TRIG 21

// ----- Parámetros del Sensor de Distancia -----
const float SPEED_OF_SOUND = 0.0343; // cm/µs
const int MAX_DISTANCE = 200;        // Distancia máxima a detectar en cm
const int OBSTACLE_THRESHOLD = 20;   // Umbral para detener el automóvil en cm

// ----- Parámetros de Control de Velocidad -----
const float DEFAULT_SPEED = 50.0; // Velocidad por defecto en porcentaje (0-100)
const float MAX_SPEED = 100.0;    // Velocidad máxima
const float MIN_SPEED = 0.0;      // Velocidad mínima

// ----- Parámetros de MCPWM -----
#define MCPWM_UNIT MCPWM_UNIT_0
#define MCPWM_TIMER_RIGHT MCPWM_TIMER_0
#define MCPWM_TIMER_LEFT MCPWM_TIMER_1
#define MCPWM_GPIO_RIGHT_RIGHTPWM RIGHT_PWM
#define MCPWM_GPIO_LEFT_LEFTPWM LEFT_PWM

// ----- Parámetros de Control -----
bool obstacleDetected = false;
float currentSpeed = DEFAULT_SPEED;

WebServer server(80);

/**
 * Configura MCPWM para una rueda específica.
 * @param timer_num Número del timer MCPWM.
 * @param gpio_num Número del GPIO para PWM.
 */
void setupMCPWM(mcpwm_timer_t timer_num, gpio_num_t gpio_num) {
  mcpwm_gpio_init(MCPWM_UNIT, (timer_num == MCPWM_TIMER_0) ? MCPWM0A : MCPWM1A,
                  gpio_num);

  mcpwm_config_t pwm_config;
  pwm_config.frequency = 1000;
  pwm_config.cmpr_a = 0;
  pwm_config.cmpr_b = 0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

  mcpwm_init(MCPWM_UNIT, timer_num, &pwm_config);
}

/**
 * Establece el ciclo de trabajo PWM para una rueda.
 * @param timer_num Número del timer MCPWM.
 * @param duty Ciclo de trabajo en porcentaje (0-100).
 */
void setPWMDuty(mcpwm_timer_t timer_num, float duty) {
  mcpwm_set_duty(MCPWM_UNIT, timer_num, MCPWM_OPR_A, duty);
  mcpwm_set_duty_type(MCPWM_UNIT, timer_num, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
}

// Funciones de movimiento
void Adelante() {
  Serial.println("Adelante");
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, HIGH);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, HIGH);

  // Establecer velocidad
  setPWMDuty(MCPWM_TIMER_RIGHT, currentSpeed);
  setPWMDuty(MCPWM_TIMER_LEFT, currentSpeed);
}

void Atras() {
  Serial.println("Atras");
  digitalWrite(B1A, HIGH);
  digitalWrite(B2A, LOW);
  digitalWrite(A1B, HIGH);
  digitalWrite(A1A, LOW);

  // Establecer velocidad
  setPWMDuty(MCPWM_TIMER_RIGHT, currentSpeed);
  setPWMDuty(MCPWM_TIMER_LEFT, currentSpeed);
}

void Derecha() {
  Serial.println("Derecha");
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, LOW);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, HIGH);

  // Establecer velocidad
  setPWMDuty(MCPWM_TIMER_RIGHT, currentSpeed / 2); // Rueda derecha más lenta
  setPWMDuty(MCPWM_TIMER_LEFT,
             currentSpeed); // Rueda izquierda a velocidad normal
}

void Izquierda() {
  Serial.println("Izquierda");
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, HIGH);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, LOW);

  // Establecer velocidad
  setPWMDuty(MCPWM_TIMER_RIGHT,
             currentSpeed); // Rueda derecha a velocidad normal
  setPWMDuty(MCPWM_TIMER_LEFT, currentSpeed / 2); // Rueda izquierda más lenta
}

void Detener() {
  Serial.println("Detenido");
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, LOW);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, LOW);

  // Establecer ciclo de trabajo PWM a 0%
  setPWMDuty(MCPWM_TIMER_RIGHT, 0);
  setPWMDuty(MCPWM_TIMER_LEFT, 0);
}

void handleRoot() {
  server.send(
      200, "text/html",
      "<!DOCTYPE html>"
      "<html lang='es'>"
      "<head>"
      "<meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
      "<title>Control de Auto</title>"
      "<style>"
      "body {"
      "  -webkit-user-select: none;"

      "  -moz-user-select: none;"
      "  -ms-user-select: none;"
      "  user-select: none;"
      "  text-align: center;"
      "  font-family: Arial, sans-serif;"
      "}"
      ".container {"
      "  display: flex;"
      "  flex-direction: column;"
      "  align-items: center;"
      "  justify-content: center;"
      "  height: 100vh;"
      "}"
      ".row {"
      "  display: flex;"
      "  justify-content: center;"
      "  width: 100%;"
      "}"
      "button {"
      "  width: 100px;"
      "  height: 100px;"
      "  font-size: 24px;"
      "  background-color: lightblue;"
      "  border: none;"
      "  border-radius: 10px;"
      "  cursor: pointer;"
      "  outline: none;"
      "  margin: 15px;"
      "  padding: 10px;"
      "  display: flex;"
      "  align-items: center;"
      "  justify-content: center;"
      "}"
      "button:active {"
      "  background-color: deepskyblue;"
      "}"
      "#stop-button {"
      "  background-color: lightcoral;"
      "}"
      "</style>"
      "</head>"
      "<body>"
      "<h1>Control de Auto</h1>"
      "<div class='container'>"
      "<div class='row'>"
      "<button onmousedown=\"sendDirection('adelante')\" "
      "onmouseup=\"sendDirection('detener')\" "
      "ontouchstart=\"sendDirection('adelante')\" "
      "ontouchend=\"sendDirection('detener')\">Arriba</button>"
      "</div>"
      "<div class='row'>"
      "<button onmousedown=\"sendDirection('izquierda')\" "
      "onmouseup=\"sendDirection('detener')\" "
      "ontouchstart=\"sendDirection('izquierda')\" "
      "ontouchend=\"sendDirection('detener')\">Izquierda</button>"
      "<button id='stop-button' onmousedown=\"sendDirection('detener')\" "
      "ontouchstart=\"sendDirection('detener')\">STOP</button>"
      "<button onmousedown=\"sendDirection('derecha')\" "
      "onmouseup=\"sendDirection('detener')\" "
      "ontouchstart=\"sendDirection('derecha')\" "
      "ontouchend=\"sendDirection('detener')\">Derecha</button>"
      "</div>"
      "<div class='row'>"
      "<button onmousedown=\"sendDirection('atras')\" "
      "onmouseup=\"sendDirection('detener')\" "
      "ontouchstart=\"sendDirection('atras')\" "
      "ontouchend=\"sendDirection('detener')\">Abajo</button>"
      "</div>"
      "</div>"
      "<p id='status'></p>"
      "<p>Distancia: <span id='distance'>--</span> cm</p>"
      "<script>"
      "function sendDirection(direction) {"
      "  fetch('/' + direction).then(response => response.text()).then(data => "
      "{"
      "    document.getElementById('status').innerText = data;"
      "  });"
      "}"
      "function updateDistance() {"
      "  fetch('/distance').then(response => response.text()).then(data => {"
      "    document.getElementById('distance').innerText = data;"
      "  });"
      "}"
      "setInterval(updateDistance, 1000);"
      "</script>"
      "</body>"
      "</html>");
}

void handleAdelante() {
  if (!obstacleDetected) {
    Adelante();
    server.send(200, "text/plain", "Moviendo adelante");
  } else {
    server.send(200, "text/plain",
                "Obstáculo detectado. No se puede mover adelante.");
  }
}

void handleAtras() {
  Atras();
  server.send(200, "text/plain", "Moviendo atrás");
}

void handleDerecha() {
  if (!obstacleDetected) {
    Derecha();
    server.send(200, "text/plain", "Girando a la derecha");
  } else {
    server.send(200, "text/plain",
                "Obstáculo detectado. No se puede girar a la derecha.");
  }
}

void handleIzquierda() {
  if (!obstacleDetected) {
    Izquierda();
    server.send(200, "text/plain", "Girando a la izquierda");
  } else {
    server.send(200, "text/plain",
                "Obstáculo detectado. No se puede girar a la izquierda.");
  }
}

void handleDetener() {
  Detener();
  server.send(200, "text/plain", "Detenido");
}

void handleDistance() {
  float distance = getDistance();
  server.send(200, "text/plain", String(distance));
}

/**
 * Activar el sensor HC-SR04 para medir la distancia
 * @return Distancia medida en centímetros
 */
float getDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);

  float distance = (duration / 2.0) * SPEED_OF_SOUND;

  if (duration == 0) {
    distance = MAX_DISTANCE;
  }

  if (distance <= OBSTACLE_THRESHOLD) {
    obstacleDetected = true;
    Detener();
    Serial.println("Obstáculo detectado. Auto detenido.");
  } else {
    obstacleDetected = false;
  }

  return distance;
}

void setup() {
  Serial.begin(115200);

  pinMode(A1A, OUTPUT);
  pinMode(A1B, OUTPUT);
  pinMode(B1A, OUTPUT);
  pinMode(B2A, OUTPUT);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  setupMCPWM(MCPWM_TIMER_RIGHT, MCPWM_GPIO_RIGHT_RIGHTPWM);
  setupMCPWM(MCPWM_TIMER_LEFT, MCPWM_GPIO_LEFT_LEFTPWM);

  WiFi.softAP(ssid, password);
  Serial.println("Access Point iniciado");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP del servidor: ");
  Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/adelante", handleAdelante);
  server.on("/atras", handleAtras);
  server.on("/derecha", handleDerecha);
  server.on("/izquierda", handleIzquierda);
  server.on("/detener", handleDetener);
  server.on("/distance", handleDistance);

  server.onNotFound([]() { server.send(404, "text/plain", "404: Not Found"); });

  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() { server.handleClient(); }