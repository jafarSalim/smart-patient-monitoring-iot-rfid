#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

// Server IP addresses
const char *serverIP1 = "192.168.0.10";  // IP for the first card
const char *serverIP2 = "192.168.0.20";  // IP for the second card

// Static IP configuration
IPAddress staticIP(192, 168, 0, 30);  // Static IP address for the ESP32
IPAddress gateway(192, 168, 0, 100);  // Gateway IP address (usually the router's IP)
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress dns(8, 8, 8, 8);            // DNS server (Google's public DNS)

// RFID pins
#define RST_PIN    22    // Reset pin connected to GPIO22
#define SS_PIN     21    // Slave Select (SDA) pin connected to GPIO21

// Create an instance of the MFRC522 class
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Define the known card UID values
byte uid1[] = {0xDE, 0xAD, 0xBE, 0x01};  // Replace with Patient 1 RFID UID
byte uid2[] = {0xDE, 0xAD, 0xBE, 0x02};  // Replace with Patient 2 RFID UID

// Create a web server object on port 80
WebServer server(80);

// Global variable to store the last scanned card UID
byte lastCardUID[4] = {0};
byte lastCardUIDSize = 0;

// Function prototypes
bool compareUID(byte *uid1, byte *uid2, byte length);
void setLastCardUID(byte *uid, byte size);
const char* getServerIPForLastCard();
String fetchData(const char *serverIP);
String extractValueFromHTML(String html, String key);

