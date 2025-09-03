#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <Update.h>

// ==================== CONFIGURATION ====================
// SoftAP Configuration
const char* ap_ssid = "TracX_OMS";
const char* ap_password = "12345678";
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// Web server on port 80
WebServer server(80);

// GPS Configuration (UART2)
HardwareSerial modemSerial(2); // UART2 (RX-16, TX-17)

// ADXL355 SPI Configuration
#define CS_PIN 5
#define INT_PIN 4
#define DRDY_PIN 2

// ADXL355 Register Addresses
#define DEVID_AD        0x00
#define DEVID_MST       0x01
#define PARTID          0x02
#define REVID           0x03
#define STATUS          0x04
#define XDATA3          0x08
#define XDATA2          0x09
#define XDATA1          0x0A
#define YDATA3          0x0B
#define YDATA2          0x0C
#define YDATA1          0x0D
#define ZDATA3          0x0E
#define ZDATA2          0x0F
#define ZDATA1          0x10
#define RANGE           0x2C
#define POWER_CTL       0x2D
#define FILTER          0x28

// Range settings
#define RANGE_2G        0x01
#define RANGE_4G        0x02
#define RANGE_8G        0x03

// ==================== GLOBAL VARIABLES ====================
bool systemRunning = false;
bool gpsInitialized = false;
bool accelInitialized = false;
unsigned long lastGPSCheck = 0;
unsigned long lastDataPrint = 0;
const unsigned long GPS_CHECK_INTERVAL = 5000; // Check GPS every 5 seconds
const unsigned long PRINT_INTERVAL = 10000; // Print data every 10 seconds

// OTA Progress variables
int otaProgress = 0;
bool otaInProgress = false;

// Data structures
struct GPSData {
  String utcTime = "";
  float latitude = 0.0;
  float longitude = 0.0;
  float altitude = 0.0;
  float speed = 0.0;
  float course = 0.0;
  String hdop = "";
  String date = "";
  bool valid = false;
  unsigned long lastUpdate = 0;
};

struct AccelData {
  float x = 0.0;
  float y = 0.0;
  float z = 0.0;
  bool valid = false;
  unsigned long lastUpdate = 0;
};

GPSData currentGPS;
AccelData currentAccel;

// ==================== SETUP FUNCTION ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println(F("\n=== TracX-1b Combined GPS & Accelerometer System ==="));
  Serial.println(F("Initializing system components..."));
  
  // Initialize SPIFFS for web files
  if (!SPIFFS.begin(true)) {
    Serial.println(F("SPIFFS initialization failed!"));
  } else {
    Serial.println(F("SPIFFS initialized successfully"));
  }
  
  // Initialize SoftAP
  initSoftAP();
  
  // Initialize OTA
  initOTA();
  
  // Initialize GPS
  initGPS();
  
  // Initialize Accelerometer
  initAccelerometer();
  
  // Setup web server routes
  setupWebServer();
  
  // Start web server
  server.begin();
  Serial.print(F("Access the system at: http://"));
  Serial.println(WiFi.softAPIP());
  Serial.println(F("System initialization complete!"));
}

// ==================== MAIN LOOP ====================
void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  
  // Handle web server requests
  server.handleClient();
  
  // Continuous sensor reading if system is running
  if (systemRunning) {
    // Continuous accelerometer reading (no delays)
    readAccelerometerData();
    
    // Check GPS connection every 5 seconds
    checkGPSConnection();
    
    // Serial Printing  data periodically
    //if (millis() - lastDataPrint >= PRINT_INTERVAL) {
     // printCurrentData();
     // lastDataPrint = millis();
    //}
  }
  
  // No delays here to avoid missing accelerometer values
}

// ==================== SOFTAP INITIALIZATION ====================
void initSoftAP() {
  Serial.println(F("Configuring SoftAP..."));
  
  // Configure SoftAP
  WiFi.softAP(ap_ssid, ap_password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  
  delay(2000); // Give time for SoftAP to start
  
  Serial.println(F("SoftAP configured successfully"));
  Serial.print(F("AP SSID: ")); Serial.println(ap_ssid);
  Serial.print(F("AP Password: ")); Serial.println(ap_password);
  Serial.print(F("AP IP address: ")); Serial.println(WiFi.softAPIP());
}

// ==================== OTA INITIALIZATION ====================
void initOTA() {
  ArduinoOTA.setHostname("TracX-OMS");
  ArduinoOTA.setPassword("12345678"); // Change this password
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
    systemRunning = false; // Stop data collection during update
    otaInProgress = true;
    otaProgress = 0;
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nOTA Update completed"));
    otaInProgress = false;
    otaProgress = 100;
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    otaProgress = (progress / (total / 100));
    Serial.printf("Progress: %u%%\r", otaProgress);
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    otaInProgress = false;
    otaProgress = 0;
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });
  
  ArduinoOTA.begin();
  Serial.println(F("OTA Ready"));
}

