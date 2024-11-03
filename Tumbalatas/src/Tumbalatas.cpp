#include "driver/mcpwm.h"
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <WebSocketsServer.h> // Incluir la biblioteca WebSocketsServer
#include <WiFi.h>

// ----- Credenciales de WiFi -----
const char *ssid = "WIFI-QM";      // Network SSID
const char *password = "159Retys"; // Network Password

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
const int MAX_DISTANCE = 200;        // Max distance to detect in cm
const int OBSTACLE_THRESHOLD = 60;   // Threshold to stop the vehicle in cm
#define frecuencia 10000

// ----- Pin del Sensor Infrarrojo -----
#define INFRARED 32
const int COLOR_THRESHOLD = 50;

// ----- Pines del Encoder -----
#define LEFT_PI 13
#define RIGHT_PI 23

// ----- Estados del Tumbalatas -----
enum States { ESCANEANDO, AVANZANDO, RETROCEDIENDO, DETENIDO, VERIFICANDO };

// ----- Inicializacion de Variables Globales -----
float distance = OBSTACLE_THRESHOLD + 1;
bool color = false;
int retroCounter = 0;
States state = States::DETENIDO;

// ----- Variables para Encoders -----
volatile unsigned long leftPulseCount = 0;
volatile unsigned long rightPulseCount = 0;

// ----- Variables para Velocidad -----
float leftRPM = 0.0;
float rightRPM = 0.0;

// ----- Configuración del Tiempo para Cálculo de Velocidad -----
const unsigned long speedInterval = 1000;
unsigned long lastSpeedTime = 0;

// ----- Configuración del Encoder -----
const int pulsesPerRevolution = 100;
const int countsPerRevolution = pulsesPerRevolution;

// ----- Declaracion de Objetos -----
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ----- Declaracion de Funciones -----
void Adelante(float left_speed, float right_speed);
void Atras(float left_speed, float right_speed);
void Derecha(float left_speed, float right_speed);
void Izquierda(float left_speed, float right_speed);
void Detener();
void handleRoot();
void handleIniciar();
void handleDetener();
void handleDistance();
void handleColor();
float getDistance();
void stateMachineTask(void *parameter);
void webServerTask(void *parameter);
void readingsTask(void *parameter);
void controlMotorAvanzar(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num,
                         float duty_cycle);
void controlMotorRetroceder(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num,
                            float duty_cycle);
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
                      size_t length);

// ----- Rutinas de Interrupción -----
void IRAM_ATTR handleLeftEncoder() { leftPulseCount++; }

void IRAM_ATTR handleRightEncoder() { rightPulseCount++; }

void setup() {
  Serial.begin(115200);

  // Conectar a Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Configuración OTA
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch"; // Actualizar sketch
    } else {
      type = "filesystem"; // Actualizar SPIFFS
    }
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() { Serial.println("End update"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin(); // Inicializar OTA

  // Configurar pines de motores
  pinMode(A1A, OUTPUT);
  pinMode(A1B, OUTPUT);
  pinMode(B1A, OUTPUT);
  pinMode(B2A, OUTPUT);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(INFRARED, INPUT);

  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  // Inicializar MCPWM para los motores
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, B1A);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, B2A);

  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, A1B);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1B, A1A);

  mcpwm_config_t pwm_config0;
  pwm_config0.frequency = frecuencia;
  pwm_config0.cmpr_a = 0;
  pwm_config0.cmpr_b = 0;
  pwm_config0.counter_mode = MCPWM_UP_COUNTER;
  pwm_config0.duty_mode = MCPWM_DUTY_MODE_0;
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config0);
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config0);

  controlMotorAvanzar(MCPWM_UNIT_0, MCPWM_TIMER_1, 0.0);
  controlMotorAvanzar(MCPWM_UNIT_0, MCPWM_TIMER_0, 0.0);

  // Configurar rutas del servidor web
  server.on("/", handleRoot);
  server.on("/iniciar", handleIniciar);
  server.on("/detener", handleDetener);

  server.onNotFound([]() { server.send(404, "text/plain", "404: Not Found"); });

  server.begin(); // Iniciar el servidor
  Serial.println("Server started");

  // Iniciar el servidor WebSocket
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("WebSocket server started on port 81");

  // Configurar pines de los encoders
  pinMode(LEFT_PI, INPUT_PULLUP);
  pinMode(RIGHT_PI, INPUT_PULLUP);

  // Adjuntar interrupciones
  attachInterrupt(digitalPinToInterrupt(LEFT_PI), handleLeftEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RIGHT_PI), handleRightEncoder, CHANGE);

  // Crear tareas
  xTaskCreate(webServerTask, "WebServerTask", 2048, NULL, 1, NULL);
  xTaskCreate(stateMachineTask, "StateMachineTask", 4096, NULL, 1, NULL);
  xTaskCreate(readingsTask, "ReadingsTask", 4096, NULL, 1, NULL);
}