void setup() {
  // Start the Serial Monitor
  Serial.begin(115200);

  // Configure static IP address
  if (!WiFi.config(staticIP, gateway, subnet, dns)) {
    Serial.println("Failed to configure static IP!");
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize SPI and MFRC522
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Scan a card");

  // Define the route for the root web page
  server.on("/", HTTP_GET, []() {
    Serial.println("Received request for root page");

    // Serve the HTML page
    String htmlResponse = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Patient Data</title>
  <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
  <style>
    body {
      background-color: #f8f9fa;
      font-family: 'Arial', sans-serif;
    }
    .card {
      margin: 20px auto;
      max-width: 600px;
      box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
    }
    .card-header {
      background-color: #007bff;
      color: white;
      font-size: 1.5rem;
    }
    .card-body {
      padding: 20px;
    }
    .data-item {
      margin-bottom: 10px;
      font-size: 1.1rem;
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="card-header text-center">
      Patient Data
    </div>
    <div class="card-body" id="patient-data">
      <p class='text-center'>No card recognized. Please scan a valid card.</p>
    </div>
  </div>
  <script>
    function fetchData() {
      fetch('/data')
        .then(response => response.text())
        .then(data => {
          document.getElementById('patient-data').innerHTML = data;
        })
        .catch(error => console.error('Error fetching data:', error));
    }

    // Fetch data every 1 second
    setInterval(fetchData, 1000);
  </script>
</body>
</html>
)=====";

    // Send the HTML response to the client
    server.send(200, "text/html", htmlResponse);
    Serial.println("Sent HTML response");
  });

  // Define the route for fetching patient data
  server.on("/data", HTTP_GET, []() {
    Serial.println("Received request for patient data");

    // Fetch data based on the last scanned card
    const char *serverIP = getServerIPForLastCard();
    String data = fetchData(serverIP);

    // Send the data as the response
    server.send(200, "text/html", data);
    Serial.println("Sent patient data");
  });

  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle client requests for the web server
  server.handleClient();

  // Check for RFID card
  if (mfrc522.PICC_IsNewCardPresent()) {
    if (mfrc522.PICC_ReadCardSerial()) {
      Serial.println("Card Detected:");
      Serial.print("UID: ");

      // Print the UID of the detected card
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      // Check the UID and set the last card UID
      if (compareUID(mfrc522.uid.uidByte, uid1, sizeof(uid1))) {
        Serial.println("Name: p1");
        setLastCardUID(uid1, sizeof(uid1));
      } else if (compareUID(mfrc522.uid.uidByte, uid2, sizeof(uid2))) {
        Serial.println("Name: p2");
        setLastCardUID(uid2, sizeof(uid2));
      } else {
        Serial.println("Card not recognized");
      }

      mfrc522.PICC_HaltA();   // Halt the current card
      mfrc522.PCD_StopCrypto1();  // Stop encryption
    }
  }
}

// Function to compare two UID arrays
bool compareUID(byte *uid1, byte *uid2, byte length) {
  for (byte i = 0; i < length; i++) {
    if (uid1[i] != uid2[i]) {
      return false;
    }
  }
  return true;
}

// Function to set the last scanned card UID
void setLastCardUID(byte *uid, byte size) {
  memcpy(lastCardUID, uid, size);
  lastCardUIDSize = size;
}

// Function to get the server IP based on the last scanned card
const char* getServerIPForLastCard() {
  if (compareUID(lastCardUID, uid1, sizeof(uid1))) {
    return serverIP1;
  } else if (compareUID(lastCardUID, uid2, sizeof(uid2))) {
    return serverIP2;
  } else {
    return nullptr;  // No recognized card
  }
}

// Function to extract values from HTML
String extractValueFromHTML(String html, String key) {
  int startIndex = html.indexOf(key);
  if (startIndex == -1) {
    return ""; // Key not found
  }

  startIndex = html.indexOf(">", startIndex) + 1;
  int endIndex = html.indexOf("<", startIndex);
  return html.substring(startIndex, endIndex);
}

// Function to fetch data from the server
String fetchData(const char *serverIP) {
  if (serverIP == nullptr) {
    return "<p class='text-center'>No data available.</p>";
  }

  // Create an HTTP client
  HTTPClient http;

  // Fetch HTML response for static fields
  String htmlUrl = "http://" + String(serverIP) + "/";
  http.begin(htmlUrl);

  int httpCode = http.GET();  // Send the request

  // Check if the request was successful
  if (httpCode == HTTP_CODE_OK) {
    // Read the HTML response
    String htmlResponse = http.getString();
    Serial.println("Server HTML Response: " + htmlResponse); // Debug: Print server response

    // Extract static fields from HTML
    String name = extractValueFromHTML(htmlResponse, "Name:");
    String gender = extractValueFromHTML(htmlResponse, "Gender:");
    String age = extractValueFromHTML(htmlResponse, "Age:");
    String bloodType = extractValueFromHTML(htmlResponse, "Blood Type:");
    String disease = extractValueFromHTML(htmlResponse, "Health Status:");

    // Fetch JSON response for dynamic fields
    String jsonUrl = "http://" + String(serverIP) + "/data";
    http.begin(jsonUrl);

    httpCode = http.GET();  // Send the request

    if (httpCode == HTTP_CODE_OK) {
      // Read the JSON response
      String jsonResponse = http.getString();
      Serial.println("Server JSON Response: " + jsonResponse); // Debug: Print server response

      // Parse the JSON response
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, jsonResponse);

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return "<p class='text-center'>Failed to parse JSON.</p>";
      }

      // Extract dynamic fields from JSON
      float temperature = doc["temperature"];
      int heartRate = doc["heartRate"];
      int spo2 = doc["spo2"];

      // Format the UID of the last scanned card
      String cardUID = "";
      for (byte i = 0; i < lastCardUIDSize; i++) {
        cardUID += String(lastCardUID[i], HEX);
        if (i < lastCardUIDSize - 1) {
          cardUID += ":";
        }
      }

      // Format the data into HTML
      String dataHtml = "<div class='data-item'><strong>Card UID:</strong> " + cardUID + "</div>";
      dataHtml += "<div class='data-item'><strong>Server IP:</strong> " + String(serverIP) + "</div>";
      dataHtml += "<div class='data-item'><strong>Name:</strong> " + name + "</div>";
      dataHtml += "<div class='data-item'><strong>Age:</strong> " + age + "</div>";
      dataHtml += "<div class='data-item'><strong>Gender:</strong> " + gender + "</div>";
      dataHtml += "<div class='data-item'><strong>Blood Type:</strong> " + bloodType + "</div>";
      dataHtml += "<div class='data-item'><strong>Vital Alert:</strong> " + disease + "</div>";
      dataHtml += "<div class='data-item'><strong>Temperature:</strong> " + String(temperature) + " °C</div>";
      dataHtml += "<div class='data-item'><strong>Heart Rate:</strong> " + String(heartRate) + " bpm</div>";
      dataHtml += "<div class='data-item'><strong>SpO2:</strong> " + String(spo2) + "%</div>";

      return dataHtml;
    } else {
      Serial.println("Failed to fetch JSON data! HTTP Code: " + String(httpCode));
      return "<p class='text-center'>Failed to fetch dynamic data.</p>";
    }
  } else {
    Serial.println("Failed to fetch HTML data! HTTP Code: " + String(httpCode));
    return "<p class='text-center'>Failed to fetch static data.</p>";
  }

  // Close the connection
  http.end();
}
