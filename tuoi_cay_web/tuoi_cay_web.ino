#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ----- Pin Definitions & Constants -----
#define DHTPIN D3
#define DHTTYPE DHT11
#define SOIL_PIN A0
#define PUMP_PIN D5
#define LIGHT_PIN D4

// ----- Initialize Hardware -----
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);

// ----- WiFi Credentials -----
const char* ssid = "hhhhh";
const char* password = "hunghaik5";

// ----- Global Variables -----
bool pumpState = false;
unsigned long lastSensorUpdate = 0;
const unsigned long sensorInterval = 2000;  // Update sensors every 2 seconds

// ----- Minimal CSS (served separately) -----
String styleCSS = R"(
body {
  font-family: Arial, sans-serif;
  text-align: center;
  margin: 0;
  padding: 20px;
  background-color: #f2f2f2;
}

h2 {
  margin: 20px 0;
}

button {
  padding: 15px 30px;
  font-size: 18px;
  margin-top: 20px;
  cursor: pointer;
  border: none;
  border-radius: 5px;
  background-color: #007bff;
  color: #fff;
}

button:hover {
  background-color: #0056b3;
}

p {
  margin: 10px 0;
}
)";

// ----- Serve the CSS -----
void handleCSS() {
  server.send(200, "text/css", styleCSS);
}

// ----- Read Sensors & Update LCD -----
void updateSensorData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilValue = analogRead(SOIL_PIN);
  soilValue = map(soilValue, 1024, 700, 0, 100);
  soilValue = constrain(soilValue, 0, 100);

  bool lightStatus = digitalRead(LIGHT_PIN);

  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);  // Show 1 decimal place
  lcd.print("C H:");
  lcd.print(humidity, 0);  // No decimal
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("S:");
  lcd.print(soilValue);  // Soil moisture
  lcd.print("% L:");
  lcd.print(lightStatus == 0 ? "H" : "L");  // Light: H (High) or L (Low)
}

// ----- Main Page (Root) -----
void handleRoot() {
  // Read current values for the webpage
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilValue = analogRead(SOIL_PIN);
  soilValue = map(soilValue, 1024, 300, 0, 100);
  soilValue = constrain(soilValue, 0, 100);

  bool lightStatus = digitalRead(LIGHT_PIN);

  // Build HTML
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<link rel='stylesheet' type='text/css' href='/style.css'>";
  // Add the meta refresh tag for auto-refreshing
  html += "<meta http-equiv='refresh' content='2'>";  // Refresh every 2 seconds
  html += "</head><body>";
  html += "<h2>ESP8266 Plant Monitor</h2>";
  html += "<p><strong>Temperature:</strong> " + String(temperature) + " &deg;C</p>";
  html += "<p><strong>Humidity:</strong> " + String(humidity) + " %</p>";
  html += "<p><strong>Soil Moisture:</strong> " + String(soilValue) + " %</p>";
  html += "<p><strong>Light Status:</strong> " + String((lightStatus == 0) ? "High" : "Low") + "</p>";
  html += "<p><strong>Pump:</strong> " + String(pumpState ? "OFF" : "ON") + "</p>";

  // Pump toggle button
  if (pumpState) {
    html += "<a href='/pump?state=off'><button>Turn Pump ON</button></a>";
  } else {
    html += "<a href='/pump?state=on'><button>Turn Pump OFF</button></a>";
  }

  html += "</body></html>";

  server.send(200, "text/html", html);
}

// ----- Pump Control -----
void handlePump() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "on") {
      digitalWrite(PUMP_PIN, HIGH);
      pumpState = true;
      Serial.println("Pump turned ON");
    } else if (state == "off") {
      digitalWrite(PUMP_PIN, LOW);
      pumpState = false;
      Serial.println("Pump turned OFF");
    }
  }
  // Redirect back to main page
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// ----- Setup -----
void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
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

  // Start server
  server.on("/", handleRoot);
  server.on("/pump", handlePump);
  server.on("/style.css", handleCSS);  // <-- Serve the CSS from here
  server.begin();
  Serial.println("HTTP server started");
}

// ----- Loop -----
void loop() {
  // Handle incoming requests
  server.handleClient();

  // Update sensors only every 'sensorInterval' ms (non-blocking)
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorUpdate >= sensorInterval) {
    updateSensorData();
    lastSensorUpdate = currentMillis;
  }
}
