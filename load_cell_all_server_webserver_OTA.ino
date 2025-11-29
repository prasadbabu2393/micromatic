
#include<HardwareSerial.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h> // NEW: For web routing and OTA
#include <Update.h>    // NEW: For OTA firmware updates
#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

// Adafruit_MLX90614 mlx = Adafruit_MLX90614();
Preferences preferences;

#define RXD2 16  //esp32 Rx for stm32
#define TXD2 17  //esp32 Tx for stm32

//pins: 12 14
const int HX711_dout = 14; //mcu > HX711 dout pin
const int HX711_sck = 12; //mcu > HX711 sck pin
//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int LVDT_pin = 35;  // Analog pin connected to LVDT output
const int limit_switch = 26;  // limit switch pin

unsigned long t = 0;
int stm_brake_status = 0;
int prev_weight=0;

// Debounce variables for limit switch
int limit_switch_state = HIGH;           // Current debounced state
int limit_switch_last_reading = HIGH;    // Last raw reading
unsigned long limit_switch_last_change = 0;  // Last time reading changed
const unsigned long DEBOUNCE_DELAY = 200;    // 200ms debounce time


String header = "";
WebServer server(80); // CHANGED: Replaced WiFiServer with WebServer
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000; // Unused with WebServer, kept for context

String ssid_h = "Tribometer_#07";  // hotpot name and password 
const char* password_h = "12345678";    

boolean newDataReady = 0;
float weight = 0;
bool SAMPLE = 0;
float calibrationValue; // calibration value 
float dividing_factor; // calibration value 
float LVDT_voltage;       // Variable to store the voltage reading
float cof = 0;
float meter; //diamter of disc of encoder is 96mm
float pulse_per_mm = 1.956;
int32_t number ;
uint8_t RxData[10];
// Removed: uint8_t a1 - no longer needed with fixed buffer read
int brake_count = 0;
String password_1, network_1, pulses, wifi_name;

// HTML for OTA update form
const char* html_ota = R"rawliteral(
<!DOCTYPE html><html><head><title>OTA Update</title><style>body{font-family:sans-serif;text-align:center;}</style></head>
<body><h2>Firmware Update</h2>
<form method='POST' action='#' enctype='multipart/form-data'>
<input type='file' name='update'><input type='submit' value='Update'></form>
</body></html>
)rawliteral";

// this function for taking count of A,B from sm32 and converting received bytes into integer(+/-)
int32_t fromBytesToSignedInt(uint8_t* byteArray) {
  return (int32_t)(byteArray[3] | 
                   (byteArray[2] << 8) | 
                   (byteArray[1] << 16) | 
                   (byteArray[0] << 24));
}

void handleDataRoute() {

   // Calculate LVDT voltage
    LVDT_voltage = (analogRead(LVDT_pin) / 4096.0) * 3.3;  // Convert to voltage for esp32 (4095)
    //requestBrakeStatus();

    // FIX: Pre-allocate String memory to prevent fragmentation
    String response;
    response.reserve(150); // Reserve enough space for the entire JSON

    // Build the JSON-like response string
    response = "{";
    response += String(weight/dividing_factor); // cof
    response += "," + String(meter); // meter
    response += "," + String(limit_switch_state); // limit_switch status (debounced)
  //  response += "," + String(stm_brake_status); // limit_switch status
    response += "," + String(brake_count); // brake_count
    response += "," + String(0); // mlx.readObjectTempC()
    response += "," + String(0); // bme.readTemperature()
    response += "," + String(0); // bme.readPressure() / 100.0F
    response += "," + String(0); // bme.readAltitude(SEALEVELPRESSURE_HPA)
    response += "," + String(0); // bme.readHumidity()  61038.00

    response += "," + String(weight);
    response += "," + String(LVDT_voltage); // LVDT voltage
    response += "}";

    server.send(200, "application/json", response);
}

void handleStart() {
    uint8_t x1 = 0; Serial2.write(x1); // sending 0 to stm32 to make A,B count = 0 //62017.00
    LoadCell.tareNoDelay();
    delay(200);
        // check if last tare operation is complete:
    while (LoadCell.getTareStatus() == true) { LoadCell.tareNoDelay(); delay(200); } 
    brake_count = 0;
    uint8_t b1 =1; Serial2.write(b1); // sending 1 to stm32 for reset 
    SAMPLE = 1;
    server.send(200, "text/plain", "Sampling and Tare Started.");
}

void handleStop() {
    SAMPLE = 0;
    server.send(200, "text/plain", "Sampling Stopped.");
}

void handleBreak() {
    prev_weight=weight;
    server.send(200, "text/plain", "Brake Applied.");
}

void handleLoginRoute() {
    // FIX: Pre-allocate String memory to prevent fragmentation
    String html;
    html.reserve(800); // Reserve enough space for the entire HTML

    // Original HTML content for the login/settings page
    html = "<!DOCTYPE html><html><body><form action=\"/action_page.php\">";

    html += "<label for=\"load_value\">load_value  : </label>";
    html += " <input type=\"text\" id=\"load_value\" name=\"load_value\"><br><br>";

    html += "<label for=\"dividing_factor\">dividing_factor: </label>";
    html += "<input type=\"text\" id=\"dividing_factor\" name=\"dividing_factor\"><br><br>";

    html += "<label for=\"pulse_per_mm\">pulse_per_mm: </label>";
    html += "<input type=\"text\" id=\"pulse_per_mm\" name=\"pulse_per_mm\"><br><br>";

    html += "<label for=\"wifi_name\">wifi_name: </label>";
    html += "<input type=\"text\" id=\"wifi_name\" name=\"wifi_name\"><br><br>";

    html += "<input type=\"submit\" value=\"Submit\">";
    html += "</form>";

    html += "<p>Click on the submit button to submit the form.</p></body></html>";

    html += "calibrationValue:" + String(calibrationValue) + "\n";
    html += " dividing_factor:" + String(dividing_factor) + "\n";
    html += " pulse_per_mm:" + String(pulse_per_mm) + "\n";
    html += " wifi_name:" + ssid_h + "\n";

    server.send(200, "text/html", html);
}

