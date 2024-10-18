/*
  Control de Automóvil con ESP32 y Sensor de Distancia HC-SR04
  ============================================================

  Este sketch permite controlar un automóvil mediante una interfaz web alojada en el ESP32.
  Además, integra un sensor de distancia HC-SR04 para detectar obstáculos y detener el automóvil
  automáticamente si se encuentra un objeto demasiado cerca.

  Componentes:
  - ESP32 Development Board
  - Motor Driver (por ejemplo, L298N)
  - HC-SR04 Ultrasonic Distance Sensor
  - Resistores (para divisor de voltaje en el pin ECHO)
  - Breadboard y Jumper Wires

  Autor: [Tu Nombre]
  Fecha: [Fecha]
*/

#include <WiFi.h>
#include <WebServer.h>
#include "driver/mcpwm.h"

// ----- Credenciales de WiFi -----
const char* ssid = "RobotinMS";      // Nombre de la red WiFi creada por la ESP32
const char* password = "12345678";   // Contraseña de la red WiFi

// ----- Pines de los Motores -----

// Right Wheel
#define B1A 5
#define B2A 2
// Left Wheel
#define A1B 4
#define A1A 18

// ----- Pines del Sensor de Distancia -----
#define ECHO 19
#define TRIG 21

// ----- Parámetros del Sensor de Distancia -----
const float SPEED_OF_SOUND = 0.0343;  // cm/µs
const int MAX_DISTANCE = 200;         // Distancia máxima a detectar en cm
const int OBSTACLE_THRESHOLD = 20;    // Umbral para detener el automóvil en cm

// ----- Parámetros de Control -----
bool obstacleDetected = false;

WebServer server(80);

void Adelante() {
  Serial.println("Adelante");
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, HIGH);
  delay(50);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, HIGH);
}

void Atras() {
  Serial.println("Atras");
  digitalWrite(B1A, HIGH);
  digitalWrite(B2A, LOW);
  delay(50);
  digitalWrite(A1B, HIGH);
  digitalWrite(A1A, LOW);
}

void Derecha() {
  Serial.println("Derecha");
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, LOW);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, HIGH);
}

void Izquierda() {
  Serial.println("Izquierda");
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, HIGH);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, LOW);
}

void Detener() {
  Serial.println("Detenido");
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, LOW);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, LOW);
}


void handleRoot() {
  server.send(200, "text/html", 
    "<!DOCTYPE html>"
    "<html lang='es'>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>Control de Auto</title>"
    "<style>"
    "body {"
    "  -webkit-user-select: none;" /* Para navegadores WebKit (Chrome, Safari) */
    "  -moz-user-select: none;"    /* Para Firefox */
    "  -ms-user-select: none;"     /* Para Internet Explorer/Edge */
    "  user-select: none;"         /* Estándar */
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
    "  margin: 15px;" /* Margen entre botones */
    "  padding: 10px;" /* Padding para el texto dentro del botón */
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
    "<button onmousedown=\"sendDirection('adelante')\" onmouseup=\"sendDirection('detener')\" ontouchstart=\"sendDirection('adelante')\" ontouchend=\"sendDirection('detener')\">Arriba</button>"
    "</div>"
    "<div class='row'>"
    "<button onmousedown=\"sendDirection('izquierda')\" onmouseup=\"sendDirection('detener')\" ontouchstart=\"sendDirection('izquierda')\" ontouchend=\"sendDirection('detener')\">Izquierda</button>"
    "<button id='stop-button' onmousedown=\"sendDirection('detener')\" ontouchstart=\"sendDirection('detener')\">STOP</button>"
    "<button onmousedown=\"sendDirection('derecha')\" onmouseup=\"sendDirection('detener')\" ontouchstart=\"sendDirection('derecha')\" ontouchend=\"sendDirection('detener')\">Derecha</button>"
    "</div>"
    "<div class='row'>"
    "<button onmousedown=\"sendDirection('atras')\" onmouseup=\"sendDirection('detener')\" ontouchstart=\"sendDirection('atras')\" ontouchend=\"sendDirection('detener')\">Abajo</button>"
    "</div>"
    "</div>"
    "<p id='status'></p>"
    "<p>Distancia: <span id='distance'>--</span> cm</p>"
    "<script>"
    "function sendDirection(direction) {"
    "  fetch('/' + direction).then(response => response.text()).then(data => {"
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
    server.send(200, "text/plain", "Obstáculo detectado. No se puede mover adelante.");
  }
}

void handleAtras() {
  Atras();
  server.send(200, "text/plain", "Moviendo atrás");
}

void handleDerecha() {p
  if (!obstacleDetected) {
    Derecha();
    server.send(200, "text/plain", "Girando a la derecha");
  } else {
    server.send(200, "text/plain", "Obstáculo detectado. No se puede girar a la derecha.");
  }
}

void handleIzquierda() {
  if (!obstacleDetected) {
    Izquierda();
    server.send(200, "text/plain", "Girando a la izquierda");
  } else {
    server.send(200, "text/plain", "Obstáculo detectado. No se puede girar a la izquierda.");
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
  // Asegurarse de que el TRIG esté LOW
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  // Enviar pulso de 10µs para iniciar la medición
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  // Leer la duración del pulso en el pin ECHO
  long duration = pulseIn(ECHO, HIGH, 30000); // Timeout de 30ms (~5m)

  // Calcular la distancia
  float distance = (duration / 2.0) * SPEED_OF_SOUND;

  // Verificar si la medición está fuera de rango
  if (duration == 0) {
    distance = MAX_DISTANCE;
  }

  // Verificar si hay un obstáculo
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
  
  // Inicializar Pines de Motores
  pinMode(A1A, OUTPUT);
  pinMode(A1B, OUTPUT);
  pinMode(B1A, OUTPUT);
  pinMode(B2A, OUTPUT);

  // Inicializar Pines del Sensor de Distancia
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // Asegurarse de que el TRIG esté LOW
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  // Configurar la ESP32 como Access Point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point iniciado");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP del servidor: ");
  Serial.println(IP);

  // Configurar las rutas del servidor
  server.on("/", handleRoot);
  server.on("/adelante", handleAdelante);
  server.on("/atras", handleAtras);
  server.on("/derecha", handleDerecha);
  server.on("/izquierda", handleIzquierda);
  server.on("/detener", handleDetener);
  server.on("/distance", handleDistance);

  // Manejar rutas no definidas
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });

  // Iniciar el servidor
  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  server.handleClient();
}