#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AccelStepper.h>

const char* ssid     = "Kobryn";
const char* password = "password123";

// Мотор 1 — горизонтальний
#define IN1_A 5   // D1
#define IN2_A 4   // D2
#define IN3_A 14  // D5
#define IN4_A 13  // D7

// Мотор 2 — вертикальний
#define IN1_B 0   // D3
#define IN2_B 2   // D4
#define IN3_B 3   // RX
#define IN4_B 1   // TX

AccelStepper stepper1(AccelStepper::HALF4WIRE, IN1_A, IN3_A, IN2_A, IN4_A);
AccelStepper stepper2(AccelStepper::HALF4WIRE, IN1_B, IN3_B, IN2_B, IN4_B);

const float STEPS_PER_DEGREE = 2048.0 / 360.0;
ESP8266WebServer server(80);

void disableCoils1() {
  digitalWrite(IN1_A, LOW); digitalWrite(IN2_A, LOW);
  digitalWrite(IN3_A, LOW); digitalWrite(IN4_A, LOW);
}
void disableCoils2() {
  digitalWrite(IN1_B, LOW); digitalWrite(IN2_B, LOW);
  digitalWrite(IN3_B, LOW); digitalWrite(IN4_B, LOW);
}

// ✅ НОРМАЛЬНА СТИЛІЗОВАНА ВЕБКА
const char HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Robo Conductor</title>
<style>
body {
  background:#0d1117;
  color:#e6edf3;
  font-family:Arial;
  text-align:center;
  padding:20px;
}
.card {
  background:#161b22;
  border-radius:12px;
  padding:15px;
  margin:10px auto;
  max-width:320px;
}
button {
  padding:10px;
  margin:5px;
  border:none;
  border-radius:8px;
  cursor:pointer;
}
.cw { background:#4fc3f7; }
.ccw { background:#c084fc; }
.stop { background:#f85149; color:#fff; }
input {
  padding:8px;
  width:80px;
  text-align:center;
}
</style>
</head>

<body>
<h2>Robo Conductor</h2>

<div class="card">
<h3>Motor 1</h3>
<input id="deg1" type="number" value="90">
<br>
<button class="cw" onclick="move(1,'cw')">CW</button>
<button class="ccw" onclick="move(1,'ccw')">CCW</button>
</div>

<div class="card">
<h3>Motor 2</h3>
<input id="deg2" type="number" value="45">
<br>
<button class="cw" onclick="move(2,'cw')">Up</button>
<button class="ccw" onclick="move(2,'ccw')">Down</button>
</div>

<div class="card">
<h3>Both</h3>
<button onclick="moveSync('cw','cw')">Both CW</button>
<button onclick="moveSync('cw','ccw')">Opposite</button>
</div>

<button class="stop" onclick="stopAll()">STOP</button>

<p id="status">Ready</p>

<script>
function move(m, d){
  let deg = document.getElementById('deg'+m).value;
  fetch(`/move?motor=${m}&dir=${d}&deg=${deg}`);
}

function moveSync(d1,d2){
  let deg1 = document.getElementById('deg1').value;
  let deg2 = document.getElementById('deg2').value;

  fetch(`/move?motor=1&dir=${d1}&deg=${deg1}`)
    .then(()=>setTimeout(()=>{
      fetch(`/move?motor=2&dir=${d2}&deg=${deg2}`)
    },150));
}

function stopAll(){
  fetch('/stop');
}
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  pinMode(IN1_A, OUTPUT); pinMode(IN2_A, OUTPUT);
  pinMode(IN3_A, OUTPUT); pinMode(IN4_A, OUTPUT);
  pinMode(IN1_B, OUTPUT); pinMode(IN2_B, OUTPUT);
  pinMode(IN3_B, OUTPUT); pinMode(IN4_B, OUTPUT);

  disableCoils1();
  disableCoils2();

  // 🔻 ЗМЕНШЕНА ШВИДКІСТЬ (важливо для 2 моторів від ESP)
  stepper1.setMaxSpeed(250);
  stepper1.setAcceleration(100);

  stepper2.setMaxSpeed(250);
  stepper2.setAcceleration(100);

  WiFi.softAP(ssid, password);

  server.on("/", []() {
    server.send_P(200, "text/html", HTML);
  });

  server.on("/move", []() {
    int motor = server.arg("motor").toInt();
    String dir = server.arg("dir");
    int deg = server.arg("deg").toInt();

    long steps = deg * STEPS_PER_DEGREE;

    if (motor == 1) {
      stepper1.move(dir == "cw" ? steps : -steps);
    } else {
      stepper2.move(dir == "cw" ? steps : -steps);
    }

    server.send(200, "text/plain", "OK");
  });

  server.on("/stop", []() {
    stepper1.stop();
    stepper2.stop();
    disableCoils1();
    disableCoils2();
    server.send(200, "text/plain", "Stopped");
  });

  server.begin();
}

void loop() {
  server.handleClient();

  if (stepper1.distanceToGo() != 0) stepper1.run();
  else disableCoils1();

  if (stepper2.distanceToGo() != 0) stepper2.run();
  else disableCoils2();
}