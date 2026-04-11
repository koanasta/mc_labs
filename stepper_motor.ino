#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AccelStepper.h>

const char* ssid     = "Kobryn";
const char* password = "password123";

#define IN1 5   
#define IN2 4   
#define IN3 14  
#define IN4 13  

AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);
const float STEPS_PER_DEGREE = 2048.0 / 360.0;

ESP8266WebServer server(80);

void disableCoils() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

const char HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Motor Control</title>
<style>
  * { box-sizing:border-box; margin:0; padding:0; }
  body { background:#0d1117; color:#e6edf3; font-family:'Segoe UI',Arial,sans-serif; display:flex; flex-direction:column; align-items:center; padding:24px 16px; min-height:100vh; }
  h1 { color:#4fc3f7; margin-bottom:6px; font-size:1.6rem; }
  .sub { color:#8b949e; font-size:.85rem; margin-bottom:24px; }
  .card { background:#161b22; border-radius:12px; padding:20px; width:100%; max-width:400px; margin-bottom:16px; }
  .card h2 { color:#8b949e; font-size:.8rem; text-transform:uppercase; letter-spacing:.1em; margin-bottom:16px; }
  label { font-size:.9rem; color:#8b949e; display:block; margin-bottom:6px; }
  input[type=number] { width:100%; padding:12px; border-radius:8px; border:1px solid #30363d; background:#0d1117; color:#e6edf3; font-size:1.1rem; text-align:center; margin-bottom:16px; }
  .presets { display:grid; grid-template-columns:repeat(3,1fr); gap:8px; margin-bottom:16px; }
  .preset { background:#21262d; border:1px solid #30363d; color:#e6edf3; padding:10px 6px; border-radius:8px; font-size:.85rem; cursor:pointer; text-align:center; }
  .preset:hover { border-color:#4fc3f7; }
  .btns { display:grid; grid-template-columns:1fr 1fr; gap:10px; }
  button { padding:14px; border-radius:8px; border:none; font-size:1rem; cursor:pointer; font-weight:600; }
  .btn-cw { background:#4fc3f7; color:#000; }
  .btn-ccw { background:#21262d; color:#e6edf3; border:1px solid #4fc3f7; }
  .btn-stop { width:100%; background:#f85149; color:#fff; margin-top:10px; }
  #status { background:#161b22; border-radius:8px; padding:14px 16px; width:100%; max-width:400px; border-left:3px solid #4fc3f7; font-size:.9rem; color:#8b949e; }
  .moving { color:#4fc3f7 !important; }
  .done { color:#3fb950 !important; }
  .error { color:#f85149 !important; }
</style>
</head>
<body>
<h1>Motor Control</h1>
<p class="sub">ESP8266 28BYJ-48 ULN2003</p>
<div class="card">
  <h2>Degrees</h2>
  <label>Enter degrees (1-360)</label>
  <input type="number" id="degrees" value="90" min="1" max="360">
  <h2 style="margin-bottom:10px">Presets</h2>
  <div class="presets">
    <div class="preset" onclick="setDeg(45)">45</div>
    <div class="preset" onclick="setDeg(90)">90</div>
    <div class="preset" onclick="setDeg(180)">180</div>
    <div class="preset" onclick="setDeg(270)">270</div>
    <div class="preset" onclick="setDeg(360)">360</div>
    <div class="preset" onclick="setDeg(1)">1</div>
  </div>
  <div class="btns">
    <button class="btn-cw" onclick="move('cw')">CW</button>
    <button class="btn-ccw" onclick="move('ccw')">CCW</button>
  </div>
  <button class="btn-stop" onclick="stop()">STOP</button>
</div>
<div id="status">Ready</div>
<script>
function setDeg(d) { document.getElementById('degrees').value = d; }
function getDeg() {
  const d = parseInt(document.getElementById('degrees').value);
  if (isNaN(d) || d < 1 || d > 360) { setStatus('Enter 1 to 360', 'error'); return null; }
  return d;
}
function setStatus(msg, type) {
  const el = document.getElementById('status');
  el.textContent = msg;
  el.className = type || '';
}
function move(dir) {
  const deg = getDeg();
  if (!deg) return;
  setStatus('Moving ' + (dir==='cw'?'CW':'CCW') + ' ' + deg + ' deg', 'moving');
  fetch('/move?dir=' + dir + '&deg=' + deg)
    .then(r => r.text()).then(t => setStatus(t, 'done'))
    .catch(() => setStatus('Error', 'error'));
}
function stop() {
  fetch('/stop').then(r => r.text()).then(t => setStatus(t, 'done'))
    .catch(() => setStatus('Error', 'error'));
}
setInterval(() => {
  fetch('/status').then(r => r.text()).then(t => {
    if (t === '0') {
      const el = document.getElementById('status');
      if (el.classList.contains('moving')) setStatus('Done', 'done');
    }
  }).catch(() => {});
}, 1000);
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  disableCoils();

  stepper.setMaxSpeed(500);
  stepper.setAcceleration(200);

  WiFi.softAP(ssid, password);
  Serial.println("\nAP started!");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() {
    server.send_P(200, "text/html", HTML);
  });

  server.on("/move", []() {
    if (!server.hasArg("dir") || !server.hasArg("deg")) {
      server.send(400, "text/plain", "Error");
      return;
    }
    String dir = server.arg("dir");
    int deg = server.arg("deg").toInt();
    if (deg < 1 || deg > 360) {
      server.send(400, "text/plain", "1-360 only");
      return;
    }
    long steps = (long)(deg * STEPS_PER_DEGREE);
    if (dir == "cw") stepper.move(steps);
    else stepper.move(-steps);
    server.send(200, "text/plain", "Moving " + String(deg) + " deg");
  });

  server.on("/stop", []() {
    stepper.stop();
    stepper.setCurrentPosition(stepper.currentPosition());
    disableCoils();
    server.send(200, "text/plain", "Stopped");
  });

  server.on("/status", []() {
    server.send(200, "text/plain", stepper.distanceToGo() != 0 ? "1" : "0");
  });

  server.begin();
  Serial.println("Server started!");
}

void loop() {
  server.handleClient();
  if (stepper.distanceToGo() != 0) {
    stepper.run();
  } else {
    disableCoils();
  }
}