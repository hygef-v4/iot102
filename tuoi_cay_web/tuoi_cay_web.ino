#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ### Pin Definitions & Constants ###
#define DHTPIN D3          // DHT11 sensor pin
#define DHTTYPE DHT11      // DHT11 sensor type
#define SOIL_PIN A0        // Soil moisture sensor pin
#define PUMP_PIN D5        // Water pump control pin
#define LIGHT_PIN D4       // Light sensor pin

// ### Initialize Hardware ###
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD I2C address may vary (e.g., 0x3F)
DHT dht(DHTPIN, DHTTYPE);            // DHT sensor instance
ESP8266WebServer server(80);         // Web server on port 80

// ### WiFi Credentials ###
const char* ssid = "POCO F4";          // Replace with your WiFi SSID
const char* password = "kurogane123";  // Replace with your WiFi password

// ### Global Variables ###
bool pumpState = false;              // True when pump is ON (LOW)
bool autoMode = false;               // Automatic watering mode
const int soilThreshold = 30;        // Soil moisture threshold (%)
unsigned long pumpStartTime = 0;     // Time when pump turned on
const unsigned long pumpDuration = 5000;  // Pump runs for 5 seconds
unsigned long lastSensorUpdate = 0;  // Last sensor update time
const unsigned long sensorInterval = 2000;  // Update sensors every 2 seconds

// We won't rely on the old CSS string, but let's keep a minimal one for any overrides
String styleCSS = R"(
/* You can place any small custom overrides here if needed. */
)";

// ### Serve CSS (optional) ###
void handleCSS() {
  server.send(200, "text/css", styleCSS);
}

// ### Read Sensors & Update LCD ###
void updateSensorData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilValue = analogRead(SOIL_PIN);
  soilValue = map(soilValue, 0, 1024, 0, 100);  
  soilValue = (soilValue - 100) * -1;      

  bool lightStatus = digitalRead(LIGHT_PIN);

  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C H:");
  lcd.print(humidity, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("S:");
  lcd.print(soilValue);
  lcd.print("% L:");
  lcd.print(lightStatus == 0 ? "H" : "L");
}
String buildFancyBar(float value, const String& color) {
  // `value` expected in [0..100].
  // We'll display it as a horizontal bar with text in the center.
  String barHtml = "<div class='bar-container'>"
                   "  <div class='bar-fill' style='background:" + color + "; width:" + String(value, 1) + "%;'></div>"
                   "  <div class='bar-label'>" + String(value, 1) + "%</div>"
                   "</div>";
  return barHtml;
}


