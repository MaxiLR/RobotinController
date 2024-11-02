#include "driver/mcpwm.h"
#include <Arduino.h>
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
const float SPEED_OF_SOUND = 0.0343; // cm/µs
const int MAX_DISTANCE = 200;        // Distancia máxima a detectar en cm
const int OBSTACLE_THRESHOLD = 60;   // Umbral para detener el automóvil en cm

// ----- Pin del Sensor Infrarrojo -----
#define INFRARED 32
const int COLOR_THRESHOLD = 50;

// ----- Estados del Tumbalatas -----
#define ESCANEANDO 0
#define AVANZANDO 1
#define RETROCEDIENDO 2
#define DETENIDO 3

// ----- Inicializacion de Variables Globales -----
float distance = OBSTACLE_THRESHOLD + 1;
bool color = false;
int state = ESCANEANDO;
int retroCounter = 0;

WebServer server(80);

// ----- Declaracion de Funciones -----
void Adelante();
void Atras();
void Derecha();
void Izquierda();
void GirarEnElLugar();
void Detener();
void handleRoot();
void handleIniciar();
void handleDetener();
void handleDistance();
void handleColor();
float getDistance();

void Adelante() {
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, HIGH);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, HIGH);
}

void Atras() {
  digitalWrite(B1A, HIGH);
  digitalWrite(B2A, LOW);
  digitalWrite(A1B, HIGH);
  digitalWrite(A1A, LOW);
}

void Derecha() {
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, LOW);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, HIGH);
}

void Izquierda() {
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, HIGH);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, LOW);
}

void Detener() {
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, LOW);
  digitalWrite(A1B, LOW);
  digitalWrite(A1A, LOW);
}

void GirarEnElLugar() {
  digitalWrite(B1A, LOW);
  digitalWrite(B2A, HIGH);
  digitalWrite(A1B, HIGH);
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
      "  background-color: #f0f0f0;"
      "  margin: 0;"
      "  padding: 0;"
      "}"
      ".container {"
      "  display: flex;"
      "  flex-direction: column;"
      "  align-items: center;"
      "  justify-content: center;"
      "  height: 100vh;"
      "}"
      ".button-group {"
      "  display: flex;"
      "  gap: 20px;"
      "  margin-bottom: 30px;"
      "}"
      "#iniciar-button {"
      "  padding: 15px 30px;"
      "  font-size: 24px;"
      "  font-weight: bold;"
      "  color: #fff;"
      "  background-color: #28a745;"
      "  border: none;"
      "  border-radius: 5px;"
      "  cursor: pointer;"
      "  transition: background-color 0.3s;"
      "}"
      "#iniciar-button:hover {"
      "  background-color: #218838;"
      "}"
      "#iniciar-button:active {"
      "  background-color: #1e7e34;"
      "}"
      "#detener-button {"
      "  padding: 15px 30px;"
      "  font-size: 24px;"
      "  font-weight: bold;"
      "  color: #fff;"
      "  background-color: #dc3545;"
      "  border: none;"
      "  border-radius: 5px;"
      "  cursor: pointer;"
      "  transition: background-color 0.3s;"
      "}"
      "#detener-button:hover {"
      "  background-color: #c82333;"
      "}"
      "#detener-button:active {"
      "  background-color: #bd2130;"
      "}"
      "#distance, #color {"
      "  font-size: 48px;"
      "  font-weight: bold;"
      "  margin-top: 20px;"
      "  color: #333;"
      "}"
      "#status {"
      "  margin-top: 20px;"
      "  font-size: 18px;"
      "  color: #555;"
      "}"
      ".info-group {"
      "  display: flex;"
      "  gap: 50px;"
      "  margin-top: 20px;"
      "}"
      ".info-item {"
      "  display: flex;"
      "  flex-direction: column;"
      "  align-items: center;"
      "}"
      ".info-item span {"
      "  margin-top: 10px;"
      "}"
      "</style>"
      "</head>"
      "<body>"
      "<h1>Control de Auto</h1>"
      "<div class='container'>"
      "  <div class='button-group'>"
      "    <button id='iniciar-button' onclick='iniciar()'>Iniciar</button>"
      "    <button id='detener-button' onclick='detener()'>Detener</button>"
      "  </div>"
      "  <p id='status'></p>"
      "  <div class='info-group'>"
      "    <div class='info-item'>"
      "      <p>Distancia:</p>"
      "      <span id='distance'>--</span> <span>cm</span>"
      "    </div>"
      "    <div class='info-item'>"
      "      <p>Color:</p>"
      "      <span id='color'>--</span>"
      "    </div>"
      "  </div>"
      "</div>"
      "<script>"
      "function iniciar() {"
      "  fetch('/iniciar').then(response => response.text()).then(data => {"
      "    document.getElementById('status').innerText = data;"
      "  }).catch(error => {"
      "    document.getElementById('status').innerText = 'Error al iniciar.';"
      "    console.error('Error:', error);"
      "  });"
      "}"
      "function detener() {"
      "  fetch('/detener').then(response => response.text()).then(data => {"
      "    document.getElementById('status').innerText = data;"
      "  }).catch(error => {"
      "    document.getElementById('status').innerText = 'Error al detener.';"
      "    console.error('Error:', error);"
      "  });"
      "}"
      "function updateDistance() {"
      "  fetch('/distance').then(response => response.text()).then(data => {"
      "    document.getElementById('distance').innerText = data;"
      "  }).catch(error => {"
      "    document.getElementById('distance').innerText = '--';"
      "    console.error('Error:', error);"
      "  });"
      "}"
      "function updateColor() {"
      "  fetch('/color').then(response => response.text()).then(data => {"
      "    document.getElementById('color').innerText = data;"
      "  }).catch(error => {"
      "    document.getElementById('color').innerText = '--';"
      "    console.error('Error:', error);"
      "  });"
      "}"
      "setInterval(updateDistance, 1000);"
      "setInterval(updateColor, 1000);"
      "</script>"
      "</body>"
      "</html>");
}

void handleDistance() { server.send(200, "text/plain", String(distance)); }

void handleColor() {
  server.send(200, "text/plain", color ? "Blanco" : "Negro");
}

void handleIniciar() {
  state = ESCANEANDO;
  server.send(200, "text/plain", "Iniciado");
}

void handleDetener() {
  state = DETENIDO;
  server.send(200, "text/plain", "Detenido");
}

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
  server.on("/iniciar", handleIniciar);
  server.on("/detener", handleDetener);
  server.on("/distance", handleDistance);
  server.on("/color", handleColor);

  server.onNotFound([]() { server.send(404, "text/plain", "404: Not Found"); });

  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  // server.handleClient();

  distance = getDistance();
  color = analogRead(INFRARED) <= COLOR_THRESHOLD ? 1 : 0;

  Serial.print("Distancia: ");
  Serial.println(distance);
  Serial.print("Color: ");
  Serial.println(color);
  Serial.print("Estado: ");
  Serial.println(state);

  switch (state) {
  case ESCANEANDO:
    if (distance <= OBSTACLE_THRESHOLD) {
      state = AVANZANDO;
    } else {
      GirarEnElLugar();
    }
    break;
  case AVANZANDO:
    if (!color) {
      Adelante();
    } else {
      state = RETROCEDIENDO;
    }
    break;
  case RETROCEDIENDO:
    Atras();
    delay(1000);
    retroCounter++;
    if (retroCounter == 1) {
      state = ESCANEANDO;
      retroCounter = 0;
    }
    break;
  case DETENIDO:
    Detener();
    break;
  default:
    break;
  }

  delay(10);
}