void handleActionPage() {
    // Extracting parameters from GET request arguments
    network_1 = server.arg("load_value");
    password_1 = server.arg("dividing_factor");
    pulses = server.arg("pulse_per_mm");
    wifi_name = server.arg("wifi_name");

    if (network_1 != "") {
        preferences.putString("ssid", network_1);
    }
    if (password_1 != "") {
        preferences.putString("password", password_1);
    }
    if (pulses != "") {
        preferences.putString("pulses", pulses);
    }
    if (wifi_name != "") {
        preferences.putString("wifi_name", wifi_name);
    }
    // FIX: Removed preferences.end() - it should stay open since we opened it in setup()
    // The ESP will restart anyway, so no need to close it

    server.send(200, "text/plain", "Credentials Saved. Rebooting...");
    delay(500); // Give time for response to be sent
    ESP.restart();
}

// OTA HANDLERS
void handleUpdateForm() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", html_ota);
}

void handleUpdateProcess() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { 
            Serial.printf("Update Success: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

void handleUpdateFinished() {
    if (Update.hasError()) {
        server.send(400, "text/plain", "Update failed.");
    } else {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", "Update successful! Rebooting...");
        ESP.restart();
    }
}

void setup() 
{
    Serial.begin(115200);
    Serial2.begin(19200, SERIAL_8N1, RXD2, TXD2);  // uart_begin
    pinMode(LVDT_pin, INPUT); // analog pin of esp32 for battery
    pinMode(limit_switch, INPUT_PULLUP); // brake status pin (0, 1)
    
    preferences.begin("memory", false);

    ssid_h = preferences.getString("wifi_name", ssid_h);
    dividing_factor = preferences.getString("password", "4").toFloat();
    pulse_per_mm = preferences.getString("pulses", "1.956").toFloat();
    calibrationValue =  preferences.getString("ssid", "67286.00").toFloat();

    LoadCell.begin();
    LoadCell.setCalFactor(calibrationValue); 

    unsigned long stabilizingtime = 2000;
    boolean _tare = true; 
    LoadCell.start(stabilizingtime, _tare);

    if (LoadCell.getTareTimeoutFlag()) {
      //Serial.println("Timeout, check MCU>HX711 wiring and pin designations");  
      //while (1);
    }

    // ///////// sensors set up (commented out in original code, keeping them commented)
    
    // --- WebServer Route Configuration ---

    // 1. DATA AND CONTROL ROUTES (Existing Functionality)
    server.on("/data", HTTP_GET, handleDataRoute);
    server.on("/start", HTTP_GET, handleStart);
    server.on("/stop", HTTP_GET, handleStop);
    // server.on("/brake", HTTP_GET, handleBreak);
    server.on("/login", HTTP_GET, handleLoginRoute);
    server.on("/action_page.php", HTTP_GET, handleActionPage); // For saving settings

    // 2. OTA UPDATE ROUTES (NEW Functionality)
    // GET /update: Serve the HTML form
    server.on("/update", HTTP_GET, handleUpdateForm);
    // POST /update: Handle the file upload and finish the process
    server.on("/update", HTTP_POST, handleUpdateFinished, handleUpdateProcess);

    // Default handler for 404
    server.onNotFound([]() {
      server.send(404, "text/plain", "Not Found");
    });
    
    // Starting the Access point and Server
    WiFi.softAP(ssid_h, password_h); 
    server.begin(); 
}

void loop()
{
    // The WebServer handles all client requests and routing here
    server.handleClient(); // NEW: Replaces the call to server_code()

    // Debounce limit switch reading (no delay, using millis)
    int current_reading = digitalRead(limit_switch);

    // If reading changed, reset the debounce timer
    if (current_reading != limit_switch_last_reading) {
        limit_switch_last_change = millis();
        limit_switch_last_reading = current_reading;
    }

    // If reading has been stable for DEBOUNCE_DELAY, update the state
    if ((millis() - limit_switch_last_change) > DEBOUNCE_DELAY) {
        limit_switch_state = current_reading;
    }

    //taking A,B count bytes from stm32
    if (Serial2.available() >= 4) //without brake status from stm32
    //if (Serial2.available() >= 5) ///with brake status from stm32
    {
        // FIX: Only read exactly 4 bytes to prevent buffer overflow
        Serial2.readBytes(RxData, 4); // Safe: only read 4 bytes into RxData[10]
        number = fromBytesToSignedInt(RxData);
        meter = number/pulse_per_mm ;     // disc full rotation mean 600 pulses will come(0.5026 per pulse) //2.933;
        // Extract brake status (5th byte)
       // stm_brake_status = RxData[4];  //with brake status from stm32

        // FIX: Clear any excess bytes to prevent buffer buildup
        while(Serial2.available() > 0) {
            Serial2.read(); // Flush remaining bytes
        }
    } 
    
  // check for new data/start next conversion:
  while (!LoadCell.update()){} newDataReady = true;

  // get smoothed value from the dataset:
  if(SAMPLE == 1)
  {
  if (newDataReady) {
    if (millis() > t ) {
      weight = LoadCell.getData();
    //   weight = LoadCell.getData()-prev_weight;
      newDataReady = 0;
      t = millis();
    }
  }
  }
}

// NOTE: The original server_code() function is entirely removed, as its logic is now split 
// into the dedicated handleUpdateForm, handleUpdateProcess, handleUpdateFinished, handleDataRoute, etc., functions.