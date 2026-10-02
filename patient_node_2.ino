#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include "MAX30105.h"  
#include "heartRate.h" 

// Wi-Fi credentials
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

// GPIO where the DS18B20 is connected
const int oneWireBus = 15;  // Change this if using a different GPIO pin

// Setup a OneWire instance to communicate with any OneWire device
OneWire oneWire(oneWireBus);

// Pass the OneWire reference to the DallasTemperature library
DallasTemperature sensors(&oneWire);

// Create server object on port 80
WiFiServer server(80);

// Static IP configuration
IPAddress staticIP(192, 168, 0, 20);  // Static IP address
IPAddress gateway(192, 168, 0, 1);    // Gateway IP address
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress dns(8, 8, 8, 8);            // DNS server

// MAX30102 sensor object
MAX30105 particleSensor;

// Variables to store MAX30102 data
float heartRate = 0.0;
float spo2 = 0.0;

// Buffer for heart rate calculation
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];    
byte rateSpot = 0;
long lastBeat = 0;

// Variables for SpO2 calculation
double avered = 0;
double aveir = 0;
double sumirrms = 0;
double sumredrms = 0;
int i = 0;
int Num = 100;

String evaluateVitalAlerts(float temp, float hr, float spo2) {
  String alerts = "";

  if (temp > 38.0) {
    alerts += "High temperature; ";
  } else if (temp < 35.0) {
    alerts += "Low temperature; ";
  }

  if (hr > 100) {
    alerts += "High heart rate; ";
  } else if (hr > 0 && hr < 60) {
    alerts += "Low heart rate; ";
  }

  if (spo2 > 0 && spo2 < 95) {
    alerts += "Low oxygen saturation; ";
  }

  if (alerts.length() == 0) {
    return "No threshold alert";
  }

  alerts.remove(alerts.length() - 2);
  return alerts;
}

void setup() {
  Serial.begin(115200);
  sensors.begin();

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 sensor not found.");
    while (1);
  }
  Serial.println("MAX30102 sensor initialized.");

  byte ledBrightness = 0x1F;
  byte sampleAverage = 4;
  byte ledMode = 2;
  int sampleRate = 400;
  int pulseWidth = 411;
  int adcRange = 4096;

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  if (!WiFi.config(staticIP, gateway, subnet, dns)) {
    Serial.println("Failed to configure static IP!");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void handleDataRequest(WiFiClient &client) {
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  if (checkForBeat(irValue) == true) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    heartRate = 60 / (delta / 1000.0);

    if (heartRate < 255 && heartRate > 20) {
      rates[rateSpot++] = (byte)heartRate;
      rateSpot %= RATE_SIZE;

      float average = 0;
      for (byte x = 0; x < RATE_SIZE; x++)
        average += rates[x];
      average /= RATE_SIZE;
      heartRate = average;
    }
  }

  avered = avered * 0.8 + redValue * 0.2;
  aveir = aveir * 0.8 + irValue * 0.2;
  sumredrms += (redValue - avered) * (redValue - avered);
  sumirrms += (irValue - aveir) * (irValue - aveir);

  if ((i % Num) == 0) {
    double R = (sqrt(sumredrms / Num) / avered) / (sqrt(sumirrms / Num) / aveir);
    spo2 = -23.3 * (R - 0.4) + 100;
    sumredrms = 0;
    sumirrms = 0;
    i = 0;
  }
  i++;

  String predictedDiseases = evaluateVitalAlerts(temperatureC, heartRate, spo2);

  String jsonResponse = "{";
  jsonResponse += "\"temperature\":" + String(temperatureC) + ",";
  jsonResponse += "\"heartRate\":" + String(heartRate) + ",";
  jsonResponse += "\"spo2\":" + String(spo2) + ",";
  jsonResponse += "\"diseases\":\"" + predictedDiseases + "\"";
  jsonResponse += "}";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println(jsonResponse);
}

