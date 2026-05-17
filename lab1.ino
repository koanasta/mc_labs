#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>


const bool LED_ON[] = {HIGH, HIGH, HIGH};
const uint8_t LED_PINS[] = {D1, D2, D5};
const uint8_t BUTTON_PIN = D8;
const uint8_t LED_COUNT = sizeof(LED_PINS) / sizeof(LED_PINS[0]);

const char *SSID_AP = "Kobryn";
const char *PASSWORD = "password123";

const unsigned long INTERVAL_NORMAL = 700;
const unsigned long INTERVAL_FAST = 100;
const unsigned long DEBOUNCE_DELAY = 50;


ESP8266WebServer server(80);

unsigned long previousMillis = 0;
uint8_t currentLed = 0;

bool webButtonActive = false;


volatile bool isrTriggered = false;   
volatile unsigned long isrTime = 0;      


bool buttonState = false;


const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1" charset="UTF-8">
  <title>Wemos Control</title>
  <style>
    body { font-family: sans-serif; text-align: center; margin-top: 50px; background-color: #f4f4f4;}
    button { padding: 30px 60px; font-size: 24px; border-radius: 12px; background-color: #E63946; color: white; border: none; cursor: pointer; user-select: none; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
    button:active { background-color: #D62828; transform: translateY(2px); box-shadow: 0 2px 4px rgba(0,0,0,0.2); }
  </style>
</head>
<body>
  <h2>Контроль швидкості LED</h2>
  <button id="btn">PRESS</button>
  <script>
    const btn = document.getElementById("btn");
    function sendState(state) { fetch("/set?state=" + state); }
    btn.addEventListener("mousedown", () => sendState(1));
    btn.addEventListener("mouseup",   () => sendState(0));
    btn.addEventListener("mouseleave",() => sendState(0));
    btn.addEventListener("touchstart", (e) => { e.preventDefault(); sendState(1); });
    btn.addEventListener("touchend",   (e) => { e.preventDefault(); sendState(0); });
  </script>
</body>
</html>
)=====";


void IRAM_ATTR buttonISR() {
  isrTriggered = true;
  isrTime = millis(); 
}


void handleRoot() {
  server.send(200, "text/html", MAIN_page);
}

void handleSet() {
  if (server.hasArg("state")) {
    webButtonActive = (server.arg("state") == "1");
    Serial.print("webButtonActive=");
    Serial.println(webButtonActive ? "true" : "false");
  }
  server.send(200, "text/plain", "OK");
}


void processButton() {

  if (!isrTriggered) return;

    Serial.println("triggered");

  if ((millis() - isrTime) < DEBOUNCE_DELAY) 
  {
      Serial.println("((millis() - isrTime) ");

    return;
  }

        Serial.println("AFTER debounce");


  buttonState  = (digitalRead(BUTTON_PIN) == HIGH);
  isrTriggered = false;
  Serial.print("buttonState=");
  Serial.println(buttonState ? "PRESSED" : "RELEASED");
}


void updateLeds() {
  bool fast = buttonState || webButtonActive;
  unsigned long interval = fast ? INTERVAL_FAST : INTERVAL_NORMAL;

  if ((millis() - previousMillis) < interval) return;

  previousMillis += interval;

  digitalWrite(LED_PINS[currentLed], !LED_ON[currentLed]);  
  currentLed = (currentLed + 1) % LED_COUNT;
  digitalWrite(LED_PINS[currentLed],  LED_ON[currentLed]);   
}


void setup() {
  Serial.begin(115200);

  for (int i = 0; i < LED_COUNT; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], !LED_ON[i]);
  }

  pinMode(BUTTON_PIN, INPUT);

  
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, CHANGE);

  WiFi.softAP(SSID_AP, PASSWORD);
  Serial.println("Ready");

  server.on("/",    handleRoot);
  server.on("/set", handleSet);
  server.begin();

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  Serial.println(digitalRead(BUTTON_PIN));
   processButton();
  server.handleClient();
   updateLeds();

}