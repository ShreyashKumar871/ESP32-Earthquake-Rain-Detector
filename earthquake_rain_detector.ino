#include <Wire.h>
#include "I2Cdev.h"     // Required by MPU6050.h
#include <MPU6050.h>
#include <WiFi.h>
#include <WebServer.h>

#define BUZZER_PIN 25
#define GREEN_LED_PIN 26
#define RED_LED_PIN 27

// --- RAIN SENSOR (MH-RD) ---
#define RAIN_AO_PIN 35   // Analog Output - raw moisture reading (ADC1, safe alongside WiFi)
#define RAIN_DO_PIN 34   // Digital Output - threshold comparator output (ADC1, input-only pin)
#define BLUE_LED_PIN 4   // Blue LED - lit while rain is detected

// Most MH-RD / FC-37 style boards drive DO LOW when moisture crosses the
// threshold set by the onboard potentiometer, and HIGH when dry.
// If your board behaves the opposite way (LED on when dry), flip this to HIGH.
#define RAIN_DETECTED_LEVEL LOW

// --- SENSOR THRESHOLDS (FREE PLAY) ---
#define THRESH_X 1.5
#define THRESH_Y 0.4
#define THRESH_Z 1.2

// --- ACCESS POINT CONFIG ---
// The ESP32 now creates its OWN WiFi network instead of joining one.
// Connect a phone/laptop to this network, then browse to the IP printed
// on the Serial Monitor (usually 192.168.4.1).
const char* ap_ssid = "EarthquakeDetector";   // Name of the network the ESP32 creates
const char* ap_password = "quake1234";        // Must be at least 8 characters, or use "" for an open network

// Optional: force a specific IP/gateway/subnet for the AP.
// Comment this block out (and the WiFi.softAPConfig call in setup) to just
// use the ESP32's default AP address (192.168.4.1).
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

MPU6050 mpu;
WebServer server(80);

// Unified alarm state variables
bool isAlarmActive = false;
unsigned long alarmEndTime = 0; 

// Global variables so the web server can read them instantly
float accel_x = 0;
float accel_y = 0;
float accel_z = 0;

// Rain sensor state, also readable instantly by the web server
int rainAnalogValue = 0;    // Raw AO reading (0-4095)
bool isRaining = false;     // Derived from DO pin