// ==================== GPS INITIALIZATION ====================
void initGPS() {
  Serial.println(F("TracX-1b GPS Data Parser"));
  modemSerial.begin(115200, SERIAL_8N1, 16, 17);
  
  delay(3000); // Wait for modem initialization (exactly like working reference)
  
  // Initialize GPS with optimal settings (exactly like working reference - no response checking)
  sendATCommand(F("AT+QGPS=1"), 1000);
  sendATCommand(F("AT+QGPSCFG=\"gnssconfig\",1"), 500);
  
  gpsInitialized = true;
  Serial.println(F("GPS initialized successfully"));
}

// ==================== ACCELEROMETER INITIALIZATION ====================
void initAccelerometer() {
  Serial.println(F("Initializing Accelerometer..."));
  
  // Initialize SPI
  SPI.begin();
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  
  // Optional interrupt pins
  pinMode(INT_PIN, INPUT);
  pinMode(DRDY_PIN, INPUT);
  
  delay(100);
  
  if (initADXL355()) {
    accelInitialized = true;
    Serial.println(F("ADXL355 initialized successfully"));
  } else {
    Serial.println(F("ADXL355 initialization failed"));
  }
}

// ==================== WEB SERVER SETUP ====================
void setupWebServer() {
  // Serve main HTML page
  server.on("/", HTTP_GET, handleRoot);
  
  // API endpoints
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/data", HTTP_GET, handleData);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/otaprogress", HTTP_GET, handleOTAProgress);
  
  // OTA upload endpoint
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateUpload, handleUpdateFile);
  
  // Handle 404
  server.onNotFound(handleNotFound);
  
  Serial.println(F("Web server routes configured"));
}

// ==================== WEB HANDLERS ====================
void handleRoot() {
  String html = generateMainHTML();
  server.send(200, "text/html", html);
}

void handleStart() {
  systemRunning = true;
  lastDataPrint = millis();
  lastGPSCheck = millis();
  
  DynamicJsonDocument doc(200);
  doc["status"] = "success";
  doc["message"] = "System started";
  doc["running"] = systemRunning;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  Serial.println(F("System started via API"));
}

void handleStop() {
  systemRunning = false;
  
  DynamicJsonDocument doc(200);
  doc["status"] = "success";
  doc["message"] = "System stopped";
  doc["running"] = systemRunning;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  Serial.println(F("System stopped via API"));
}

