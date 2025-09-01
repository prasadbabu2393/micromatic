
// ////----------------------  with ui 


// #include <PL_ADXL355.h>
// #include <HardwareSerial.h>
// #include <WiFi.h>
// #include <WebServer.h>
// #include <ArduinoJson.h>

// // UART Setup
// HardwareSerial modemSerial(2);    // UART2 (RX-16, TX-17) for GPS

// // Encoder pins for meter calculation
// const int ENCODER_A_PIN = 25;    // GPIO25 for encoder channel A
// const int ENCODER_B_PIN = 26;    // GPIO26 for encoder channel B

// // ADXL355 Setup
// PL::ADXL355 adxl355;
// uint8_t i2cAddress = 0x1D;
// auto range = PL::ADXL355_Range::range2g;

// // LVDT Setup
// const int LVDT_PIN = 34;
// const float VREF = 3.3;
// const int ADC_RESOLUTION = 4095;
// const float ZERO_VOLTAGE = 1.65;
// const float MIN_VOLTAGE = 0.82;
// const float MAX_VOLTAGE = 2.48;
// const float VOLTAGE_TO_MM = 75.0 / (MAX_VOLTAGE - MIN_VOLTAGE);
// const int NUM_SAMPLES = 1;

// // Encoder variables
// volatile int32_t encoderCount = 0;
// volatile bool encoderStateA = false;
// volatile bool encoderStateB = false;
// volatile bool prevStateA = false;
// volatile bool prevStateB = false;
// volatile unsigned long lastInterruptTime = 0;
// const unsigned long debounceDelay = 2; // 2ms debounce

// // Meter calculation constants
// const float PULSES_PER_METER = 1000.0;  // Adjust based on your encoder resolution
// float meterValue = 0.0;

// // Web Server Setup
// WebServer server(80);
// const char* ssid = "TracX_Pro";
// const char* password = "12345678";

// // Data Storage
// struct SensorData {
//   float displacement = 0.0;
//   float accel_x = 0.0;
//   float accel_y = 0.0;
//   float accel_z = 0.0;
//   float latitude = 0.0;
//   float longitude = 0.0;
//   float altitude = 0.0;
//   float speed = 0.0;
//   float course = 0.0;
//   String utc_time = "";
//   float meter_value = 0.0;
//   unsigned long last_update = 0;
// } currentData;

// // Interrupt service routine for encoder
// void IRAM_ATTR encoderISR() {
//   unsigned long currentTime = millis();
  
//   // Debouncing
//   if (currentTime - lastInterruptTime < debounceDelay) {
//     return;
//   }
//   lastInterruptTime = currentTime;
  
//   // Read current states
//   bool currentStateA = digitalRead(ENCODER_A_PIN);
//   bool currentStateB = digitalRead(ENCODER_B_PIN);
  
//   // Determine direction based on state changes
//   if ((prevStateA == LOW && prevStateB == LOW && currentStateA == HIGH) ||
//       (prevStateA == HIGH && prevStateB == HIGH && currentStateA == LOW)) {
//     // Clockwise
//     encoderCount++;
//   }
//   else if ((prevStateA == LOW && prevStateB == LOW && currentStateB == HIGH) ||
//            (prevStateA == HIGH && prevStateB == HIGH && currentStateB == LOW)) {
//     // Counterclockwise
//     encoderCount--;
//   }
  
//   // Update previous states
//   prevStateA = currentStateA;
//   prevStateB = currentStateB;
// }

// void setup() {
//   Serial.begin(115200);
  
//   // Initialize ADC
//   analogReadResolution(12);
//   analogSetAttenuation(ADC_11db);
  
//   // Initialize encoder pins
//   pinMode(ENCODER_A_PIN, INPUT_PULLUP);
//   pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  
//   // Attach interrupts for encoder
//   attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, CHANGE);
//   attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), encoderISR, CHANGE);
  
//   // Initialize GPS UART
//   modemSerial.begin(115200, SERIAL_8N1, 16, 17);
  
//   delay(3000);
//   Serial.println(F("TracX-Pro Enhanced System Starting..."));
  
//   // Initialize GPS
//   sendATCommand(F("AT+QGPS=1"), 2000);
//   sendATCommand(F("AT+QGPSCFG=\"gnssconfig\",1"), 1000);
  
//   // Initialize ADXL355
//   adxl355.beginI2C(i2cAddress);
//   adxl355.setRange(range);
//   adxl355.enableMeasurement();
  
//   // Initialize WiFi AP
//   setupWiFiAP();
  
//   // Setup web server routes
//   setupWebServer();
  
//   Serial.println(F("System initialized successfully!"));
//   Serial.println(F("Connect to WiFi: TracX_Pro (password: tracx2024)"));
//   Serial.println(F("Open browser: http://192.168.4.1"));
// }

// void loop() {
//   // Handle web server
//   server.handleClient();
  
//   // Read encoder and calculate meter value
//   calculateMeterValue();
  
//   // Read LVDT data
//   readLVDT();
  
//   // Read accelerometer data
//   readAccelerometer();
  
//   // Read GPS data
//   readGPSData();
  
//   // Update timestamp
//   currentData.last_update = millis();
  
//   // Print all data to serial
//   printAllData();
  
//   delay(30); // Faster update rate for better responsiveness
// }

// void setupWiFiAP() {
//   WiFi.softAP(ssid, password);
//   IPAddress IP = WiFi.softAPIP();
//   Serial.print("AP IP address: ");
//   Serial.println(IP);
// }