void loop() {
  ArduinoOTA.handle();
  webSocket.loop();
}

void Adelante(float left_speed, float right_speed) {
  controlMotorAvanzar(MCPWM_UNIT_0, MCPWM_TIMER_1, left_speed);
  controlMotorAvanzar(MCPWM_UNIT_0, MCPWM_TIMER_0, right_speed);
}

void Atras(float left_speed, float right_speed) {
  controlMotorRetroceder(MCPWM_UNIT_0, MCPWM_TIMER_1, left_speed);
  controlMotorRetroceder(MCPWM_UNIT_0, MCPWM_TIMER_0, right_speed);
}

void Derecha(float left_speed, float right_speed) {
  controlMotorAvanzar(MCPWM_UNIT_0, MCPWM_TIMER_1, left_speed);
  controlMotorRetroceder(MCPWM_UNIT_0, MCPWM_TIMER_0, right_speed);
}

void Izquierda(float left_speed, float right_speed) {
  controlMotorRetroceder(MCPWM_UNIT_0, MCPWM_TIMER_1, left_speed);
  controlMotorAvanzar(MCPWM_UNIT_0, MCPWM_TIMER_0, right_speed);
}

void Detener() {
  controlMotorAvanzar(MCPWM_UNIT_0, MCPWM_TIMER_1, 0.0);
  controlMotorAvanzar(MCPWM_UNIT_0, MCPWM_TIMER_0, 0.0);
  controlMotorRetroceder(MCPWM_UNIT_0, MCPWM_TIMER_1, 0.0);
  controlMotorRetroceder(MCPWM_UNIT_0, MCPWM_TIMER_0, 0.0);
}

void controlMotorAvanzar(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num,
                         float duty_cycle) {
  mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_A, 0.0);
  mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
  mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_B, duty_cycle);
  mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
}

void controlMotorRetroceder(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num,
                            float duty_cycle) {
  mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_A, duty_cycle);
  mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
  mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_B, 0.0);
  mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
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
      "  background-color: #219838;"
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
      "#distance, #color, #leftRPM, #rightRPM {"
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
      "  flex-wrap: wrap;"
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
      "    <div class='info-item'>"
      "      <p>Velocidad Izquierda:</p>"
      "      <span id='leftRPM'>--</span> <span>RPM</span>"
      "    </div>"
      "    <div class='info-item'>"
      "      <p>Velocidad Derecha:</p>"
      "      <span id='rightRPM'>--</span> <span>RPM</span>"
      "    </div>"
      "  </div>"
      "</div>"
      "<script>"
      "let socket = new WebSocket('ws://' + window.location.hostname + ':81/');"
      "socket.onopen = function(event) {"
      "  console.log('WebSocket conectado');"
      "};"
      "socket.onmessage = function(event) {"
      "  let data = JSON.parse(event.data);"
      "  if (data.distance !== undefined) {"
      "    document.getElementById('distance').innerText = data.distance;"
      "  }"
      "  if (data.color !== undefined) {"
      "    document.getElementById('color').innerText = data.color ? 'Blanco' "
      ": 'Negro';"
      "  }"
      "  if (data.status !== undefined) {"
      "    switch (data.status) {"
      "      case 0:"
      "        document.getElementById('status').innerText = 'ESCANEANDO';"
      "        break;"
      "      case 1:"
      "        document.getElementById('status').innerText = 'AVANZANDO';"
      "        break;"
      "      case 2:"
      "        document.getElementById('status').innerText = 'RETROCEDIENDO';"
      "        break;"
      "      case 3:"
      "        document.getElementById('status').innerText = 'DETENIDO';"
      "        break;"
      "      case 4:"
      "        document.getElementById('status').innerText = 'VERIFICANDO';"
      "        break;"
      "    }"
      "  }"
      "  if (data.leftRPM !== undefined) {"
      "    document.getElementById('leftRPM').innerText = "
      "data.leftRPM.toFixed(2);"
      "  }"
      "  if (data.rightRPM !== undefined) {"
      "    document.getElementById('rightRPM').innerText = "
      "data.rightRPM.toFixed(2);"
      "  }"
      "};"
      "socket.onclose = function(event) {"
      "  console.log('WebSocket desconectado');"
      "};"
      "socket.onerror = function(error) {"
      "  console.error('WebSocket Error: ', error);"
      "};"
      "function iniciar() {"
      "  fetch('/iniciar').then(response => response.text()).then(data => {"
      "  }).catch(error => {"
      "    console.error('Error:', error);"
      "  });"
      "} "
      "function detener() {"
      "  fetch('/detener').then(response => response.text()).then(data => {"
      "  }).catch(error => {"
      "    console.error('Error:', error);"
      "  });"
      "} "
      "</script>"
      "</body>"
      "</html>");
}

