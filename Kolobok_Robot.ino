#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// --- НАСТРОЙКИ WI-FI ---
const char* ssid = "Kolobok_Bot";      // Имя сети
const char* password = "12345678";     // Пароль

// --- ПИНЫ (Настройка GPIO) ---
// Мотор (L298N)
const int ENA_PIN = 14; // Скорость (ШИМ)
const int IN1_PIN = 27; // Направление A
const int IN2_PIN = 26; // Направление B

// Сервопривод (Маятник)
const int SERVO_PIN = 13;

// --- ОБЪЕКТЫ ---
WebServer server(80);
Servo myServo;

// --- ВЕБ-ИНТЕРФЕЙС (HTML + CSS + JS ДЖОЙСТИК) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
  <title>KOLOBOK JOYSTICK</title>
  <style>
    body { 
      font-family: Arial; 
      background-color: #222; 
      color: white; 
      text-align: center;
      overflow: hidden; /* Запрет прокрутки */
      user-select: none; /* Запрет выделения */
    }
    h2 { color: #04AA6D; margin-top: 10px; }
    
    /* Контейнер джойстика */
    #joystick-container {
      position: relative;
      width: 200px;
      height: 200px;
      margin: 50px auto;
      background: rgba(255, 255, 255, 0.1);
      border: 2px solid #04AA6D;
      border-radius: 50%;
      touch-action: none; /* Важно для мобильных! */
    }
    
    /* Сам рычажок */
    #joystick-stick {
      position: absolute;
      left: 50%;
      top: 50%;
      width: 60px;
      height: 60px;
      margin-left: -30px;
      margin-top: -30px;
      background: #04AA6D;
      border-radius: 50%;
      box-shadow: 0 0 10px rgba(0,0,0,0.5);
    }
    
    #status { font-size: 18px; margin-top: 20px; color: #bbb; }
  </style>
</head>
<body>
  <h2>KOLOBOK CONTROL</h2>
  <div id="status">STOP</div>
  
  <div id="joystick-container">
    <div id="joystick-stick"></div>
  </div>

  <script>
    var container = document.getElementById("joystick-container");
    var stick = document.getElementById("joystick-stick");
    var statusText = document.getElementById("status");
    
    var isDragging = false;
    var maxRadius = 70; // Макс. отклонение стика
    var containerRect = container.getBoundingClientRect();
    var centerX = containerRect.width / 2;
    var centerY = containerRect.height / 2;
    
    // Переменные для ограничения частоты отправки (чтобы не спамить ESP32)
    var lastSendTime = 0;

    // --- ОБРАБОТЧИКИ СОБЫТИЙ (TOUCH & MOUSE) ---
    container.addEventListener("touchstart", startDrag);
    container.addEventListener("touchmove", moveDrag);
    container.addEventListener("touchend", endDrag);
    
    container.addEventListener("mousedown", startDrag);
    document.addEventListener("mousemove", moveDrag);
    document.addEventListener("mouseup", endDrag);

    function startDrag(e) {
      isDragging = true;
      moveDrag(e);
    }

    function moveDrag(e) {
      if (!isDragging) return;
      e.preventDefault(); // Блокируем скролл страницы

      // Получаем координаты пальца/мыши
      var clientX, clientY;
      if (e.touches) {
        clientX = e.touches[0].clientX;
        clientY = e.touches[0].clientY;
      } else {
        clientX = e.clientX;
        clientY = e.clientY;
      }

      // Пересчитываем координаты относительно центра круга
      var rect = container.getBoundingClientRect();
      var x = clientX - rect.left - centerX;
      var y = clientY - rect.top - centerY;

      // Ограничиваем движение кругом (maxRadius)
      var distance = Math.sqrt(x*x + y*y);
      if (distance > maxRadius) {
        var angle = Math.atan2(y, x);
        x = Math.cos(angle) * maxRadius;
        y = Math.sin(angle) * maxRadius;
      }

      // Двигаем стик визуально
      stick.style.transform = `translate(${x}px, ${y}px)`;

      // --- КОНВЕРТАЦИЯ В ДАННЫЕ ДЛЯ РОБОТА ---
      // Y (вверх-вниз) -> Скорость (-255 до 255)
      // Инвертируем Y, так как в браузере Y=0 сверху
      var speed = Math.round((y / maxRadius) * -255);
      
      // X (влево-вправо) -> Угол Серво (45 до 135)
      // Центр (90) + отклонение
      var turn = Math.round(90 + (x / maxRadius) * 45); 

      statusText.innerText = "Speed: " + speed + " | Turn: " + turn;
      sendData(speed, turn);
    }

    function endDrag() {
      isDragging = false;
      // Возвращаем стик в центр
      stick.style.transform = `translate(0px, 0px)`;
      statusText.innerText = "STOP";
      // Отправляем команду остановки
      sendData(0, 90);
    }

    function sendData(speed, turn) {
      // Ограничиваем частоту запросов (не чаще 1 раза в 50мс)
      var now = Date.now();
      if (now - lastSendTime > 50 || speed === 0) {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/action?speed=" + speed + "&turn=" + turn, true);
        xhr.send();
        lastSendTime = now;
      }
    }
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  // Настройка пинов
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  myServo.attach(SERVO_PIN);
  myServo.write(90); // Маятник прямо

  // Запуск Wi-Fi
  WiFi.softAP(ssid, password);
  Serial.println("Wi-Fi Started");
  Serial.println(WiFi.softAPIP());

  // Маршруты сервера
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/action", HTTP_GET, handleAction);

  server.begin();
}

void loop() {
  server.handleClient();
}

void handleAction() {
  if (server.hasArg("speed") && server.hasArg("turn")) {
    int speedVal = server.arg("speed").toInt();
    int turnVal = server.arg("turn").toInt();
    
    controlMotor(speedVal);
    controlServo(turnVal);
    
    server.send(200, "text/plain", "OK");
  }
}

void controlMotor(int speed) {
  // Мертвая зона для слабых значений
  if (abs(speed) < 50) speed = 0;

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
  analogWrite(ENA_PIN, abs(speed));
}

void controlServo(int angle) {
  // Ограничиваем угол, чтобы не сломать механизм
  angle = constrain(angle, 45, 135); 
  myServo.write(angle);
}