// void setupWebServer() {
//   // Serve main page
//   server.on("/", handleRoot);
  
//   // API endpoint for sensor data
//   server.on("/api/data", handleAPIData);
  
//   server.begin();
//   Serial.println("Web server started");
// }

// void handleRoot() {
//   String html = R"(
// <!DOCTYPE html>
// <html>
// <head>
//     <title>Rail Trolly</title>
//     <meta name="viewport" content="width=device-width, initial-scale=1">
//     <style>
//         * { margin: 0; padding: 0; box-sizing: border-box; }
        
//         body { 
//             font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
//             background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
//             color: white;
//             min-height: 100vh;
//             overflow-x: hidden;
//         }
        
//         .header {
//             text-align: center;
//             padding: 20px;
//             background: rgba(0,0,0,0.2);
//             backdrop-filter: blur(10px);
//             border-bottom: 1px solid rgba(255,255,255,0.1);
//         }
        
//         .header h1 {
//             font-size: 2.5em;
//             font-weight: 300;
//             margin-bottom: 5px;
//             letter-spacing: 2px;
//         }
        
//         .header p {
//             opacity: 0.8;
//             font-size: 1.1em;
//         }
        
//         .status-bar {
//             display: flex;
//             justify-content: center;
//             align-items: center;
//             padding: 10px;
//             background: rgba(0,0,0,0.1);
//         }
        
//         .status {
//             padding: 8px 20px;
//             border-radius: 25px;
//             font-weight: 600;
//             font-size: 0.9em;
//             display: flex;
//             align-items: center;
//             gap: 8px;
//             transition: all 0.3s ease;
//         }
        
//         .status.online { 
//             background: linear-gradient(45deg, #4CAF50, #45a049);
//             box-shadow: 0 4px 15px rgba(76, 175, 80, 0.3);
//         }
        
//         .status.offline { 
//             background: linear-gradient(45deg, #f44336, #d32f2f);
//             box-shadow: 0 4px 15px rgba(244, 67, 54, 0.3);
//         }
        
//         .pulse { animation: pulse 2s infinite; }
        
//         @keyframes pulse {
//             0% { opacity: 1; }
//             50% { opacity: 0.5; }
//             100% { opacity: 1; }
//         }
        
//         .dashboard {
//             display: grid;
//             grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
//             gap: 20px;
//             padding: 20px;
//             max-width: 1400px;
//             margin: 0 auto;
//         }
        
//         .card {
//             background: rgba(255,255,255,0.08);
//             border: 1px solid rgba(255,255,255,0.12);
//             border-radius: 20px;
//             padding: 25px;
//             backdrop-filter: blur(15px);
//             transition: all 0.3s ease;
//             position: relative;
//             overflow: hidden;
//         }
        
//         .card::before {
//             content: '';
//             position: absolute;
//             top: 0;
//             left: 0;
//             right: 0;
//             height: 3px;
//             background: linear-gradient(90deg, #ff6b6b, #4ecdc4, #45b7d1, #f39c12);
//             opacity: 0;
//             transition: opacity 0.3s ease;
//         }
        
//         .card:hover {
//             transform: translateY(-5px);
//             box-shadow: 0 20px 40px rgba(0,0,0,0.3);
//         }
        
//         .card:hover::before {
//             opacity: 1;
//         }
        
//         .card-header {
//             display: flex;
//             align-items: center;
//             gap: 10px;
//             margin-bottom: 20px;
//         }
        
//         .card-icon {
//             font-size: 1.8em;
//             opacity: 0.8;
//         }
        
//         .card-title {
//             font-size: 1.1em;
//             font-weight: 600;
//             opacity: 0.9;
//         }
        
//         .card-value {
//             font-size: 2.8em;
//             font-weight: 700;
//             color: #4ecdc4;
//             margin: 15px 0;
//             text-shadow: 0 2px 10px rgba(78, 205, 196, 0.3);
//         }
        
//         .card-unit {
//             font-size: 0.6em;
//             opacity: 0.7;
//             font-weight: 400;
//         }
        
//         .card-description {
//             font-size: 0.9em;
//             opacity: 0.7;
//             line-height: 1.4;
//         }
        
//         .meter-card {
//             grid-column: span 2;
//             background: linear-gradient(135deg, rgba(255, 107, 107, 0.15), rgba(255, 165, 0, 0.15));
//             border: 2px solid rgba(255, 107, 107, 0.3);
//         }
        
//         .meter-card .card-value {
//             font-size: 4em;
//             color: #ff6b6b;
//             animation: glow 3s ease-in-out infinite alternate;
//         }
        
//         @keyframes glow {
//             from { text-shadow: 0 0 20px rgba(255, 107, 107, 0.5); }
//             to { text-shadow: 0 0 30px rgba(255, 107, 107, 0.8), 0 0 40px rgba(255, 107, 107, 0.6); }
//         }
        
//         .multi-value {
//             display: grid;
//             grid-template-columns: 1fr 1fr 1fr;
//             gap: 15px;
//             margin: 15px 0;
//         }
        
//         .mini-value {
//             text-align: center;
//             padding: 10px;
//             background: rgba(255,255,255,0.05);
//             border-radius: 10px;
//         }
        
//         .mini-value-label {
//             font-size: 0.8em;
//             opacity: 0.7;
//             margin-bottom: 5px;
//         }
        
//         .mini-value-number {
//             font-size: 1.2em;
//             font-weight: 600;
//             color: #45b7d1;
//         }
        