void handleDistance() { server.send(200, "text/plain", String(getDistance())); }

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

void webServerTask(void *parameter) {
  while (true) {
    server.handleClient();
    delay(100);
  }
}

void readingsTask(void *parameter) {
  while (true) {
    distance = getDistance();
    color = analogRead(INFRARED) <= COLOR_THRESHOLD ? 1 : 0;

    noInterrupts();
    unsigned long leftPulses = leftPulseCount;
    unsigned long rightPulses = rightPulseCount;
    leftPulseCount = 0;
    rightPulseCount = 0;
    interrupts();

    leftRPM =
        (leftPulses / (float)countsPerRevolution) * (60000.0 / speedInterval);
    rightRPM =
        (rightPulses / (float)countsPerRevolution) * (60000.0 / speedInterval);

    Serial.printf("Left RPM: %.2f, Right RPM: %.2f\n", leftRPM, rightRPM);

    String json = "{";
    json += "\"distance\":" + String(distance) + ",";
    json += "\"color\":" + String(color ? "true" : "false") + ",";
    json += "\"status\":" + String(state) + ",";
    json += "\"leftRPM\":" + String(leftRPM, 2) + ",";
    json += "\"rightRPM\":" + String(rightRPM, 2);
    json += "}";

    webSocket.broadcastTXT(json);

    delay(speedInterval);
  }
}

void stateMachineTask(void *parameter) {
  while (true) {
    switch (state) {
    case States::ESCANEANDO:
      if (distance <= OBSTACLE_THRESHOLD) {
        state = VERIFICANDO;
      } else {
        Derecha(98.0, 98.0);
      }
      break;
    case States::AVANZANDO:
      if (color) {
        state = RETROCEDIENDO;
      } else {
        Adelante(98.0, 98.0);
      }
      break;
    case States::RETROCEDIENDO:
      Atras(98.0, 98.0);
      delay(1000);
      retroCounter++;
      if (retroCounter == 2) {
        state = ESCANEANDO;
        retroCounter = 0;
      }
      break;
    case States::VERIFICANDO:
      Detener();
      delay(500);
      if (distance <= OBSTACLE_THRESHOLD) {
        state = AVANZANDO;
      } else {
        state = ESCANEANDO;
      }
      break;
    case States::DETENIDO:
      Detener();
      break;
    default:
      break;
    }

    delay(100);
  }
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
                      size_t length) {
  switch (type) {
  case WStype_DISCONNECTED:
    Serial.printf("WebSocket client #%u disconnected\n", num);
    break;
  case WStype_CONNECTED: {
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("WebSocket client #%u connected from %d.%d.%d.%d\n", num,
                  ip[0], ip[1], ip[2], ip[3]);
    break;
  }
  case WStype_TEXT:
    Serial.printf("WebSocket message from #%u: %s\n", num, payload);
    break;
  }
}