// Simple sketch for sending data to the TMEP.cz
// by mikrom (http://www.mikrom.cz)
// http://wiki.tmep.cz/doku.php?id=zarizeni:esp8266
//
// If you send only temperature url will be: http://ahoj.tmep.cz/?mojemereni=25.6 (domain is "ahoj" and guid is "mojemereni"
// If you send also humidity url will be: http://ahoj.tmep.cz/?mojemereni=25.6&humV=60
// but for humidity You will need DHT11 or DHT22 sensor and code will need some modifications
//
// Not tested, but should work! :)

#include <ESP8266WiFi.h> // WiFi library
#include "Adafruit_SHT4x.h"

Adafruit_SHT4x sht4 = Adafruit_SHT4x();

// Define settings
//const char* ssid    = "HomeZl"; // WiFi SSID
//const char* pass    = "Domov4770"; // WiFi password
const char* ssid    = "NewSWSfree"; // WiFi SSID
const char* domain  = "mps";    // domain.tmep.cz
const char* guid    = "temp";      // mojemereni
const long sleep    = 60;            // Sekundy



void setup() {
  // Start serial
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println("Adafruit SHT4x test");
  if (! sht4.begin()) {
    Serial.println("Couldn't find SHT4x");
    while (1) delay(1);
  }
  Serial.println("Found SHT4x sensor");
  Serial.print("Serial number 0x");
  Serial.println(sht4.readSerial(), HEX);

  // You can have 3 different precisions, higher precision takes longer
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  switch (sht4.getPrecision()) {
    case SHT4X_HIGH_PRECISION:
      Serial.println("High precision");
      break;
    case SHT4X_MED_PRECISION:
      Serial.println("Med precision");
      break;
    case SHT4X_LOW_PRECISION:
      Serial.println("Low precision");
      break;
  }
  // Connect to the WiFi
  Serial.print("Connecting to "); Serial.println(ssid);
  //WiFi.begin(ssid, pass);
  WiFi.begin(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  Serial.println();


  // You can have 6 different heater settings
  // higher heat and longer times uses more power
  // and reads will take longer too!
  sht4.setHeater(SHT4X_NO_HEATER);
  switch (sht4.getHeater()) {
    case SHT4X_NO_HEATER:
      Serial.println("No heater");
      break;
    case SHT4X_HIGH_HEATER_1S:
      Serial.println("High heat for 1 second");
      break;
    case SHT4X_HIGH_HEATER_100MS:
      Serial.println("High heat for 0.1 second");
      break;
    case SHT4X_MED_HEATER_1S:
      Serial.println("Medium heat for 1 second");
      break;
    case SHT4X_MED_HEATER_100MS:
      Serial.println("Medium heat for 0.1 second");
      break;
    case SHT4X_LOW_HEATER_1S:
      Serial.println("Low heat for 1 second");
      break;
    case SHT4X_LOW_HEATER_100MS:
      Serial.println("Low heat for 0.1 second");
      break;
  }
}

void loop() {

  sensors_event_t humidity, temp;


  uint32_t timestamp = millis();
  sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  timestamp = millis() - timestamp;


  Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");

  Serial.print("Read duration (ms): ");
  Serial.println(timestamp);

float t = temp.temperature;
  float h =humidity.relative_humidity;

  // Connect to the HOST and send data via GET method
  WiFiClient client; // Use WiFiClient class to create TCP connections

  char host[50];            // Joining two chars is little bit difficult. Make new array, 50 bytes long
  strcpy(host, domain);     // Copy /domain/ in to the /host/
  strcat(host, ".tmep.cz"); // Add ".tmep.cz" at the end of the /host/. /host/ is now "/domain/.tmep.cz"
  const int httpPort = 80;

  Serial.print("Connecting to "); Serial.println(host);
  if (!client.connect(host, httpPort)) {
    // If you didn't get a connection to the server
    Serial.println("Connection failed");
    return;
  }
  Serial.println("Client connected");

  // Make an url. We need: /?guid=t&humV=h
  String url = "/?";
  url += guid;
  url += "=";
  url += t;
  url += "&humV=";
  url += h;
  Serial.print("Requesting URL: "); Serial.println(url);

  // Make a HTTP GETrequest.
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  // Workaroud for timeout
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }

  Serial.println();

  // Wait for another round
  ESP.deepSleep(sleep*1000000);
}