//         .update-indicator {
//             position: fixed;
//             top: 20px;
//             right: 20px;
//             width: 12px;
//             height: 12px;
//             border-radius: 50%;
//             background: #4CAF50;
//             animation: blink 1s infinite;
//         }
        
//         @keyframes blink {
//             0%, 50% { opacity: 1; }
//             51%, 100% { opacity: 0.3; }
//         }
        
//         @media (max-width: 768px) {
//             .meter-card { grid-column: span 1; }
//             .card-value { font-size: 2.2em; }
//             .meter-card .card-value { font-size: 3em; }
//             .dashboard { grid-template-columns: 1fr; }
//         }
//     </style>
// </head>
// <body>
//     <div class="update-indicator" id="updateIndicator"></div>
    
//     <div class="header">
//         <h1>TRACX PRO</h1>
//         <p>Advanced Railway Monitoring & Analytics</p>
//     </div>
    
//     <div class="status-bar">
//         <div class="status online" id="statusIndicator">
//             <span class="pulse">●</span> SYSTEM ONLINE
//         </div>
//     </div>
    
//     <div class="dashboard">
//         <div class="card meter-card">
//             <div class="card-header">
//                 <div class="card-icon"></div>
//                 <div class="card-title">TRACK POSITION</div>
//             </div>
//             <div class="card-value" id="meterValue">0.000 <span class="card-unit">m</span></div>
//             <div class="card-description">Encoder-based precise track measurement</div>
//         </div>
        
//         <div class="card">
//             <div class="card-header">
//                 <div class="card-icon"></div>
//                 <div class="card-title">TRACK DISPLACEMENT</div>
//             </div>
//             <div class="card-value" id="displacement">0.00 <span class="card-unit">mm</span></div>
//             <div class="card-description">LVDT sensor track deformation</div>
//         </div>
        
//         <div class="card">
//             <div class="card-header">
//                 <div class="card-icon"></div>
//                 <div class="card-title">GPS COORDINATES</div>
//             </div>
//             <div class="multi-value">
//                 <div class="mini-value">
//                     <div class="mini-value-label">LATITUDE</div>
//                     <div class="mini-value-number" id="latitude">--</div>
//                 </div>
//                 <div class="mini-value">
//                     <div class="mini-value-label">LONGITUDE</div>
//                     <div class="mini-value-number" id="longitude">--</div>
//                 </div>
//                 <div class="mini-value">
//                     <div class="mini-value-label">ALTITUDE</div>
//                     <div class="mini-value-number" id="altitude">-- m</div>
//                 </div>
//             </div>
//         </div>
        
//         <div class="card">
//             <div class="card-header">
//                 <div class="card-icon"></div>
//                 <div class="card-title">MOTION DYNAMICS</div>
//             </div>
//             <div class="multi-value">
//                 <div class="mini-value">
//                     <div class="mini-value-label">SPEED</div>
//                     <div class="mini-value-number" id="speed">-- km/h</div>
//                 </div>
//                 <div class="mini-value">
//                     <div class="mini-value-label">COURSE</div>
//                     <div class="mini-value-number" id="course">--°</div>
//                 </div>
//                 <div class="mini-value">
//                     <div class="mini-value-label">TIME</div>
//                     <div class="mini-value-number" id="time">--:--</div>
//                 </div>
//             </div>
//         </div>
        
//         <div class="card">
//             <div class="card-header">
//                 <div class="card-icon"></div>
//                 <div class="card-title">ACCELEROMETER</div>
//             </div>
//             <div class="multi-value">
//                 <div class="mini-value">
//                     <div class="mini-value-label">X-AXIS</div>
//                     <div class="mini-value-number" id="accelX">-- g</div>
//                 </div>
//                 <div class="mini-value">
//                     <div class="mini-value-label">Y-AXIS</div>
//                     <div class="mini-value-number" id="accelY">-- g</div>
//                 </div>
//                 <div class="mini-value">
//                     <div class="mini-value-label">Z-AXIS</div>
//                     <div class="mini-value-number" id="accelZ">-- g</div>
//                 </div>
//             </div>
//         </div>
//     </div>
    
//     <script>
//         let updateCount = 0;
        
//         function updateData() {
//             const indicator = document.getElementById('updateIndicator');
//             indicator.style.background = '#ff9800';
            
//             fetch('/api/data')
//                 .then(response => response.json())
//                 .then(data => {
//                     // Update meter value with animation
//                     document.getElementById('meterValue').innerHTML = 
//                         data.meter_value.toFixed(3) + ' <span class="card-unit">m</span>';
                    
//                     // Update displacement
//                     document.getElementById('displacement').innerHTML = 
//                         data.displacement.toFixed(2) + ' <span class="card-unit">mm</span>';
                    
//                     // Update GPS coordinates
//                     document.getElementById('latitude').textContent = data.latitude.toFixed(6);
//                     document.getElementById('longitude').textContent = data.longitude.toFixed(6);
//                     document.getElementById('altitude').textContent = data.altitude.toFixed(1) + 'm';
                    
//                     // Update motion data
//                     document.getElementById('speed').textContent = data.speed.toFixed(1) + ' km/h';
//                     document.getElementById('course').textContent = data.course.toFixed(1) + '°';
//                     document.getElementById('time').textContent = data.utc_time;
                    
//                     // Update accelerometer
//                     document.getElementById('accelX').textContent = data.accel_x.toFixed(3) + 'g';
//                     document.getElementById('accelY').textContent = data.accel_y.toFixed(3) + 'g';
//                     document.getElementById('accelZ').textContent = data.accel_z.toFixed(3) + 'g';
                    
