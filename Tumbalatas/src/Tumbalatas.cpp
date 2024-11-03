#include "driver/mcpwm.h"
#include <ArduinoOTA.h>
#include <WebServer.h>
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

// ----- Pines del PID -----
#define LEFT_PI 13
#define RIGHT_PI 23

// ----- Estados del Tumbalatas -----
#define ESCANEANDO 0
#define AVANZANDO 1
#define RETROCEDIENDO 2
#define DETENIDO 3

// ----- Inicializacion de Variables Globales -----
float distance = OBSTACLE_THRESHOLD + 1;
bool color = false;
int state = DETENIDO;
int retroCounter = 0;

WebServer server(80); // Create a web server on port 80

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
void controlMotorAvanzar(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num,
                         float duty_cycle);
void controlMotorRetroceder(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num,
                            float duty_cycle);

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // OTA setup
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch"; // Update sketch
    } else {
      type = "filesystem"; // Update SPIFFS
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

  ArduinoOTA.begin(); // Initialize OTA

  // Set up motor pins
  pinMode(A1A, OUTPUT);
  pinMode(A1B, OUTPUT);
  pinMode(B1A, OUTPUT);
  pinMode(B2A, OUTPUT);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(RIGHT_PI, INPUT);
  pinMode(LEFT_PI, INPUT);

  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

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

  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/iniciar", handleIniciar);
  server.on("/detener", handleDetener);
  server.on("/distance", handleDistance);
  server.on("/color", handleColor);

  server.onNotFound([]() { server.send(404, "text/plain", "404: Not Found"); });

  server.begin(); // Start the server
  Serial.println("Server started");

  // Create tasks
  xTaskCreate(webServerTask, "WebServerTask", 2048, NULL, 1, NULL);
  xTaskCreate(stateMachineTask, "StateMachineTask", 2048, NULL, 1, NULL);
}

void loop() {
  ArduinoOTA.handle();   // Handle OTA requests
  server.handleClient(); // Handle incoming client requests
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
  controlMotorRetroceder(MCPWM_UNIT_0, MCPWM_TIMER_1, 0.0);
  controlMotorRetroceder(MCPWM_UNIT_0, MCPWM_TIMER_0, 0.0);
  controlMotorAvanzar(MCPWM_UNIT_0, MCPWM_TIMER_0, 0.0);
  controlMotorAvanzar(MCPWM_UNIT_0, MCPWM_TIMER_1, 0.0);
}

void controlMotorAvanzar(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num,
                         float duty_cycle) {
  mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_A, 0);
  mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
  mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_B, duty_cycle);
  mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
}

void controlMotorRetroceder(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num,
                            float duty_cycle) {
  mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_A, duty_cycle);
  mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
  mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_B, 0);
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
      "} "
      "function detener() {"
      "  fetch('/detener').then(response => response.text()).then(data => {"
      "    document.getElementById('status').innerText = data;"
      "  }).catch(error => {"
      "    document.getElementById('status').innerText = 'Error al detener.';"
      "    console.error('Error:', error);"
      "  });"
      "} "
      "function updateDistance() {"
      "  fetch('/distance').then(response => response.text()).then(data => {"
      "    document.getElementById('distance').innerText = data;"
      "  }).catch(error => {"
      "    document.getElementById('distance').innerText = '--';"
      "    console.error('Error:', error);"
      "  });"
      "} "
      "function updateColor() {"
      "  fetch('/color').then(response => response.text()).then(data => {"
      "    document.getElementById('color').innerText = data;"
      "  }).catch(error => {"
      "    document.getElementById('color').innerText = '--';"
      "    console.error('Error:', error);"
      "  });"
      "} "
      "setInterval(updateDistance, 1000);"
      "setInterval(updateColor, 1000);"
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
    delay(10); // Avoid high CPU usage
  }
}

void stateMachineTask(void *parameter) {
  while (true) {
    distance = getDistance();
    color = analogRead(INFRARED) <= COLOR_THRESHOLD ? 1 : 0;

    switch (state) {
    case ESCANEANDO:
      if (distance <= OBSTACLE_THRESHOLD) {
        state = AVANZANDO;
      } else {
        Izquierda(98.0, 98.0);
      }
      break;
    case AVANZANDO:
      if (!color) {
        Adelante(98.0, 98.0);
      } else {
        state = RETROCEDIENDO;
      }
      break;
    case RETROCEDIENDO:
      Atras(98.0, 98.0);
      delay(1000);
      retroCounter++;
      if (retroCounter == 2) {
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
}

// TODO: Make threads independant
// TODO: Scan sleep before moving forward