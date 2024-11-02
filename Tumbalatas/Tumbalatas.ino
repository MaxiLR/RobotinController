#include "driver/mcpwm.h"
#include <WebServer.h>
#include <WiFi.h>

// ----- Credenciales de WiFi -----
const char *ssid = "RobotinMS";
const char *password = "12345678";

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
const float SPEED_OF_SOUND = 0.0343; // cm/µs
const int MAX_DISTANCE = 200;        // Distancia máxima a detectar en cm
const int OBSTACLE_THRESHOLD = 20;   // Umbral para detener el automóvil en cm

// ----- Parámetros de Control -----
bool obstacleDetected = false;

WebServer server(80);

void Adelante() {
  Serial.println("Adelante");
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, HIGH);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, HIGH);
}

void Atras() {
  Serial.println("Atras");
  digitalWrite(B1A, HIGH);
  digitalWrite(B2A, LOW);
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
      "#distance {"
      "  font-size: 48px;"
      "  font-weight: bold;"
      "  margin-top: 20px;"
      "}"
      "</style>"
      "</head>"
      "<body>"
      "<h1>Control de Auto</h1>"
      "<div class='container'>"
      "<p id='status'></p>"
      "<p>Distancia: <span id='distance'>--</span> cm</p>"
      "</div>"
      "<script>"
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