//                     // Update status
//                     const status = document.getElementById('statusIndicator');
//                     status.className = 'status online';
//                     status.innerHTML = '<span class="pulse">●</span> SYSTEM ONLINE';
                    
//                     indicator.style.background = '#4CAF50';
//                 })
//                 .catch(error => {
//                     console.error('Update failed:', error);
//                     const status = document.getElementById('statusIndicator');
//                     status.className = 'status offline';
//                     status.innerHTML = '<span class="pulse"></span> CONNECTION LOST';
                    
//                     indicator.style.background = '#f44336';
//                 });
//         }
        
//         // Update every 500ms for smooth real-time feel
//         setInterval(updateData, 500);
//         updateData(); // Initial load
//     </script>
// </body>
// </html>
//   )";
  
//   server.send(200, "text/html", html);
// }

// void handleAPIData() {
//   StaticJsonDocument<384> doc;
  
//   doc["displacement"] = currentData.displacement;
//   doc["accel_x"] = currentData.accel_x;
//   doc["accel_y"] = currentData.accel_y;
//   doc["accel_z"] = currentData.accel_z;
//   doc["latitude"] = currentData.latitude;
//   doc["longitude"] = currentData.longitude;
//   doc["altitude"] = currentData.altitude;
//   doc["speed"] = currentData.speed;
//   doc["course"] = currentData.course;
//   doc["utc_time"] = currentData.utc_time;
//   doc["meter_value"] = currentData.meter_value;
//   doc["last_update"] = currentData.last_update;
  
//   String jsonString;
//   serializeJson(doc, jsonString);
  
//   server.send(200, "application/json", jsonString);
// }

// void calculateMeterValue() {
//   // Convert encoder pulses to meters
//   noInterrupts();
//   int32_t currentCount = encoderCount;
//   interrupts();
  
//   meterValue = (float)currentCount / PULSES_PER_METER;
//   currentData.meter_value = meterValue;
// }

// void readLVDT() {
//   float avg_voltage = 0;
//   for (int i = 0; i < NUM_SAMPLES; i++) {
//     avg_voltage += analogRead(LVDT_PIN) * (VREF / ADC_RESOLUTION);
//     delayMicroseconds(50); // Faster sampling
//   }
//   avg_voltage /= NUM_SAMPLES;
  
//   float displacement = (avg_voltage - ZERO_VOLTAGE) * VOLTAGE_TO_MM;
//   displacement = constrain(displacement, -74.5, 74.5);
  
//   currentData.displacement = displacement;
// }

// void readAccelerometer() {
//   auto accelerations = adxl355.getAccelerations();
//   currentData.accel_x = accelerations.x;
//   currentData.accel_y = accelerations.y;
//   currentData.accel_z = accelerations.z;
// }

// void readGPSData() {
//   String response = sendATCommand(F("AT+QGPSLOC=2"), 1000); // Faster timeout
  
//   if (response.indexOf("+QGPSLOC:") != -1) {
//     parseGPSExtendedData(response);
//   }
// }

// void parseGPSExtendedData(String response) {
//   int start = response.indexOf(':') + 2;
//   int end = response.indexOf("\r\n", start);
//   String data = response.substring(start, end);
  
//   String fields[12];
//   int fieldCount = 0;
//   int lastComma = -1;
  
//   for (int i = 0; i < data.length() && fieldCount < 12; i++) {
//     if (data.charAt(i) == ',') {
//       fields[fieldCount++] = data.substring(lastComma + 1, i);
//       lastComma = i;
//     }
//   }
//   if (fieldCount < 12) {
//     fields[fieldCount] = data.substring(lastComma + 1);
//   }
  
//   // Update GPS data in structure
//   currentData.utc_time = formatUTCTime(fields[0]);
//   currentData.latitude = fields[1].toFloat();
//   currentData.longitude = fields[2].toFloat();
//   currentData.altitude = fields[3].toFloat();
//   currentData.speed = fields[4].toFloat();
//   currentData.course = fields[5].toFloat();
// }

// String formatUTCTime(String timeStr) {
//   if (timeStr.length() >= 6) {
//     return timeStr.substring(0, 2) + ":" + 
//            timeStr.substring(2, 4) + ":" + 
//            timeStr.substring(4, 6);
//   }
//   return timeStr;
// }

// String sendATCommand(const __FlashStringHelper* command, unsigned long timeout) {
//   modemSerial.println(command);
  
//   String response;
//   unsigned long start = millis();
//   while (millis() - start < timeout) {
//     while (modemSerial.available()) {
//       char c = modemSerial.read();
//       response += c;
//     }
//   }
//   return response;
// }

// void printAllData() {
//   Serial.println(F("\n=== TracX-Pro System Status ==="));
//   Serial.print(F("Encoder Count: ")); Serial.print(encoderCount);
//   Serial.print(F(" | Meter Value: ")); Serial.print(meterValue, 3); Serial.println(F(" m"));
//   Serial.print(F("Displacement: ")); Serial.print(currentData.displacement, 2); Serial.println(F(" mm"));
//   Serial.print(F("Accelerations - X: ")); Serial.print(currentData.accel_x, 3);
//   Serial.print(F(" g, Y: ")); Serial.print(currentData.accel_y, 3);
//   Serial.print(F(" g, Z: ")); Serial.print(currentData.accel_z, 3); Serial.println(F(" g"));
//   Serial.print(F("GPS - Lat: ")); Serial.print(currentData.latitude, 6);
//   Serial.print(F(", Lng: ")); Serial.print(currentData.longitude, 6);
//   Serial.print(F(", Alt: ")); Serial.print(currentData.altitude); Serial.println(F(" m"));
//   Serial.print(F("Speed: ")); Serial.print(currentData.speed); Serial.print(F(" km/h, Course: "));
//   Serial.print(currentData.course); Serial.println(F("°"));
//   Serial.print(F("UTC Time: ")); Serial.println(currentData.utc_time);
//   Serial.println(F("=================================="));
// }




