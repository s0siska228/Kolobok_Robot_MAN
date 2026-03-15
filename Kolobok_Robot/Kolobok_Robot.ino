#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <MPU6050_light.h>
#include <ESP32Servo.h>

const char* ssid = "Kolobok_Bot";
const char* password = "12345678";

const int ENA_PIN = 14;
const int IN1_PIN = 27;
const int IN2_PIN = 26;
const int SERVO_PIN = 13;
const int LED_PIN = 16;

unsigned long lastCommandTime = 0;
const unsigned long SAFE_TIMEOUT = 60000;
bool firstCommandReceived = false;
bool manualSos = false;
bool safeModeActive = false;

extern const char index_html[] PROGMEM;

WebServer server(80);
Servo myServo;
MPU6050 mpu(Wire);

void setup() {
  Serial.begin(115200);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  myServo.attach(SERVO_PIN);
  myServo.write(90);

  Wire.begin();
  byte status = mpu.begin();
  Serial.print(F("MPU6050 статус: "));
  Serial.println(status);
  delay(1000);                   // !
  mpu.calcOffsets(true, false);  // гиро и не акселерометр

  WiFi.softAP(ssid, password);
  server.on("/", []() {
    server.send(200, "text/html", index_html);
  }); 
  server.on("/getAllData", []() {
    lastCommandTime = millis(); 
    safeModeActive = false;
    String angle = String(mpu.getAngleX()) + "," + String(mpu.getAngleY()) + "," + String(mpu.getTemp());
    server.send(200, "text/plain", angle);
  });
  server.on("/action", handleAction);
  server.begin();
}

void loop() {
  mpu.update();
  server.handleClient();
  handleSOSPattern();
  connectionCheck();
}

// Логика SOS-сигнала
void handleSOSPattern() {
  static unsigned long lastUpdate = 0;
  static int step = 0;
  const int timings[] = { 200, 200, 200, 200, 200, 200, 600, 200, 600, 200, 600, 200, 200, 200, 200, 200, 200, 1000 };  // . . . _ _ _ . . .

  if (manualSos || safeModeActive) {
    if (millis() - lastUpdate > timings[step]) {
      lastUpdate = millis();
      step++;
      if (step >= 18) step = 0;
      digitalWrite(LED_PIN, (step % 2 == 0));
    }
  } else {
    digitalWrite(LED_PIN, LOW);
    step = 0;
  }
}

// Авто-стоп при потере связи
void connectionCheck() {
  if (firstCommandReceived && !manualSos && (millis() - lastCommandTime > SAFE_TIMEOUT)) {
    if (!safeModeActive) {
      safeModeActive = true;
      stopRobot();
      Serial.println("Соединение потеряно");
    }
  }
}

// Логика взаимодействия с сервером
void handleAction() {
  if (server.hasArg("sos")) {
    manualSos = (server.arg("sos") == "1");
    if (manualSos) stopRobot();
  }

  if (server.hasArg("speed") && !manualSos) {
    firstCommandReceived = true;
    lastCommandTime = millis();
    safeModeActive = false;

    int s = server.arg("speed").toInt();
    int t = server.arg("turn").toInt();

    // Стоп при отпускании джойстика
    if (abs(s) < 15) {
      stopRobot();
    } else {
      digitalWrite(IN1_PIN, s > 0);
      digitalWrite(IN2_PIN, s < 0);
      analogWrite(ENA_PIN, abs(s));
      myServo.write(constrain(t, 45, 135));
    }
  }
  server.send(200, "text/plain", "OK");
}

// Стоп
void stopRobot() {
  analogWrite(ENA_PIN, 0);
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  myServo.write(90);
}