void handleData() {
  DynamicJsonDocument doc(1024);
  
  // System status
  doc["timestamp"] = millis();
  doc["running"] = systemRunning;
  
  // GPS data
  JsonObject gps = doc.createNestedObject("gps");
  gps["valid"] = currentGPS.valid;
  gps["time"] = currentGPS.utcTime;
  gps["latitude"] = currentGPS.latitude;
  gps["longitude"] = currentGPS.longitude;
  gps["altitude"] = currentGPS.altitude;
  gps["speed"] = currentGPS.speed;
  gps["course"] = currentGPS.course;
  gps["hdop"] = currentGPS.hdop;
  gps["date"] = currentGPS.date;
  gps["last_update"] = currentGPS.lastUpdate;
  
  // Accelerometer data
  JsonObject accel = doc.createNestedObject("accelerometer");
  accel["valid"] = currentAccel.valid;
  accel["x"] = currentAccel.x;
  accel["y"] = currentAccel.y;
  accel["z"] = currentAccel.z;
  accel["last_update"] = currentAccel.lastUpdate;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleStatus() {
  DynamicJsonDocument doc(512);
  doc["running"] = systemRunning;
  doc["gps_initialized"] = gpsInitialized;
  doc["accel_initialized"] = accelInitialized;
  doc["softap_active"] = WiFi.softAPgetStationNum() >= 0;
  doc["ap_ip"] = WiFi.softAPIP().toString();
  doc["connected_clients"] = WiFi.softAPgetStationNum();
  doc["uptime"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleOTAProgress() {
  DynamicJsonDocument doc(200);
  doc["in_progress"] = otaInProgress;
  doc["progress"] = otaProgress;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleReset() {
  server.send(200, "text/plain", "Resetting system...");
  delay(1000);
  ESP.restart();
}

void handleUpdatePage() {
  String html = generateUpdateHTML();
  server.send(200, "text/html", html);
}

void handleUpdateUpload() {
  server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
  delay(1000);
  ESP.restart();
}

void handleUpdateFile() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update: %s\n", upload.filename.c_str());
    otaInProgress = true;
    otaProgress = 0;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    } else {
      otaProgress = (Update.progress() * 100) / Update.size();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %uB\n", upload.totalSize);
      otaProgress = 100;
      otaInProgress = false;
    } else {
      Update.printError(Serial);
      otaInProgress = false;
      otaProgress = 0;
    }
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "404 Not Found");
}

// ==================== GPS CONNECTION CHECKING ====================
void checkGPSConnection() {
  if (!gpsInitialized) return;
  
  unsigned long currentTime = millis();
  
  // Check GPS connection every 5 seconds (like working reference)
  if (currentTime - lastGPSCheck >= GPS_CHECK_INTERVAL) {
    readGPSData();
    lastGPSCheck = currentTime;
    
    // If GPS data is old or invalid, reinitialize
    //if (!currentGPS.valid || (currentTime - currentGPS.lastUpdate > 15000)) {
      //Serial.println(F("GPS connection lost, reinitializing..."));
     // initGPS(); // Reinitialize GPS
   // }
  }
}

// ==================== GPS DATA READING ====================
void readGPSData() {
  String response = sendATCommand(F("AT+QGPSLOC=2"), 500); // Reduced timeout like working reference
  
  if (response.indexOf("+QGPSLOC:") != -1) {
    parseGPSExtendedData(response);
  } else {
    currentGPS.valid = false;
  }
}

// ==================== CONTINUOUS ACCELEROMETER READING ====================
void readAccelerometerData() {
  if (!accelInitialized) return;
  
  float x, y, z;
  if (readAcceleration(x, y, z)) {
    currentAccel.x = x;
    currentAccel.y = y;
    currentAccel.z = z;
    currentAccel.valid = true;
    currentAccel.lastUpdate = millis();
  }
}

// ==================== DATA PRINTING ====================
void printCurrentData() {
  Serial.println(F("\n=== Current Sensor Data ==="));
  Serial.printf("Uptime: %lu ms\n", millis());
  
  if (currentGPS.valid) {
    Serial.printf("GPS: %.6f, %.6f, %.2fm, %.2fkm/h, Course: %.1f°\n", 
      currentGPS.latitude, currentGPS.longitude, currentGPS.altitude, 
      currentGPS.speed, currentGPS.course);
    Serial.printf("GPS Time: %s, HDOP: %s, Date: %s\n", 
      currentGPS.utcTime.c_str(), currentGPS.hdop.c_str(), currentGPS.date.c_str());
  } else {
    Serial.println("GPS: No valid fix");
  }
  
  if (currentAccel.valid) {
    Serial.printf("Accel: X:%.4fg, Y:%.4fg, Z:%.4fg\n", 
      currentAccel.x, currentAccel.y, currentAccel.z);
  } else {
    Serial.println("Accelerometer: No data");
  }
  
  Serial.printf("Connected clients: %d\n", WiFi.softAPgetStationNum());
}

// ==================== GPS FUNCTIONS ====================
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

void parseGPSExtendedData(String response) {
  int start = response.indexOf(':') + 2;
  int end = response.indexOf("\r\n", start);
  if (end == -1) end = response.length();
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
  
  // Check if we have valid data (at least latitude and longitude)
  if (fields[1].length() > 0 && fields[2].length() > 0) {
    // Update GPS data structure
    currentGPS.utcTime = formatUTCTime(fields[0]);
    currentGPS.latitude = fields[1].toFloat();
    currentGPS.longitude = fields[2].toFloat();
    currentGPS.altitude = fields[3].toFloat();
    currentGPS.speed = fields[4].toFloat();
    currentGPS.course = fields[5].toFloat();
    currentGPS.hdop = fields[6];
    currentGPS.date = fields[10]; // DDMMYY format
    currentGPS.valid = true;
    currentGPS.lastUpdate = millis();
  } else {
    currentGPS.valid = false;
  }
}

String formatUTCTime(String timeStr) {
  if (timeStr.length() >= 6) {
    return timeStr.substring(0, 2) + ":" + 
           timeStr.substring(2, 4) + ":" + 
           timeStr.substring(4, 6);
  }
  return timeStr;
}

// ==================== ACCELEROMETER FUNCTIONS ====================
void writeRegister(uint8_t reg, uint8_t value) {
  digitalWrite(CS_PIN, LOW);
  SPI.transfer((reg << 1) | 0x00);
  SPI.transfer(value);
  digitalWrite(CS_PIN, HIGH);
  delayMicroseconds(10);
}

uint8_t readRegister(uint8_t reg) {
  digitalWrite(CS_PIN, LOW);
  SPI.transfer((reg << 1) | 0x01);
  uint8_t value = SPI.transfer(0x00);
  digitalWrite(CS_PIN, HIGH);
  delayMicroseconds(10);
  return value;
}

bool initADXL355() {
  uint8_t devID = readRegister(DEVID_AD);
  if (devID != 0xAD) {
    Serial.print("Wrong Device ID: 0x");
    Serial.println(devID, HEX);
    return false;
  }
  
  uint8_t partID = readRegister(PARTID);
  if (partID != 0xED) {
    Serial.print("Wrong Part ID: 0x");
    Serial.println(partID, HEX);
    return false;
  }
  
  writeRegister(RANGE, RANGE_2G | 0x81);
  writeRegister(FILTER, 0x00);
  writeRegister(POWER_CTL, 0x00);
  
  delay(10);
  return true;
}

bool readAcceleration(float &x, float &y, float &z) {
  uint8_t status = readRegister(STATUS);
  if (!(status & 0x01)) {
    return false;
  }
  
  int32_t rawX = ((int32_t)readRegister(XDATA3) << 12) | 
                 ((int32_t)readRegister(XDATA2) << 4) | 
                 ((int32_t)readRegister(XDATA1) >> 4);
  
  int32_t rawY = ((int32_t)readRegister(YDATA3) << 12) | 
                 ((int32_t)readRegister(YDATA2) << 4) | 
                 ((int32_t)readRegister(YDATA1) >> 4);
  
  int32_t rawZ = ((int32_t)readRegister(ZDATA3) << 12) | 
                 ((int32_t)readRegister(ZDATA2) << 4) | 
                 ((int32_t)readRegister(ZDATA1) >> 4);
  
  if (rawX & 0x80000) rawX |= 0xFFF00000;
  if (rawY & 0x80000) rawY |= 0xFFF00000;
  if (rawZ & 0x80000) rawZ |= 0xFFF00000;
  
  float scale = 4.0 / 1048576.0;
  x = rawX * scale;
  y = rawY * scale;
  z = rawZ * scale;
  
  return true;
}

// ==================== HTML GENERATION ====================
String generateMainHTML() {
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>TracX-1b Control Panel</title>";
  html += "<style>";
  html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; color: #333; }";
  html += ".container { max-width: 1200px; margin: 0 auto; padding: 20px; }";
  html += ".header { text-align: center; color: white; margin-bottom: 30px; }";
  html += ".header h1 { font-size: 2.5em; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }";
  html += ".header p { font-size: 1.2em; opacity: 0.9; }";
  html += ".connection-info { background: rgba(255,255,255,0.1); color: white; padding: 15px; border-radius: 10px; margin-bottom: 20px; text-align: center; }";
  html += ".card { background: white; border-radius: 15px; padding: 25px; margin: 20px 0; box-shadow: 0 10px 30px rgba(0,0,0,0.1); transition: transform 0.3s ease; }";
  html += ".card:hover { transform: translateY(-5px); }";
  html += ".controls { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-bottom: 20px; }";
  html += ".btn { padding: 12px 24px; border: none; border-radius: 8px; font-size: 16px; cursor: pointer; transition: all 0.3s ease; text-decoration: none; display: inline-block; text-align: center; font-weight: 600; }";
  html += ".btn-start { background: #28a745; color: white; } .btn-start:hover { background: #218838; }";
  html += ".btn-stop { background: #dc3545; color: white; } .btn-stop:hover { background: #c82333; }";
  html += ".btn-data { background: #007bff; color: white; } .btn-data:hover { background: #0056b3; }";
  html += ".btn-update { background: #ffc107; color: #212529; } .btn-update:hover { background: #e0a800; }";
  html += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; }";
  html += ".status-item { background: #f8f9fa; padding: 20px; border-radius: 10px; border-left: 4px solid #007bff; }";
  html += ".status-item h3 { margin-bottom: 10px; color: #495057; }";
  html += ".status-value { font-size: 1.5em; font-weight: bold; color: #007bff; }";
  html += ".data-display { background: #212529; color: #f8f9fa; padding: 20px; border-radius: 10px; font-family: 'Courier New', monospace; overflow-x: auto; white-space: pre-wrap; max-height: 400px; overflow-y: auto; }";
  html += ".indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 8px; }";
  html += ".indicator.active { background: #28a745; } .indicator.inactive { background: #dc3545; }";
  html += ".refresh-rate { font-size: 0.9em; color: #6c757d; margin-top: 10px; }";
  html += ".progress-bar { width: 100%; height: 20px; background: #e9ecef; border-radius: 10px; overflow: hidden; margin: 10px 0; }";
  html += ".progress-fill { height: 100%; background: #28a745; width: 0%; transition: width 0.3s ease; }";
  html += ".ota-status { background: #fff3cd; color: #856404; padding: 15px; border-radius: 5px; margin: 10px 0; display: none; }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='header'><h1>TracX-1b Control Panel</h1><p>GPS Tracking & Accelerometer Data Collection System</p></div>";
  
  html += "<div class='connection-info'><strong>SoftAP Mode Active</strong><br>Connect to WiFi: <strong>TracX-1b</strong> | Password: <strong>tracx123456</strong><br>Access Point IP: <strong>192.168.4.1</strong></div>";
  
  html += "<div class='ota-status' id='otaStatus'>";
  html += "<strong>OTA Update in Progress...</strong><br>";
  html += "<div class='progress-bar'><div class='progress-fill' id='otaProgress'></div></div>";
  html += "<span id='otaProgressText'>0%</span>";
  html += "</div>";
  
  html += "<div class='card'><h2>System Controls</h2><div class='controls'>";
  html += "<button class='btn btn-start' onclick='startSystem()'>Start System</button>";
  html += "<button class='btn btn-stop' onclick='stopSystem()'>Stop System</button>";
  html += "<button class='btn btn-data' onclick='refreshData()'>Refresh Data</button>";
  html += "<a href='/update' class='btn btn-update'>OTA Update</a>";
  html += "</div></div>";
  
  html += "<div class='card'><h2>System Status</h2><div class='status-grid'>";
  html += "<div class='status-item'><h3><span class='indicator' id='systemIndicator'></span>System Status</h3><div class='status-value' id='systemStatus'>Loading...</div></div>";
  html += "<div class='status-item'><h3><span class='indicator' id='gpsIndicator'></span>GPS Module</h3><div class='status-value' id='gpsStatus'>Loading...</div></div>";
  html += "<div class='status-item'><h3><span class='indicator' id='accelIndicator'></span>Accelerometer</h3><div class='status-value' id='accelStatus'>Loading...</div></div>";
  html += "<div class='status-item'><h3><span class='indicator' id='clientsIndicator'></span>Connected Clients</h3><div class='status-value' id='clientsStatus'>Loading...</div></div>";
  html += "</div></div>";
  
  html += "<div class='card'><h2>Live Data Feed</h2>";
  html += "<div class='refresh-rate'>⚡ Continuous accelerometer + GPS keepalive every 10s + On-demand GPS reading</div>";
  html += "<div class='data-display' id='dataDisplay'>Click Refresh Data to load current sensor readings...</div>";
  html += "</div></div>";
  
  html += "<script>";
  html += "function updateIndicator(id, active) { const indicator = document.getElementById(id); indicator.className = 'indicator ' + (active ? 'active' : 'inactive'); }";
  html += "function startSystem() { fetch('/start').then(response => response.json()).then(data => { alert('System started successfully!'); refreshStatus(); }).catch(error => alert('Error starting system: ' + error)); }";
  html += "function stopSystem() { fetch('/stop').then(response => response.json()).then(data => { alert('System stopped successfully!'); refreshStatus(); }).catch(error => alert('Error stopping system: ' + error)); }";
  html += "function refreshData() { fetch('/data').then(response => response.json()).then(data => { document.getElementById('dataDisplay').textContent = JSON.stringify(data, null, 2); }).catch(error => { document.getElementById('dataDisplay').textContent = 'Error loading data: ' + error; }); }";
  html += "function refreshStatus() { fetch('/status').then(response => response.json()).then(data => { document.getElementById('systemStatus').textContent = data.running ? 'Running' : 'Stopped'; document.getElementById('gpsStatus').textContent = data.gps_initialized ? 'Ready' : 'Error'; document.getElementById('accelStatus').textContent = data.accel_initialized ? 'Ready' : 'Error'; document.getElementById('clientsStatus').textContent = data.connected_clients; updateIndicator('systemIndicator', data.running); updateIndicator('gpsIndicator', data.gps_initialized); updateIndicator('accelIndicator', data.accel_initialized); updateIndicator('clientsIndicator', data.connected_clients > 0); }).catch(error => console.error('Error loading status:', error)); }";
  html += "function checkOTAProgress() { fetch('/otaprogress').then(response => response.json()).then(data => { const otaStatus = document.getElementById('otaStatus'); const otaProgress = document.getElementById('otaProgress'); const otaProgressText = document.getElementById('otaProgressText'); if (data.in_progress) { otaStatus.style.display = 'block'; otaProgress.style.width = data.progress + '%'; otaProgressText.textContent = data.progress + '%'; } else { otaStatus.style.display = 'none'; } }); }";
  html += "setInterval(refreshStatus, 3000);";
  html += "setInterval(function() { fetch('/status').then(response => response.json()).then(data => { if (data.running) { refreshData(); } }); }, 2000);";
  html += "setInterval(checkOTAProgress, 1000);";
  html += "refreshStatus(); refreshData();";
  html += "</script></body></html>";
  
  return html;
}