///////--------------     corrected ui with start and stop  ------------


#include <PL_ADXL355.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// UART Setup
HardwareSerial modemSerial(2);    // UART2 (RX-16, TX-17) for GPS

// Encoder pins for meter calculation
const int ENCODER_A_PIN = 25;    // GPIO25 for encoder channel A
const int ENCODER_B_PIN = 26;    // GPIO26 for encoder channel B

// ADXL355 Setup
PL::ADXL355 adxl355;
uint8_t i2cAddress = 0x1D;
auto range = PL::ADXL355_Range::range2g;

// LVDT Setup
const int LVDT_PIN = 34;
const float VREF = 3.3;
const int ADC_RESOLUTION = 4095;
const float ZERO_VOLTAGE = 1.65;
const float MIN_VOLTAGE = 0.82;
const float MAX_VOLTAGE = 2.48;
const float VOLTAGE_TO_MM = 75.0 / (MAX_VOLTAGE - MIN_VOLTAGE);

// Add this new variable for zero offset
float ZERO_VOLTAGE_OFFSET = 0.0;  // Offset to adjust zero point

// Encoder variables
volatile int32_t encoderCount = 0;
volatile bool encoderStateA = false;
volatile bool encoderStateB = false;
volatile bool prevStateA = false;
volatile bool prevStateB = false;
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 1; // Reduced to 1ms

// Meter calculation constants
const float PULSES_PER_METER = 1000.0;
float meterValue = 0.0;

// System control
bool systemRunning = false;  // Start/Stop control

// Web Server Setup
WebServer server(80);
const char* ssid = "TracX_Pro";
const char* password = "12345678";

// Data Storage
struct SensorData {
  float displacement = 0.0;
  float accel_x = 0.0;
  float accel_y = 0.0;
  float accel_z = 0.0;
  float latitude = 0.0;
  float longitude = 0.0;
  float altitude = 0.0;
  float speed = 0.0;
  float course = 0.0;
  String utc_time = "";
  float meter_value = 0.0;
  unsigned long last_update = 0;
} currentData;  

// Interrupt service routine for encoder
void IRAM_ATTR encoderISR() {
  unsigned long currentTime = micros();
  
  // Minimal debouncing
  if (currentTime - lastInterruptTime < (debounceDelay * 1000)) {
    return;
  }
  lastInterruptTime = currentTime;
  
  // Read current states
  bool currentStateA = digitalRead(ENCODER_A_PIN);
  bool currentStateB = digitalRead(ENCODER_B_PIN);
  
  // Determine direction based on state changes
  if ((prevStateA == LOW && prevStateB == LOW && currentStateA == HIGH) ||
      (prevStateA == HIGH && prevStateB == HIGH && currentStateA == LOW)) {
    encoderCount++;
  }
  else if ((prevStateA == LOW && prevStateB == LOW && currentStateB == HIGH) ||
           (prevStateA == HIGH && prevStateB == HIGH && currentStateB == LOW)) {
    encoderCount--;
  }
  
  // Update previous states
  prevStateA = currentStateA;
  prevStateB = currentStateB;
}

void setup() {
  Serial.begin(115200);
  
  // Initialize ADC for fastest possible readings
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  // Initialize encoder pins
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  
  // Attach interrupts for encoder
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), encoderISR, CHANGE);
  
  // Initialize GPS UART
  modemSerial.begin(115200, SERIAL_8N1, 16, 17);
  
  Serial.println(F("TracX-Pro Enhanced System Starting..."));
  
  // Initialize GPS
  sendATCommand(F("AT+QGPS=1"), 1000);
  sendATCommand(F("AT+QGPSCFG=\"gnssconfig\",1"), 500);
  
  // Initialize ADXL355
  adxl355.beginI2C(i2cAddress);
  adxl355.setRange(range);
  adxl355.enableMeasurement();
  
  // Initialize WiFi AP
  setupWiFiAP();
  
  // Setup web server routes
  setupWebServer();
  
  Serial.println(F("System initialized successfully!"));
  Serial.println(F("Connect to WiFi: TracX_Pro (password: 12345678)"));
  Serial.println(F("Open browser: http://192.168.4.1"));
}

void loop() {
  // Handle web server
  server.handleClient();
  
  // Only read sensors if system is running
  if (systemRunning) {
    // Read encoder and calculate meter value
    calculateMeterValue();
    
    // Read LVDT data (direct raw value, no averaging)
    readLVDT();
    
    // Read accelerometer data
    readAccelerometer();
    
    // Read GPS data
    readGPSData();
    
    // Update timestamp
    currentData.last_update = millis();
  }
  
  // No delays here for maximum speed
}

void setupWiFiAP() {
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
}

void setupWebServer() {
  // Serve main page
  server.on("/", handleRoot);
  
  // API endpoint for sensor data
  server.on("/api/data", handleAPIData);
  
  // Raw data endpoint
  server.on("/data", handleRawData);
  
  // Start data collection
  server.on("/start", handleStart);
  
  // Stop data collection
  server.on("/stop", handleStop);
  
  server.begin();
  Serial.println("Web server started");
}

