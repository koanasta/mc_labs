#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid     = "611VVA";
const char* password = "123qwerty9";

ESP8266WebServer server(80);

const int btnPin  = 15;
const int leds[]  = {D4, D3, D7};
const int numLeds = 3;

int  currentLed   = 0;
bool reverseOrder = false;

volatile unsigned long intervalMs = 300;
const unsigned long INTERVAL_MIN  = 50;
const unsigned long INTERVAL_MAX  = 300;
const unsigned long SPEED_STEP    = 50;
const unsigned long HOLD_TICK     = 100;

unsigned long lastUpdate = 0;

const unsigned long DEBOUNCE_MS = 50;

volatile bool          btnPressed = false;
volatile unsigned long btnIsrTime = 0;

bool          btnHeld      = false;
unsigned long holdStart    = 0;
unsigned long lastHoldTick = 0;

IRAM_ATTR void btnISR() {
  unsigned long now = millis();
  if (now - btnIsrTime > DEBOUNCE_MS) {
    btnIsrTime = now;
    btnPressed = true;
  }
}

void ledWrite(int pin, bool state) {
 digitalWrite(pin, state);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>LED Controller</title>
  <style>
    body {
      background: #fff0f5; color: #7a2a4a;
      font-family: 'DM Sans', sans-serif;
      display: flex; flex-direction: column;
      align-items: center; justify-content: center;
      min-height: 100vh; gap: 2rem;
    }
    h1 { font-size: 1.3rem; font-weight: 500; color: #c0436e; }
    .info { font-size: 0.95rem; color: #b05070; }
    .info span { font-weight: 600; color: #c0436e; }
    button {
      padding: 0.65rem 2rem; font-size: 0.95rem;
      background: #e8306a; color: #fff; border: none;
      border-radius: 24px; cursor: pointer;
    }
    button:hover { background: #c0436e; }
  </style>
</head>
<body>
  <h1>LED Control</h1>
  <div class="info">Direction: <span id="dir">→</span></div>
  <div class="info">Speed interval: <span id="spd">300</span> ms</div>
  <button onclick="pressButton()">Change direction</button>
<script>
  function pressButton() {
    fetch('/button').then(() => setTimeout(updateStatus, 100));
  }
  function updateStatus() {
    fetch('/status').then(r => r.json()).then(d => {
      document.getElementById('dir').textContent = d.reverse ? '←' : '→';
      document.getElementById('spd').textContent = d.interval;
    }).catch(() => {});
  }
  setInterval(updateStatus, 350);
  updateStatus();
</script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleButton() {
  reverseOrder = !reverseOrder;
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  String json = "{\"currentLed\":" + String(currentLed) +
                ",\"reverse\":" + (reverseOrder ? "true" : "false") +
                ",\"interval\":" + String(intervalMs) + "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(74880);

  for (int i = 0; i < numLeds; i++) {
    pinMode(leds[i], OUTPUT);
    ledWrite(leds[i], LOW);
  }
  ledWrite(leds[0], HIGH);

  pinMode(btnPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(btnPin), btnISR, RISING);

  server.on("/",       handleRoot);
  server.on("/button", handleButton);
  server.on("/status", handleStatus);
  server.begin();

  WiFi.begin(ssid, password);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) delay(100);
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("Connected! IP: " + WiFi.localIP().toString());
}

void loop() {
  server.handleClient();
  handlePhysicalButton();
  updateLeds();
}

void handlePhysicalButton() {
  unsigned long now = millis();
  bool currentState = digitalRead(btnPin);

  if (btnPressed) {
    btnPressed   = false;
    reverseOrder = !reverseOrder;
    btnHeld      = true;
    holdStart    = now;
    lastHoldTick = now;
  }

  if (btnHeld) {
    if (currentState == LOW) {
      btnHeld    = false;
      intervalMs = INTERVAL_MAX;
    } else {
      if (now - lastHoldTick >= HOLD_TICK) {
        lastHoldTick = now;
        if (intervalMs > INTERVAL_MIN)
          intervalMs = max((unsigned long)INTERVAL_MIN, intervalMs - SPEED_STEP);
      }
    }
  }
}

void updateLeds() {
  if (millis() - lastUpdate >= intervalMs) {
    lastUpdate = millis();
    ledWrite(leds[currentLed], LOW);
    if (!reverseOrder)
      currentLed = (currentLed + 1) % numLeds;
    else
      currentLed = (currentLed - 1 + numLeds) % numLeds;
    ledWrite(leds[currentLed], HIGH);
  }
}