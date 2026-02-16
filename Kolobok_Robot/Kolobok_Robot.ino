#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// --- НАСТРОЙКИ ---
const char* ssid = "Kolobok_Bot";
const char* password = "12345678";

const int ENA_PIN = 14; 
const int IN1_PIN = 27; 
const int IN2_PIN = 26; 
const int SERVO_PIN = 13;
const int LED_PIN = 16;

// --- СОСТОЯНИЕ ---
unsigned long lastCommandTime = 0;
const unsigned long SAFE_TIMEOUT = 60000; // Уменьшил до 3 сек для надежности
bool firstCommandReceived = false;
bool manualSos = false;
bool safeModeActive = false;

// --- АКСЕЛЕРОМЕТР ---
const int ADDR = 0x18;
float smoothX = 0;
int displayAngle = 0;

WebServer server(80);
Servo myServo;

// --- ЛОГИКА SOS ---
void handleSOSPattern() {
  static unsigned long lastUpdate = 0;
  static int step = 0;
  const int timings[] = {200,200, 200,200, 200,200, 600,200, 600,200, 600,200, 200,200, 200,200, 200,1000};
  
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

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
  <title>KOLOBOK CONTROL</title>
  <style>
    body { font-family: sans-serif; background-color: #1a1a1a; color: white; text-align: center; margin: 0; display: flex; flex-direction: column; align-items: center; height: 100vh; justify-content: space-between; padding: 40px 0; box-sizing: border-box;}
    h2 { color: #00e676; margin: 0; font-size: 22px; text-transform: uppercase; letter-spacing: 1px;}
    #angle-display { font-size: 34px; font-weight: bold; text-shadow: 0 0 10px rgba(0,0,0,0.5); }
    #joystick-container { width: 220px; height: 220px; border: 3px solid #00e676; border-radius: 50%; position: relative; touch-action: none; background: radial-gradient(circle, rgba(0,230,118,0.05) 0%, rgba(0,0,0,0) 70%);}
    #joystick-stick { width: 70px; height: 70px; background: #00e676; border-radius: 50%; position: absolute; left: 50%; top: 50%; margin: -35px 0 0 -35px; box-shadow: 0 0 20px #00e676; cursor: pointer;}
    .sos-panel { background: #2d2d2d; padding: 12px 25px; border-radius: 40px; display: flex; align-items: center; gap: 20px; box-shadow: 0 4px 10px rgba(0,0,0,0.3);}
    .sos-panel span { font-weight: bold; font-size: 16px; letter-spacing: 1px;}
    .switch { position: relative; width: 60px; height: 32px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #444; border-radius: 32px; transition: .3s; }
    .slider:before { position: absolute; content: ""; height: 24px; width: 24px; left: 4px; bottom: 4px; background-color: white; border-radius: 50%; transition: .3s; }
    input:checked + .slider { background-color: #ff3d00; }
    input:checked + .slider:before { transform: translateX(28px); }
    #status { font-size: 11px; color: #555; text-transform: uppercase; letter-spacing: 2px; }
  </style>
</head><body>
  <h2>Управление Колобком</h2>
  <div id="angle-display">Угол: 0&deg;</div>
  <div id="joystick-container"><div id="joystick-stick"></div></div>
  <div class="sos-panel">
    <span>РЕЖИМ SOS</span>
    <label class="switch"><input type="checkbox" id="sos-btn"><span class="slider"></span></label>
  </div>
  <div id="status">СВЯЗЬ...</div>
<script>
  let container = document.getElementById("joystick-container"), stick = document.getElementById("joystick-stick");
  let angleText = document.getElementById("angle-display"), statusText = document.getElementById("status"), sosBtn = document.getElementById("sos-btn");
  let isDragging = false, maxR = 110, lastSend = 0;

  // ОПРОС УГЛА
  setInterval(() => {
    fetch('/getAngle').then(r => r.text()).then(angle => {
      let a = parseInt(angle);
      angleText.innerHTML = "Угол: " + a + "&deg;";
      angleText.style.color = Math.abs(a) < 25 ? "#00e676" : (Math.abs(a) < 55 ? "#ffeb3b" : "#ff3d00");
      statusText.innerText = "КОМАНДЫ ДОХОДЯТ";
      statusText.style.color = "#555";
    }).catch(() => {
      statusText.innerText = "СВЯЗЬ ПОТЕРЯНА";
      statusText.style.color = "#ff3d00";
    });
  }, 150);

  sosBtn.onchange = () => fetch("/action?sos=" + (sosBtn.checked ? 1 : 0));

  function sendAction(s, t, force = false) {
    let now = Date.now();
    if (force || now - lastSend > 60) {
      fetch(`/action?speed=${s}&turn=${t}`);
      lastSend = now;
    }
  }

  function move(e) {
    if(!isDragging || sosBtn.checked) return;
    let rect = container.getBoundingClientRect();
    let x = (e.touches ? e.touches[0].clientX : e.clientX) - rect.left - 110;
    let y = (e.touches ? e.touches[0].clientY : e.clientY) - rect.top - 110;
    let dist = Math.sqrt(x*x + y*y);
    if(dist > maxR) { x *= maxR/dist; y *= maxR/dist; }
    stick.style.transform = `translate(${x}px, ${y}px)`;
    sendAction(Math.round(y/-maxR*255), Math.round(x/maxR*45+90));
  }

  container.onmousedown = container.ontouchstart = (e) => { isDragging = true; move(e); };
  window.onmousemove = window.ontouchmove = move;
  window.onmouseup = window.ontouchend = () => { 
    if(!isDragging) return;
    isDragging = false; 
    stick.style.transform = "translate(0,0)"; 
    sendAction(0, 90, true); // Мгновенный принудительный стоп
  };
</script></body></html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  pinMode(ENA_PIN, OUTPUT); pinMode(IN1_PIN, OUTPUT); pinMode(IN2_PIN, OUTPUT); pinMode(LED_PIN, OUTPUT);
  myServo.attach(SERVO_PIN); myServo.write(90);

  Wire.begin();
  Wire.beginTransmission(ADDR); Wire.write(0x20); Wire.write(0x57); Wire.endTransmission();

  WiFi.softAP(ssid, password);
  server.on("/", []() { server.send(200, "text/html", index_html); });
  server.on("/getAngle", []() { server.send(200, "text/plain", String(displayAngle)); });
  server.on("/action", handleAction);
  server.begin();
}

void loop() {
  server.handleClient();
  updateSensorData();
  handleSOSPattern();

  // Авто-стоп при потере связи
  if (firstCommandReceived && !manualSos && (millis() - lastCommandTime > SAFE_TIMEOUT)) {
    if (!safeModeActive) {
      safeModeActive = true;
      stopRobot();
      Serial.println("Connection Lost - Stopped");
    }
  }
}

void updateSensorData() {
  Wire.beginTransmission(ADDR); Wire.write(0x2A | 0x80); Wire.endTransmission(false);
  Wire.requestFrom(ADDR, 2);
  if (Wire.available() >= 2) {
    int16_t rawX = (int16_t)((Wire.read()) | (Wire.read() << 8));
    smoothX = (smoothX * 0.8) + (rawX * 0.2);
    displayAngle = constrain(map((int)smoothX, -16000, 16000, -90, 90), -90, 90);
  }
}

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

    // ГАРАНТИРОВАННЫЙ СТОП
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

void stopRobot() {
  analogWrite(ENA_PIN, 0); 
  digitalWrite(IN1_PIN, LOW); 
  digitalWrite(IN2_PIN, LOW);
  myServo.write(90);
}