void handleStart() {
  systemRunning = true;
 
  // Reset encoder count and meter value to zero
  noInterrupts();
  encoderCount = 0;
  interrupts();
  meterValue = 0.0;
  currentData.meter_value = 0.0;
  
  // Reset LVDT displacement to zero by setting a new zero reference
  float currentVoltage = analogRead(LVDT_PIN) * (VREF / ADC_RESOLUTION);
  ZERO_VOLTAGE_OFFSET = currentVoltage - ZERO_VOLTAGE;  // Calculate offset from original zero
  currentData.displacement = 0.0;

  server.send(200, "text/plain", "Data collection STARTED");
  Serial.println("Data collection STARTED");
}

void handleStop() {
  systemRunning = false;
  server.send(200, "text/plain", "Data collection STOPPED");
  Serial.println("Data collection STOPPED");
}

void handleRawData() {
  // Return only raw JSON data, no HTML
  StaticJsonDocument<512> doc;
  
  doc["displacement"] = currentData.displacement;
  doc["accel_x"] = currentData.accel_x;
  doc["accel_y"] = currentData.accel_y;
  doc["accel_z"] = currentData.accel_z;
  doc["latitude"] = currentData.latitude;
  doc["longitude"] = currentData.longitude;
  doc["altitude"] = currentData.altitude;
  doc["speed"] = currentData.speed;
  doc["course"] = currentData.course;
  doc["utc_time"] = currentData.utc_time;
  doc["meter_value"] = currentData.meter_value;
  doc["last_update"] = currentData.last_update;
  doc["system_running"] = systemRunning;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.send(200, "application/json", jsonString);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Rail Trolly</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
            color: white;
            min-height: 100vh;
            overflow-x: hidden;
        }
        
        .header {
            text-align: center;
            padding: 20px;
            background: rgba(0,0,0,0.2);
            backdrop-filter: blur(10px);
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        
        .header h1 {
            font-size: 2.5em;
            font-weight: 300;
            margin-bottom: 5px;
            letter-spacing: 2px;
        }
        
        .header p {
            opacity: 0.8;
            font-size: 1.1em;
        }
        
        .control-panel {
            display: flex;
            justify-content: center;
            align-items: center;
            gap: 20px;
            padding: 20px;
            background: rgba(0,0,0,0.1);
        }
        
        .control-btn {
            padding: 12px 30px;
            border: none;
            border-radius: 25px;
            font-weight: 600;
            font-size: 1em;
            cursor: pointer;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .start-btn {
            background: linear-gradient(45deg, #4CAF50, #45a049);
            color: white;
            box-shadow: 0 4px 15px rgba(76, 175, 80, 0.3);
        }
        
        .start-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(76, 175, 80, 0.4);
        }
        
        .stop-btn {
            background: linear-gradient(45deg, #f44336, #d32f2f);
            color: white;
            box-shadow: 0 4px 15px rgba(244, 67, 54, 0.3);
        }
        
        .stop-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(244, 67, 54, 0.4);
        }
        
        .status-bar {
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 10px;
            background: rgba(0,0,0,0.1);
        }
        
        .status {
            padding: 8px 20px;
            border-radius: 25px;
            font-weight: 600;
            font-size: 0.9em;
            display: flex;
            align-items: center;
            gap: 8px;
            transition: all 0.3s ease;
        }
        
        .status.online { 
            background: linear-gradient(45deg, #4CAF50, #45a049);
            box-shadow: 0 4px 15px rgba(76, 175, 80, 0.3);
        }
        
        .status.offline { 
            background: linear-gradient(45deg, #f44336, #d32f2f);
            box-shadow: 0 4px 15px rgba(244, 67, 54, 0.3);
        }
        
        .status.stopped { 
            background: linear-gradient(45deg, #ff9800, #f57c00);
            box-shadow: 0 4px 15px rgba(255, 152, 0, 0.3);
        }
        
        .pulse { animation: pulse 2s infinite; }
        
        @keyframes pulse {
            0% { opacity: 1; }
            50% { opacity: 0.5; }
            100% { opacity: 1; }
        }
        
        .dashboard {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 20px;
            padding: 20px;
            max-width: 1400px;
            margin: 0 auto;
        }
        
        .card {
            background: rgba(255,255,255,0.08);
            border: 1px solid rgba(255,255,255,0.12);
            border-radius: 20px;
            padding: 25px;
            backdrop-filter: blur(15px);
            transition: all 0.3s ease;
            position: relative;
            overflow: hidden;
        }
        
        .card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            height: 3px;
            background: linear-gradient(90deg, #ff6b6b, #4ecdc4, #45b7d1, #f39c12);
            opacity: 0;
            transition: opacity 0.3s ease;
        }
        
        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 20px 40px rgba(0,0,0,0.3);
        }
        
        .card:hover::before {
            opacity: 1;
        }
        
        .card-header {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-bottom: 20px;
        }
        
        .card-icon {
            font-size: 1.8em;
            opacity: 0.8;
        }
        
        .card-title {
            font-size: 1.1em;
            font-weight: 600;
            opacity: 0.9;
        }
        
        .card-value {
            font-size: 2.8em;
            font-weight: 700;
            color: #4ecdc4;
            margin: 15px 0;
            text-shadow: 0 2px 10px rgba(78, 205, 196, 0.3);
        }
        
        .card-unit {
            font-size: 0.6em;
            opacity: 0.7;
            font-weight: 400;
        }
        
        .card-description {
            font-size: 0.9em;
            opacity: 0.7;
            line-height: 1.4;
        }
        
        .meter-card {
            grid-column: span 2;
            background: linear-gradient(135deg, rgba(255, 107, 107, 0.15), rgba(255, 165, 0, 0.15));
            border: 2px solid rgba(255, 107, 107, 0.3);
        }
        
        .meter-card .card-value {
            font-size: 4em;
            color: #ff6b6b;
            animation: glow 3s ease-in-out infinite alternate;
        }
        
        @keyframes glow {
            from { text-shadow: 0 0 20px rgba(255, 107, 107, 0.5); }
            to { text-shadow: 0 0 30px rgba(255, 107, 107, 0.8), 0 0 40px rgba(255, 107, 107, 0.6); }
        }
        
        .multi-value {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 15px;
            margin: 15px 0;
        }
        
        .mini-value {
            text-align: center;
            padding: 10px;
            background: rgba(255,255,255,0.05);
            border-radius: 10px;
        }
        
        .mini-value-label {
            font-size: 0.8em;
            opacity: 0.7;
            margin-bottom: 5px;
        }
        
        .mini-value-number {
            font-size: 1.2em;
            font-weight: 600;
            color: #45b7d1;
        }
        
        .update-indicator {
            position: fixed;
            top: 20px;
            right: 20px;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #4CAF50;
            animation: blink 1s infinite;
        }
        
        @keyframes blink {
            0%, 50% { opacity: 1; }
            51%, 100% { opacity: 0.3; }
        }
        
        @media (max-width: 768px) {
            .meter-card { grid-column: span 1; }
            .card-value { font-size: 2.2em; }
            .meter-card .card-value { font-size: 3em; }
            .dashboard { grid-template-columns: 1fr; }
            .control-panel { flex-direction: column; gap: 10px; }
        }
    </style>
</head>
<body>
    <div class="update-indicator" id="updateIndicator"></div>
    
    <div class="header">
        <h1>TRACX PRO</h1>
        <p>Advanced Railway Monitoring & Analytics</p>
    </div>
    
    <div class="control-panel">
        <button class="control-btn start-btn" onclick="startSystem()">START</button>
        <button class="control-btn stop-btn" onclick="stopSystem()">STOP</button>
    </div>
    
    <div class="status-bar">
        <div class="status offline" id="statusIndicator">
            <span class="pulse"></span> SYSTEM STOPPED
        </div>
    </div>
    
    <div class="dashboard">
        <div class="card meter-card">
            <div class="card-header">
                <div class="card-icon"></div>
                <div class="card-title">TRACK POSITION</div>
            </div>
            <div class="card-value" id="meterValue">0.000 <span class="card-unit">m</span></div>
            <div class="card-description">Encoder-based precise track measurement</div>
        </div>
        
        <div class="card">
            <div class="card-header">
                <div class="card-icon"></div>
                <div class="card-title">TRACK DISPLACEMENT</div>
            </div>
            <div class="card-value" id="displacement">0.00 <span class="card-unit">mm</span></div>
            <div class="card-description">LVDT sensor track deformation</div>
        </div>
        
        <div class="card">
            <div class="card-header">
                <div class="card-icon"></div>
                <div class="card-title">GPS COORDINATES</div>
            </div>
            <div class="multi-value">
                <div class="mini-value">
                    <div class="mini-value-label">LATITUDE</div>
                    <div class="mini-value-number" id="latitude">--</div>
                </div>
                <div class="mini-value">
                    <div class="mini-value-label">LONGITUDE</div>
                    <div class="mini-value-number" id="longitude">--</div>
                </div>
                <div class="mini-value">
                    <div class="mini-value-label">ALTITUDE</div>
                    <div class="mini-value-number" id="altitude">-- m</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <div class="card-header">
                <div class="card-icon"></div>
                <div class="card-title">MOTION DYNAMICS</div>
            </div>
            <div class="multi-value">
                <div class="mini-value">
                    <div class="mini-value-label">SPEED</div>
                    <div class="mini-value-number" id="speed"> m/s</div>
                </div>
                <div class="mini-value">
                    <div class="mini-value-label">COURSE</div>
                    <div class="mini-value-number" id="course"></div>
                </div>
                <div class="mini-value">
                    <div class="mini-value-label">TIME</div>
                    <div class="mini-value-number" id="time"></div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <div class="card-header">
                <div class="card-icon"></div>
                <div class="card-title">ACCELEROMETER</div>
            </div>
            <div class="multi-value">
                <div class="mini-value">
                    <div class="mini-value-label">X-AXIS</div>
                    <div class="mini-value-number" id="accelX">-- g</div>
                </div>
                <div class="mini-value">
                    <div class="mini-value-label">Y-AXIS</div>
                    <div class="mini-value-number" id="accelY">-- g</div>
                </div>
                <div class="mini-value">
                    <div class="mini-value-label">Z-AXIS</div>
                    <div class="mini-value-number" id="accelZ">-- g</div>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        function startSystem() {
            fetch('/start')
                .then(response => response.text())
                .then(data => {
                    console.log('Start response:', data);
                })
                .catch(error => console.error('Start error:', error));
        }
        
        function stopSystem() {
            fetch('/stop')
                .then(response => response.text())
                .then(data => {
                    console.log('Stop response:', data);
                })
                .catch(error => console.error('Stop error:', error));
        }
        
        function updateData() {
            const indicator = document.getElementById('updateIndicator');
            indicator.style.background = '#ff9800';
            
            fetch('/api/data')
                .then(response => response.json())
                .then(data => {
                    // Update meter value
                    document.getElementById('meterValue').innerHTML = 
                        data.meter_value.toFixed(3) + ' <span class="card-unit">m</span>';
                    
                    // Update displacement
                    document.getElementById('displacement').innerHTML = 
                        data.displacement.toFixed(2) + ' <span class="card-unit">mm</span>';
                    
                    // Update GPS coordinates
                    document.getElementById('latitude').textContent = data.latitude.toFixed(6);
                    document.getElementById('longitude').textContent = data.longitude.toFixed(6);
                    document.getElementById('altitude').textContent = data.altitude.toFixed(1) + 'm';
                    
                    // Update motion data
                    document.getElementById('speed').textContent = data.speed.toFixed(1) + ' km/h';
                    document.getElementById('course').textContent = data.course.toFixed(1) + '°';
                    document.getElementById('time').textContent = data.utc_time;
                    
                    // Update accelerometer
                    document.getElementById('accelX').textContent = data.accel_x.toFixed(3) + 'g';
                    document.getElementById('accelY').textContent = data.accel_y.toFixed(3) + 'g';
                    document.getElementById('accelZ').textContent = data.accel_z.toFixed(3) + 'g';
                    
                    // Update status based on system_running
                    const status = document.getElementById('statusIndicator');
                    if (data.system_running) {
                        status.className = 'status online';
                        status.innerHTML = '<span class="pulse"></span> SYSTEM RUNNING';
                    } else {
                        status.className = 'status stopped';
                        status.innerHTML = '<span class="pulse"></span> SYSTEM STOPPED';
                    }
                    
                    indicator.style.background = '#4CAF50';
                })
                .catch(error => {
                    console.error('Update failed:', error);
                    const status = document.getElementById('statusIndicator');
                    status.className = 'status offline';
                    status.innerHTML = '<span class="pulse"></span> CONNECTION LOST';
                    
                    indicator.style.background = '#f44336';
                });
        }
        
        // Update every 100ms for real-time feel
        setInterval(updateData, 100);
        updateData(); // Initial load
    </script>
</body>
</html>
 )rawliteral";
  
  server.send(200, "text/html", html);
}

void handleAPIData() {
  StaticJsonDocument<512> doc;
  
  doc["displacement"] = currentData.displacement;
  doc["accel_x"] = currentData.accel_x;
  doc["accel_y"] = currentData.accel_y;
  doc["accel_z"] = currentData.accel_z;
  doc["latitude"] = currentData.latitude;
  doc["longitude"] = currentData.longitude;
  doc["altitude"] = currentData.altitude;
  doc["speed"] = currentData.speed;
  doc["course"] = currentData.course;
  doc["utc_time"] = currentData.utc_time;
  doc["meter_value"] = currentData.meter_value;
  doc["last_update"] = currentData.last_update;
  doc["system_running"] = systemRunning;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.send(200, "application/json", jsonString);
}

void calculateMeterValue() {
  // Convert encoder pulses to meters (direct value, no processing delays)
  noInterrupts();
  int32_t currentCount = encoderCount;
  interrupts();
  
  meterValue = (float)currentCount / PULSES_PER_METER;
  currentData.meter_value = meterValue;
}

void readLVDT() {
  // Direct single reading, no averaging
  float voltage = analogRead(LVDT_PIN) * (VREF / ADC_RESOLUTION);
  
  // Apply zero offset when calculating displacement
  float displacement = (voltage - ZERO_VOLTAGE - ZERO_VOLTAGE_OFFSET) * VOLTAGE_TO_MM;
  displacement = constrain(displacement, -74.5, 74.5);
  
  currentData.displacement = displacement;
}

void readAccelerometer() {
  auto accelerations = adxl355.getAccelerations();
  currentData.accel_x = accelerations.x;
  currentData.accel_y = accelerations.y;
  currentData.accel_z = accelerations.z;
}

void readGPSData() {
  String response = sendATCommand(F("AT+QGPSLOC=2"), 500); // Reduced timeout
  
  if (response.indexOf("+QGPSLOC:") != -1) {
    parseGPSExtendedData(response);
  }
}

void parseGPSExtendedData(String response) {
  int start = response.indexOf(':') + 2;
  int end = response.indexOf("\r\n", start);
  String data = response.substring(start, end);
  
  String fields[12];
  int fieldCount = 0;
  int lastComma = -1;
  
  for (int i = 0; i < data.length() && fieldCount < 12; i++) {
    if (data.charAt(i) == ',') {
      fields[fieldCount++] = data.substring(lastComma + 1, i);
      lastComma = i;
    }
  }
  if (fieldCount < 12) {
    fields[fieldCount] = data.substring(lastComma + 1);
  }
  
  // Update GPS data in structure
  currentData.utc_time = formatUTCTime(fields[0]);
  currentData.latitude = fields[1].toFloat();
  currentData.longitude = fields[2].toFloat();
  currentData.altitude = fields[3].toFloat();
  currentData.speed = fields[4].toFloat();
  currentData.course = fields[5].toFloat();
}

String formatUTCTime(String timeStr) {
  if (timeStr.length() >= 6) {
    return timeStr.substring(0, 2) + ":" + 
           timeStr.substring(2, 4) + ":" + 
           timeStr.substring(4, 6);
  }
  return timeStr;
}

String sendATCommand(const __FlashStringHelper* command, unsigned long timeout) {
  modemSerial.println(command);
  
  String response;
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (modemSerial.available()) {
      char c = modemSerial.read();
      response += c;
    }
  }
  return response;
}