// ### Main Page ###
void handleRoot() {
  // --- Read sensor data ---
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilValue = analogRead(SOIL_PIN);
  soilValue = map(soilValue, 0, 1024, 0, 100);  
  soilValue = (soilValue - 100) * -1;  
  bool lightStatus = digitalRead(LIGHT_PIN);

  // --- Begin building HTML ---
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'>";

  // --- Bootstrap CSS from CDN ---
  html += "<link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css'>";

  // --- Font Awesome for icons ---
  html += "<link rel='stylesheet' href='https://use.fontawesome.com/releases/v5.15.4/css/all.css'>";

  // --- Auto-refresh every 2 seconds ---
  html += "<meta http-equiv='refresh' content='2'>";

  // --- Inline custom CSS (bars & fancy buttons) ---
  html += "<style>";

  // Remove underline from links
  html += "a { text-decoration: none; } a:hover { text-decoration: none; }";

  // 1) Custom bar styles
  html += ".bar-container {"
          "  position: relative;"
          "  width: 100%;"
          "  height: 30px;"
          "  background: #e0e0e0;"
          "  border-radius: 5px;"
          "  overflow: hidden;"
          "  margin: 0 auto;"
          "}";
  html += ".bar-fill {"
          "  position: absolute;"
          "  left: 0; top: 0; bottom: 0;"
          "  width: 0%;"
          "  transition: width 0.5s ease-in-out;"
          "}";
  html += ".bar-label {"
          "  position: absolute;"
          "  width: 100%;"
          "  text-align: center;"
          "  font-weight: bold;"
          "  line-height: 30px;"
          "  color: #fff;"
          "}";

  // 2) Fancy gradient buttons (dark theme)
  html += ".btn-custom {"
          "  display: inline-block;"
          "  background: linear-gradient(45deg, #343a40, #000);"
          "  color: #ffffff;"
          "  border: none;"
          "  border-radius: 25px;"
          "  padding: 12px 30px;"
          "  font-size: 16px;"
          "  margin: 10px;"
          "  cursor: pointer;"
          "  box-shadow: 0 3px 6px rgba(0,0,0,0.1);"
          "  transition: transform 0.2s, background 0.3s;"
          "  text-decoration: none;"
          "}"
          ".btn-custom:hover {"
          "  background: linear-gradient(45deg, #23272b, #343a40);"
          "  transform: translateY(-2px);"
          "}"
          ".btn-custom:active {"
          "  transform: translateY(1px);"
          "}";
  html += "</style>";

  // --- Page Title ---
  html += "<title>Smart Plant Care</title></head>";

  // --- Body ---
  html += "<body class='bg-light'>";
  html += "<div class='container py-5'>";
  html += "  <h2 class='text-center mb-4'>Smart Plant Care</h2>";

  // Row of three cards: Temperature, Humidity, Soil Moisture
  html += "  <div class='row mb-4'>";

  // 1) Temperature card
  html += "    <div class='col-md-4'>";
  html += "      <div class='card mb-3'>";
  html += "        <div class='card-body text-center'>";
  html += "          <i class='fas fa-thermometer-half fa-2x mb-3 text-danger'></i>";
  html += "          <h5 class='card-title'>Temperature</h5>";
  html += "          <p class='display-4'>";
  if (isnan(temperature)) {
    html += "N/A &deg;C";
  } else {
    html += String(temperature, 1) + " &deg;C";
  }
  html += "          </p>";
  html += "        </div>";
  html += "      </div>";
  html += "    </div>";

  // 2) Humidity card with fancy bar
  html += "    <div class='col-md-4'>";
  html += "      <div class='card mb-3'>";
  html += "        <div class='card-body text-center'>";
  html += "          <h5 class='card-title'>Humidity</h5>";
  float safeHumidity = isnan(humidity) ? 0 : humidity;
  html += buildFancyBar(safeHumidity, "#17a2b8"); // teal color
  html += "        </div>";
  html += "      </div>";
  html += "    </div>";

  // 3) Soil Moisture card with fancy bar
  html += "    <div class='col-md-4'>";
  html += "      <div class='card mb-3'>";
  html += "        <div class='card-body text-center'>";
  html += "          <h5 class='card-title'>Soil Moisture</h5>";
  html += buildFancyBar(soilValue, "#28a745"); // green color
  html += "        </div>";
  html += "      </div>";
  html += "    </div>";

  html += "  </div>"; // end row

  // Row for Light Status, Pump, Auto Mode
  html += "  <div class='row mb-4'>";

  // Light Status
  html += "    <div class='col-md-4'>";
  html += "      <div class='card mb-3 text-center'>";
  html += "        <div class='card-body'>";
  html += "          <h5 class='card-title'>Light Status</h5>";
  html += "          <p class='card-text'>";
  html += (lightStatus == 0) 
            ? "<span class='text-warning font-weight-bold'>High</span>"
            : "<span class='text-muted font-weight-bold'>Low</span>";
  html += "          </p>";
  html += "        </div>";
  html += "      </div>";
  html += "    </div>";

  // Pump
  html += "    <div class='col-md-4'>";
  html += "      <div class='card mb-3 text-center'>";
  html += "        <div class='card-body'>";
  html += "          <h5 class='card-title'>Pump</h5>";
  html += "          <p class='card-text'>";
  html += pumpState
            ? "<span class='text-danger font-weight-bold'>ON</span>"
            : "<span class='text-muted font-weight-bold'>OFF</span>";
  html += "          </p>";
  html += "        </div>";
  html += "      </div>";
  html += "    </div>";

  // Auto Mode
  html += "    <div class='col-md-4'>";
  html += "      <div class='card mb-3 text-center'>";
  html += "        <div class='card-body'>";
  html += "          <h5 class='card-title'>Auto Mode</h5>";
  html += "          <p class='card-text'>";
  html += autoMode
            ? "<span class='text-success font-weight-bold'>ON</span>"
            : "<span class='text-secondary font-weight-bold'>OFF</span>";
  html += "          </p>";
  html += "        </div>";
  html += "      </div>";
  html += "    </div>";

  html += "  </div>"; // end row

  // Button group (Pump ON/OFF, Auto Mode ON/OFF)
  html += "  <div class='text-center'>";
  if (pumpState) {
    html += "    <a href='/pump?state=off' class='btn-custom'>Turn Pump OFF</a>";
  } else {
    html += "    <a href='/pump?state=on' class='btn-custom'>Turn Pump ON</a>";
  }
  if (autoMode) {
    html += "    <a href='/auto?state=off' class='btn-custom'>Turn Auto Mode OFF</a>";
  } else {
    html += "    <a href='/auto?state=on' class='btn-custom'>Turn Auto Mode ON</a>";
  }
  html += "  </div>"; // end button group

  html += "</div>"; // end container

  // --- Scripts (Bootstrap + jQuery + Popper) ---
  html += "<script src='https://code.jquery.com/jquery-3.5.1.slim.min.js'></script>";
  html += "<script src='https://cdn.jsdelivr.net/npm/popper.js@1.16.0/dist/umd/popper.min.js'></script>";
  html += "<script src='https://stackpath.bootstrapcdn.com/bootstrap/4.5.2/js/bootstrap.min.js'></script>";

  html += "</body></html>";

  // --- Send to client ---
  server.send(200, "text/html", html);
}