void loop() {
  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  if (checkForBeat(irValue) == true) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    heartRate = 60 / (delta / 1000.0);

    if (heartRate < 255 && heartRate > 20) {
      rates[rateSpot++] = (byte)heartRate;
      rateSpot %= RATE_SIZE;

      float average = 0;
      for (byte x = 0; x < RATE_SIZE; x++)
        average += rates[x];
      average /= RATE_SIZE;
      heartRate = average;
    }
  }

  avered = avered * 0.8 + redValue * 0.2;
  aveir = aveir * 0.8 + irValue * 0.2;
  sumredrms += (redValue - avered) * (redValue - avered);
  sumirrms += (irValue - aveir) * (irValue - aveir);

  if ((i % Num) == 0) {
    double R = (sqrt(sumredrms / Num) / avered) / (sqrt(sumirrms / Num) / aveir);
    spo2 = -23.3 * (R - 0.4) + 100;
    sumredrms = 0;
    sumirrms = 0;
    i = 0;
  }
  i++;

  WiFiClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        String request = client.readStringUntil('\r');
        if (request.indexOf("GET /data") >= 0) {
          handleDataRequest(client);
        } else {
          String htmlResponse = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>Patient 2</title>
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #1e3c72, #2a5298);
      color: #fff;
      margin: 0;
      padding: 0;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      overflow: hidden;
    }
    .sensor-container {
      background: rgba(255, 255, 255, 0.1);
      backdrop-filter: blur(10px);
      border-radius: 20px;
      padding: 40px;
      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.2);
      text-align: center;
      max-width: 400px;
      width: 100%;
      animation: fadeIn 1.5s ease-in-out;
    }
    h1 {
      font-size: 2.5rem;
      margin-bottom: 20px;
      color: #fff;
      text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
    }
    p {
      font-size: 1.4rem;
      margin: 15px 0;
      color: #e0e0e0;
    }
    .value {
      font-weight: bold;
      color: #ff9f43;
      text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.3);
    }
    @keyframes fadeIn {
      from { opacity: 0; transform: translateY(-20px); }
      to { opacity: 1; transform: translateY(0); }
    }
    body::before {
      content: '';
      position: absolute;
      top: -50%;
      left: -50%;
      width: 200%;
      height: 200%;
      background: radial-gradient(circle, rgba(255, 255, 255, 0.1) 10%, transparent 10.01%);
      background-size: 20px 20px;
      animation: moveBackground 10s linear infinite;
      z-index: -1;
    }
    @keyframes moveBackground {
      from { transform: translate(0, 0); }
      to { transform: translate(20px, 20px); }
    }
    .alert {
      color: #ff7675;
      font-weight: bold;
      animation: pulse 1.5s infinite;
    }
    @keyframes pulse {
      0% { opacity: 1; }
      50% { opacity: 0.5; }
      100% { opacity: 1; }
    }
  </style>
</head>
<body>
  <div class="sensor-container">
    <h1>Patient Monitoring</h1>
    <p>Name: <span class="value">Patient 2</span></p>
    <p>Gender: <span class="value">Demo</span></p>
    <p>Age: <span class="value">Demo</span></p>
    <p>Blood Type: <span class="value">Demo</span></p> 
    <p>Temperature: <span class="value" id="temperature">Loading...</span></p>
    <p>Heart Rate: <span class="value" id="heartRate">Loading...</span></p>
    <p>SpO2: <span class="value" id="spo2">Loading...</span></p>
    <p>Vital Alert: <span class="value alert" id="diseases">Waiting for sensor data</span></p>
  </div>

  <script>
    async function fetchSensorData() {
      try {
        const response = await fetch('/data');
        const data = await response.json();
        document.getElementById('temperature').textContent = data.temperature + ' °C';
        document.getElementById('heartRate').textContent = data.heartRate + ' BPM';
        document.getElementById('spo2').textContent = data.spo2 + ' %';
        document.getElementById('diseases').textContent = data.diseases;
        
        // Add alert class if any critical issues detected
        if (!data.diseases.includes('No critical')) {
          document.getElementById('diseases').classList.add('alert');
        } else {
          document.getElementById('diseases').classList.remove('alert');
        }
      } catch (error) {
        console.error('Error fetching data:', error);
      }
    }

    setInterval(fetchSensorData, 1000);
    fetchSensorData();
  </script>
</body>
</html>
)=====";

          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          client.println(htmlResponse);
        }
        client.stop();
      }
    }
  }
}
