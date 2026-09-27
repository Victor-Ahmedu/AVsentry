/*
  AVsentry — AI-Based Smart Room Safety & Environmental Monitoring System
  ------------------------------------------------------------------------
  TinyML / Edge AI project. A decision tree (trained offline in Python,
  see /training) is compiled to C via micromlgen and embedded directly in
  model.h. All inference runs on-device on the NodeMCU ESP8266 — no cloud,
  no external ML runtime.

  WiFi is used ONLY for an optional local dashboard (served by the
  NodeMCU itself, viewable at http://avsentry.local on the same network).
  The core sensor -> predict -> actuate loop runs independently of WiFi
  and continues to work even if the network is unavailable.

  Hardware:
    DHT11   -> D4   (temperature / humidity)
    IR      -> D7   (0 = detected, 1 = clear)
    Buzzer  -> D1   (passive, uses tone()/noTone())
    Green LED -> D0
    Red LED   -> D5
    Servo   -> D2   (signal), powered from 3.3V rail (not Vin)
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DHT.h>
#include <Servo.h>
#include "model.h"

#define DHTPIN D4
#define DHTTYPE DHT11
#define IRPIN D7
#define BUZZPIN D1
#define GREENLED D0
#define REDLED D5
#define SERVOPIN D2

// ---- WiFi (dashboard only — core system works even if this fails) ----
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

DHT dht(DHTPIN, DHTTYPE);
Servo myServo;
Eloquent::ML::Port::DecisionTree clf;
ESP8266WebServer server(80);

bool wifiConnected = false;

const char* classNames[] = {
  "discomfort_hot",
  "discomfort_humid",
  "intrusion",
  "normal",
  "occupied_uncomfortable"
};

// Latest readings, shared with the web dashboard
float g_temp = 0, g_hum = 0;
int g_ir = 1;
String g_label = "normal";
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 2000;

// ---- Dashboard HTML page (served once, updates itself via JS polling) ----
const char PAGE_HTML[] PROGMEM = R"====(
<!DOCTYPE html><html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AVsentry Dashboard</title>
<style>
  body{margin:0;background:#0b1220;color:#e7edf7;font-family:Segoe UI,Arial,sans-serif;padding:24px}
  h1{color:#2dd4bf;margin:0 0 4px;font-size:26px}
  .sub{color:#93a3bf;margin-bottom:24px;font-size:13px}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:14px;max-width:700px}
  .card{background:#141e33;border:1px solid #233047;border-radius:14px;padding:16px;text-align:center}
  .card .val{font-size:28px;font-weight:700;color:#38bdf8}
  .card .lbl{color:#93a3bf;font-size:12px;margin-top:4px}
  .status{margin-top:20px;padding:18px;border-radius:14px;text-align:center;font-size:20px;font-weight:700;max-width:700px}
  .n{background:rgba(45,212,191,.15);color:#2dd4bf}
  .w{background:rgba(249,115,22,.15);color:#f97316}
  .d{background:rgba(244,63,94,.15);color:#f43f5e}
  .updated{color:#93a3bf;font-size:11px;margin-top:10px}
</style></head>
<body>
  <h1>AVsentry</h1>
  <div class="sub">Live sensor dashboard &middot; local network</div>
  <div class="grid">
    <div class="card"><div class="val" id="t">--</div><div class="lbl">Temp (&deg;C)</div></div>
    <div class="card"><div class="val" id="h">--</div><div class="lbl">Humidity (%)</div></div>
    <div class="card"><div class="val" id="i">--</div><div class="lbl">IR State</div></div>
  </div>
  <div class="status" id="status">--</div>
  <div class="updated" id="upd"></div>
<script>
async function poll(){
  try{
    const r = await fetch('/data');
    const d = await r.json();
    document.getElementById('t').textContent = d.temp.toFixed(1);
    document.getElementById('h').textContent = d.hum.toFixed(1);
    document.getElementById('i').textContent = d.ir==0 ? 'Detected' : 'Clear';
    const s = document.getElementById('status');
    s.textContent = d.label.replace(/_/g,' ').toUpperCase();
    s.className = 'status ' + (d.label==='normal' ? 'n' : d.label==='intrusion' ? 'd' : 'w');
    document.getElementById('upd').textContent = 'Updated ' + new Date().toLocaleTimeString();
  }catch(e){ document.getElementById('status').textContent = 'Connection lost...'; }
}
poll();
setInterval(poll, 1500);
</script>
</body></html>
)====";

void handleRoot() {
  server.send_P(200, "text/html", PAGE_HTML);
}

void handleData() {
  String json = "{";
  json += "\"temp\":" + String(g_temp, 1) + ",";
  json += "\"hum\":" + String(g_hum, 1) + ",";
  json += "\"ir\":" + String(g_ir) + ",";
  json += "\"label\":\"" + g_label + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  myServo.attach(SERVOPIN);
  pinMode(IRPIN, INPUT);
  pinMode(BUZZPIN, OUTPUT);
  pinMode(GREENLED, OUTPUT);
  pinMode(REDLED, OUTPUT);
  myServo.write(90);

  // Try WiFi briefly — but never block the core safety system on it
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi for dashboard");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(400);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nDashboard online at: http://" + WiFi.localIP().toString());
    if (MDNS.begin("avsentry")) {
      Serial.println("Also reachable at: http://avsentry.local");
    }
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.begin();
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi unavailable — running standalone (dashboard disabled).");
  }
}

void loop() {
  if (wifiConnected) {
    server.handleClient();
    MDNS.update();
  }

  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int ir = digitalRead(IRPIN);

    if (isnan(temp) || isnan(hum)) return;

    float features[] = {temp, hum, (float)ir};
    int predictedIndex = clf.predict(features);
    String predictedLabel = classNames[predictedIndex];

    g_temp = temp; g_hum = hum; g_ir = ir; g_label = predictedLabel;

    Serial.print("temp="); Serial.print(temp);
    Serial.print(" hum="); Serial.print(hum);
    Serial.print(" ir="); Serial.print(ir);
    Serial.print(" -> "); Serial.println(predictedLabel);

    if (predictedLabel == "intrusion") {
      digitalWrite(GREENLED, LOW); digitalWrite(REDLED, HIGH);
      tone(BUZZPIN, 1000); myServo.write(0);
    } else if (predictedLabel == "discomfort_hot" || predictedLabel == "discomfort_humid" || predictedLabel == "occupied_uncomfortable") {
      digitalWrite(GREENLED, LOW); digitalWrite(REDLED, HIGH);
      noTone(BUZZPIN); myServo.write(90);
    } else {
      digitalWrite(GREENLED, HIGH); digitalWrite(REDLED, LOW);
      noTone(BUZZPIN); myServo.write(90);
    }
  }
}