// ### Pump Control ###
void handlePump() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "on") {
      digitalWrite(PUMP_PIN, LOW);  // Pump ON
      pumpState = true;
      Serial.println("Pump turned ON");
    } else if (state == "off") {
      digitalWrite(PUMP_PIN, HIGH);  // Pump OFF
      pumpState = false;
      Serial.println("Pump turned OFF");
    }
  }
  // Redirect back to the main page
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// ### Auto Mode Control ###
void handleAuto() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "on") {
      autoMode = true;
      Serial.println("Auto Mode turned ON");
    } else if (state == "off") {
      autoMode = false;
      digitalWrite(PUMP_PIN, HIGH);  // Ensure pump is OFF
      pumpState = false;
      Serial.println("Auto Mode turned OFF and pump turned OFF");
    }
  }
  // Redirect back to the main page
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// ### Setup ###
void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH);  // Pump OFF on startup
  pumpState = false;

  pinMode(LIGHT_PIN, INPUT);
  dht.begin();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Web Interface: http://");
  Serial.println(WiFi.localIP());

  // Define server routes
  server.on("/", handleRoot);
  server.on("/pump", handlePump);
  server.on("/auto", handleAuto);
  server.on("/style.css", handleCSS);
  server.begin();
  Serial.println("HTTP server started");
}

// Add this at the top with your other global variables:
unsigned long lastAutoCheck = 0;  // For throttling auto mode checks

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  // Update sensor data (and LCD) every sensorInterval
  if (currentMillis - lastSensorUpdate >= sensorInterval) {
    updateSensorData();
    lastSensorUpdate = currentMillis;
  }

  // Throttle auto mode checks to once every sensorInterval
  if (autoMode && (currentMillis - lastAutoCheck >= sensorInterval)) {
    lastAutoCheck = currentMillis; // Update last auto check time
    
    // If pump is not running, check soil moisture to decide if pump should start
    if (!pumpState) {
      int soilValue = analogRead(SOIL_PIN);
      soilValue = map(soilValue, 0, 1024, 0, 100);  
      soilValue = (soilValue - 100) * -1;  
      if (soilValue < soilThreshold) {
        digitalWrite(PUMP_PIN, LOW);  // Pump ON
        pumpState = true;
        pumpStartTime = currentMillis;
        Serial.println("Auto Mode: Pump ON due to dry soil");
      }
    }
  }

  // Turn off pump after duration in Auto Mode
  if (pumpState && autoMode) {
    if (currentMillis - pumpStartTime >= pumpDuration) {
      Serial.println("Auto Mode: Pump OFF after duration");
      digitalWrite(PUMP_PIN, HIGH);  // Pump OFF
      pumpState = false;
    }
  }
}


