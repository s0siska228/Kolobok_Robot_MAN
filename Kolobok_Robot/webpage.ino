const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
  <title>KOLOBOK CONTROL</title>
  <style>
    body { font-family: sans-serif; background-color: #1a1a1a; color: white; text-align: center; margin: 0; display: flex; flex-direction: column; align-items: center; height: 100vh; justify-content: space-between; padding: 40px 0; box-sizing: border-box;}
    h2 { color: #00e676; margin: 0; font-size: 22px; text-transform: uppercase; letter-spacing: 1px;}
    .telemetry {
        margin: 10px 0;
    }
    #angle-x-display, #angle-y-display, #temp-display {
        font-size: 22px; /* Чуть меньше, чтобы влезло три строки */
        font-weight: bold;
        line-height: 1.2;
    }
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
  <div class="telemetry">
    <div id="angle-x-display">Угол X: 0&deg;</div>
    <div id="angle-y-display">Угол Y: 0&deg;</div>
    <div id="temp-display">Темп: 0&deg;C</div>
  </div>
  <div id="joystick-container"><div id="joystick-stick"></div></div>
  <div class="sos-panel">
    <span>РЕЖИМ SOS</span>
    <label class="switch"><input type="checkbox" id="sos-btn"><span class="slider"></span></label>
  </div>
  <div id="status">СВЯЗЬ...</div>
<script>
  let container = document.getElementById("joystick-container")
  let stick = document.getElementById("joystick-stick");
  let statusText = document.getElementById("status") 
  let sosBtn = document.getElementById("sos-btn");
  let angleXText = document.getElementById("angle-x-display");
  let angleYText = document.getElementById("angle-y-display");
  let TempText = document.getElementById("temp-display");
  let isDragging = false, maxR = 110, lastSend = 0;

  // ОПРОСЫ
  setInterval(() => {
      fetch('/getAllData').then(r => r.text()).then(data => {

      let parts = data.split(',');
      let x = parseInt(parts[0]);
      let y = parseInt(parts[1]);
      let temp = parseInt(parts[2]);

      angleXText.innerHTML = "Угол X: " + x + "&deg;";
      angleXText.style.color = Math.abs(x) < 25 ? "#00e676" : "#ff3d00";

      angleYText.innerHTML = "Угол Y: " + y + "&deg;";
      angleYText.style.color = Math.abs(y) < 25 ? "#00e676" : "#ff3d00";

      TempText.innerHTML = "Температура: " + temp + "&deg;";
      TempText.style.color = temp > 50 ? "#ff3d00" : "#00e676";
      
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