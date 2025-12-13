#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// --- НАСТРОЙКИ WI-FI ---
const char* ssid = "Kolobok_Bot";      // Имя сети
const char* password = "12345678";     // Пароль (минимум 8 символов)

// --- ПИНЫ (Настройка GPIO) ---
// Мотор (L298N)
const int ENA_PIN = 14; // Скорость
const int IN1_PIN = 27; // Направление 1
const int IN2_PIN = 26; // Направление 2

// Сервопривод (Маятник)
const int SERVO_PIN = 13;

// --- ОБЪЕКТЫ ---
WebServer server(80);
Servo myServo;

// Переменные состояния
int motorSpeed = 0; // -255 ... 255
int servoAngle = 90; // 90 - центр, 0 - лево, 180 - право

// --- HTML СТРАНИЦА (Интерфейс управления) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>KOLOBOK CONTROL</title>
  <style>
    body { font-family: Arial; text-align: center; margin:0px auto; padding-top: 30px; background-color: #222; color: white; }
    .slider { -webkit-appearance: none; width: 80%; height: 25px; background: #d3d3d3; outline: none; opacity: 0.7; transition: .2s; border-radius: 10px; }
    .slider:hover { opacity: 1; }
    .slider::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #04AA6D; cursor: pointer; border-radius: 50%; }
    h2 { color: #04AA6D; }
    .data { font-size: 20px; margin-bottom: 10px; }
  </style>
</head>
<body>
  <h2>ROBOT KOLOBOK</h2>
  
  <p class="data">SPEED: <span id="speedVal">0</span></p>
  <input type="range" min="-255" max="255" value="0" class="slider" id="speedRange" oninput="sendData()">
  
  <br><br><br>
  
  <p class="data">TURN: <span id="turnVal">90</span></p>
  <input type="range" min="45" max="135" value="90" class="slider" id="turnRange" oninput="sendData()">

  <script>
    function sendData() {
      var speed = document.getElementById("speedRange").value;
      var turn = document.getElementById("turnRange").value;
      
      document.getElementById("speedVal").innerHTML = speed;
      document.getElementById("turnVal").innerHTML = turn;

      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/action?speed=" + speed + "&turn=" + turn, true);
      xhr.send();
    }
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  // Настройка пинов мотора
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  // Настройка сервопривода
  myServo.attach(SERVO_PIN);
  myServo.write(90); // Ставим маятник ровно

  // Запуск Wi-Fi точки доступа
  WiFi.softAP(ssid, password);
  Serial.println("Wi-Fi Access Point started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Маршрутизация сервера
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/action", HTTP_GET, handleAction);

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient(); // Обработка входящих команд
}

// Функция обработки команд от слайдеров
void handleAction() {
  if (server.hasArg("speed") && server.hasArg("turn")) {
    int speedVal = server.arg("speed").toInt();
    int turnVal = server.arg("turn").toInt();

    controlMotor(speedVal);
    controlServo(turnVal);
    
    server.send(200, "text/plain", "OK");
  }
}

// Логика управления мотором
void controlMotor(int speed) {
  // Мертвая зона (чтобы мотор не гудел при малых значениях)
  if (abs(speed) < 40) speed = 0;

  if (speed > 0) {
    digitalWrite(IN1_PIN, HIGH);
    digitalWrite(IN2_PIN, LOW);
  } else if (speed < 0) {
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, HIGH);
  } else {
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);
  }
  
  analogWrite(ENA_PIN, abs(speed)); // Управление мощностью (ШИМ)
}

// Логика управления сервоприводом
void controlServo(int angle) {
  // Ограничиваем углы, чтобы маятник не бился о стенки
  angle = constrain(angle, 45, 135); 
  myServo.write(angle);
}