String generateUpdateHTML() {
  String html = "<!DOCTYPE html><html><head><title>TracX-1b OTA Update</title>";
  html += "<style>body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; background: #f5f5f5; }";
  html += ".update-form { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }";
  html += "input[type='file'] { width: 100%; padding: 10px; margin: 20px 0; border: 2px dashed #ccc; border-radius: 5px; }";
  html += "input[type='submit'] { background: #007bff; color: white; padding: 12px 30px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }";
  html += "input[type='submit']:hover { background: #0056b3; }";
  html += ".warning { background: #fff3cd; color: #856404; padding: 15px; border-radius: 5px; margin-bottom: 20px; }";
  html += ".connection-info { background: #d4edda; color: #155724; padding: 15px; border-radius: 5px; margin-bottom: 20px; }";
  html += ".progress-bar { width: 100%; height: 20px; background: #e9ecef; border-radius: 10px; overflow: hidden; margin: 10px 0; }";
  html += ".progress-fill { height: 100%; background: #28a745; width: 0%; transition: width 0.3s ease; }";
  html += ".upload-status { padding: 15px; border-radius: 5px; margin: 10px 0; display: none; }";
  html += "</style></head><body>";
  html += "<div class='update-form'><h1>OTA Update</h1>";
  html += "<div class='warning'><strong>Warning:</strong> Only upload firmware files (.bin) from trusted sources. The system will restart after successful upload.</div>";
  html += "<div class='upload-status' id='uploadStatus'>";
  html += "<strong>Upload in Progress...</strong><br>";
  html += "<div class='progress-bar'><div class='progress-fill' id='uploadProgress'></div></div>";
  html += "<span id='uploadProgressText'>0%</span>";
  html += "</div>";
  html += "<form method='POST' action='/update' enctype='multipart/form-data' id='uploadForm'>";
  html += "<label for='update'>Select Firmware File (.bin):</label>";
  html += "<input type='file' name='update' id='update' accept='.bin'>";
  html += "<input type='submit' value='Update Firmware' id='submitBtn'></form>";
  html += "<p><a href='/'> Back to Control Panel</a></p></div>";
  html += "<script>";
  html += "document.getElementById('uploadForm').addEventListener('submit', function(e) {";
  html += "document.getElementById('uploadStatus').style.display = 'block';";
  html += "document.getElementById('submitBtn').disabled = true;";
  html += "var progressInterval = setInterval(function() {";
  html += "fetch('/otaprogress').then(response => response.json()).then(data => {";
  html += "document.getElementById('uploadProgress').style.width = data.progress + '%';";
  html += "document.getElementById('uploadProgressText').textContent = data.progress + '%';";
  html += "if (!data.in_progress && data.progress == 100) { clearInterval(progressInterval); alert('Upload completed! System will restart.'); }";
  html += "}); }, 500); });";
  html += "</script></body></html>";
  return html;
}