// ==========================================
// 🎨 EARTHQUAKE SEISMOGRAPH DASHBOARD
// ==========================================
const char* dashboard_html PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Earthquake Detector</title>
  <style>
    body { font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: #0f172a; color: #f8fafc; margin: 0; padding: 30px 20px; display: flex; flex-direction: column; align-items: center; }
    .header { text-align: center; margin-bottom: 30px; }
    h1 { margin: 0 0 10px 0; font-size: 3rem; color: #e2e8f0; letter-spacing: 1px; }
    .status-badge { display: inline-block; padding: 6px 15px; border-radius: 20px; background: #1e293b; font-size: 1rem; margin: 5px; border: 1px solid #334155; }
    .status-badge span { color: #10b981; font-weight: bold; }
    
    .alarm-banner { width: 100%; max-width: 1200px; padding: 20px; border-radius: 15px; text-align: center; font-size: 3rem; font-weight: bold; letter-spacing: 3px; transition: all 0.2s ease; box-sizing: border-box; text-transform: uppercase; margin-bottom: 30px; }
    .safe { background-color: #059669; color: white; box-shadow: 0 0 20px rgba(5, 150, 105, 0.3); }
    .danger { background-color: #dc2626; color: white; animation: pulse 0.4s infinite alternate; box-shadow: 0 0 50px rgba(220, 38, 38, 0.8); }
    @keyframes pulse { 0% { transform: scale(1); } 100% { transform: scale(1.03); } }

    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 25px; width: 100%; max-width: 1200px; }
    .card { background: #1e293b; border-radius: 15px; padding: 20px; text-align: center; box-shadow: 0 8px 20px rgba(0,0,0,0.5); }
    .card h3 { margin: 0 0 15px 0; color: #94a3b8; font-size: 1.2rem; text-transform: uppercase; letter-spacing: 2px; }
    
    /* Seismograph Canvas Style */
    canvas { width: 100%; height: 140px; background: #0b1121; border-radius: 8px; border: 1px solid #334155; display: block; margin: 0 auto; }
    .value { font-size: 1.1rem; color: #64748b; margin-top: 10px; font-family: monospace; }

    /* Rain card */
    .rain-icon { font-size: 3rem; margin-top: 10px; }

    .btn { margin-top: 40px; background: #3b82f6; color: white; border: none; padding: 18px 40px; font-size: 1.5rem; border-radius: 12px; cursor: pointer; font-weight: bold; transition: all 0.2s; box-shadow: 0 8px 15px rgba(0,0,0,0.3); }
    .btn:hover { background: #2563eb; transform: translateY(-2px); }
    .btn:active { transform: translateY(2px); }
  </style>
</head>
<body>

  <div class="header">
    <h1>Earthquake Detector</h1>
    <div class="status-badge">WiFi: <span>Connected</span></div>
    <div class="status-badge">Sensor: <span>Active</span></div>
    <div class="status-badge">Rain: <span id="rain-status-badge">Dry</span></div>
  </div>

  <div id="alarm-banner" class="alarm-banner safe">
    ✅ SAFE
  </div>
  
  <div class="grid">
    <div class="card" style="border-top: 4px solid #38bdf8;">
      <h3>Left / Right Shift</h3>
      <canvas id="canvas-x" width="400" height="150"></canvas>
      <div class="value" id="val-x">0.00 g</div>
    </div>
    <div class="card" style="border-top: 4px solid #10b981;">
      <h3>Forward / Back Shift</h3>
      <canvas id="canvas-y" width="400" height="150"></canvas>
      <div class="value" id="val-y">0.00 g</div>
    </div>
    <div class="card" style="border-top: 4px solid #f43f5e;">
      <h3>Up / Down Shift</h3>
      <canvas id="canvas-z" width="400" height="150"></canvas>
      <div class="value" id="val-z">1.00 g</div>
    </div>
    <div class="card" style="border-top: 4px solid #0ea5e9;">
      <h3>Rain Sensor</h3>
      <div class="rain-icon" id="rain-icon">☀️</div>
      <div class="value" id="val-rain">Raw: 0</div>
    </div>
  </div>

  <button class="btn" onclick="triggerTest()">🚨 Trigger Test Earthquake</button>

  <script>
    const maxPoints = 50;
    const dataX = new Array(maxPoints).fill(0);
    const dataY = new Array(maxPoints).fill(0);
    const dataZ = new Array(maxPoints).fill(1);

    function drawGraph(canvasId, data, color, baseline) {
      const canvas = document.getElementById(canvasId);
      const ctx = canvas.getContext('2d');
      const w = canvas.width;
      const h = canvas.height;
      
      ctx.clearRect(0, 0, w, h);
      
      ctx.strokeStyle = '#334155';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(0, h/2);
      ctx.lineTo(w, h/2);
      ctx.stroke();

      ctx.strokeStyle = color;
      ctx.lineWidth = 3;
      ctx.lineJoin = 'round';
      ctx.beginPath();
      
      const step = w / (maxPoints - 1);
      for(let i = 0; i < maxPoints; i++) {
        let diff = data[i] - baseline;
        if(diff > 2.5) diff = 2.5; 
        if(diff < -2.5) diff = -2.5;
        
        const y = (h/2) - (diff * (h/2.5)); 
        const x = i * step;
        
        if(i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }

    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('val-x').innerText = "Raw: " + data.x.toFixed(2) + ' g';
          document.getElementById('val-y').innerText = "Raw: " + data.y.toFixed(2) + ' g';
          document.getElementById('val-z').innerText = "Raw: " + data.z.toFixed(2) + ' g';
          
          dataX.shift(); dataX.push(data.x);
          dataY.shift(); dataY.push(data.y);
          dataZ.shift(); dataZ.push(data.z);

          drawGraph('canvas-x', dataX, '#38bdf8', 0);
          drawGraph('canvas-y', dataY, '#10b981', 0);
          drawGraph('canvas-z', dataZ, '#f43f5e', 1);
          
          const banner = document.getElementById('alarm-banner');
          if (data.alarm) {
            banner.className = 'alarm-banner danger';
            banner.innerText = '⚠️ EARTHQUAKE ⚠️';
          } else {
            banner.className = 'alarm-banner safe';
            banner.innerText = '✅ SAFE';
          }

          document.getElementById('val-rain').innerText = "Raw: " + data.rainRaw;
          const rainBadge = document.getElementById('rain-status-badge');
          const rainIcon = document.getElementById('rain-icon');
          if (data.rain) {
            rainBadge.innerText = "Raining";
            rainBadge.style.color = '#38bdf8';
            rainIcon.innerText = '🌧️';
          } else {
            rainBadge.innerText = "Dry";
            rainBadge.style.color = '#10b981';
            rainIcon.innerText = '☀️';
          }
        })
        .catch(err => console.error("Error fetching data"));
    }
    
    function triggerTest() {
      fetch('/testalarm', { method: 'POST' });
    }

    setInterval(updateData, 100); 
  </script>

</body>
</html>
)rawliteral";

// ==========================================
// SERVER HANDLERS
// ==========================================

void handleRoot() {
  server.send(200, "text/html", dashboard_html);
}

void handleData() {
  String json = "{";
  json += "\"x\":" + String(accel_x, 2) + ",";
  json += "\"y\":" + String(accel_y, 2) + ",";
  json += "\"z\":" + String(accel_z, 2) + ",";
  json += "\"alarm\":" + String(isAlarmActive ? "true" : "false") + ",";
  json += "\"rain\":" + String(isRaining ? "true" : "false") + ",";
  json += "\"rainRaw\":" + String(rainAnalogValue);
  json += "}";
  server.send(200, "application/json", json);
}

void handleTestAlarm() {
  isAlarmActive = true;
  alarmEndTime = millis() + 5000; 
  server.send(200, "text/plain", "OK"); 
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, LOW);

  // GPIO34 is input-only on the ESP32 (no internal pull-up/down available),
  // which is fine here since the MH-RD board actively drives this line.
  pinMode(RAIN_DO_PIN, INPUT);
  // RAIN_AO_PIN needs no pinMode() call - analogRead() handles it.

  Wire.begin();
  mpu.initialize();

  // ----------------------------------------
  // START AS ACCESS POINT (own local network)
  // ----------------------------------------
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);   // remove this line to use the default 192.168.4.1
  WiFi.softAP(ap_ssid, ap_password);

  delay(500); // give the AP a moment to come up before reading its IP

  IPAddress myIP = WiFi.softAPIP();

  Serial.println();
  Serial.println("========================================");
  Serial.println("   Earthquake Detector - Access Point");
  Serial.println("========================================");
  Serial.print("Network Name (SSID): ");
  Serial.println(ap_ssid);
  Serial.print("Password:            ");
  Serial.println(ap_password);
  Serial.print("Dashboard IP:        ");
  Serial.println(myIP);
  Serial.println("----------------------------------------");
  Serial.println("Connect a phone/laptop to the network above,");
  Serial.print("then open http://");
  Serial.print(myIP);
  Serial.println("/ in a browser.");
  Serial.println("========================================");

  // Start WebServer
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);         
  server.on("/testalarm", HTTP_POST, handleTestAlarm);
  server.begin();
}

void loop() {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  // Read raw values
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Convert to g's and store in global variables for the web server to read
  accel_x = (float)ax / 16384.0;
  accel_y = (float)ay / 16384.0;
  accel_z = (float)az / 16384.0;

  // 1. CHECK FOR MOVEMENT
  bool isMoving = (abs(accel_x) > THRESH_X || abs(accel_y) > THRESH_Y || abs(accel_z) > THRESH_Z);

  if (isMoving) {
    isAlarmActive = true;
    alarmEndTime = millis() + 5000; // Restart the 5-second timer
  }

  // 2. CHECK IF TIMER HAS EXPIRED
  if (isAlarmActive && millis() > alarmEndTime) {
    isAlarmActive = false; 
  }

  // 3. HANDLE BUZZER AND LEDS 
  if (isAlarmActive) {
    digitalWrite(GREEN_LED_PIN, LOW);
    
    // Blink Red LED and beep Buzzer rapidly 
    static unsigned long alarmBlinkTime = 0;
    if (millis() - alarmBlinkTime > 100) {
      alarmBlinkTime = millis();
      bool state = !digitalRead(RED_LED_PIN);
      digitalWrite(RED_LED_PIN, state);
      digitalWrite(BUZZER_PIN, state); 
    }
  } else {
    // Normal state - Blinks softly and slowly (every 1000ms instead of 200ms)
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    
    static unsigned long normalBlinkTime = 0;
    if (millis() - normalBlinkTime > 1000) {
      normalBlinkTime = millis();
      digitalWrite(GREEN_LED_PIN, !digitalRead(GREEN_LED_PIN));
    }
  }

  // 4. RAIN SENSOR - read moisture level and drive the blue LED
  rainAnalogValue = analogRead(RAIN_AO_PIN);
  isRaining = (digitalRead(RAIN_DO_PIN) == RAIN_DETECTED_LEVEL);
  digitalWrite(BLUE_LED_PIN, isRaining ? HIGH : LOW);

  server.handleClient();
  delay(10); 
}