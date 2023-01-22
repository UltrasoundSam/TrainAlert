#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "configuration_secrets.h"

// Create wifi client
WiFiClientSecure client;

// Create end point of API
char endpoint[52];
char departure[5]; 
char platform[5];
char depart_station_full[12];
char dest_station_full[12];
char output_msg[128];

// Variables to hold http response and JSON doc
DynamicJsonDocument doc(4096);

void setup() {
  // Setup Serial
  Serial.begin(115200);
    
  // Setup wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(WiFi.localIP());
  Serial.print("\n");

}

void loop() {
  // Format endpoint for Realtime Trains API
  char* dest_station = "YRK";
  char* depart_station = "SHF";
  sprintf(endpoint, "https://api.rtt.io/api/v1/json/search/%s/to/%s", depart_station, dest_station);
  
  // Get data from api
  String payload;
  get_apidata(payload);

  // Get useful information from json data
  parse_json(payload, platform, departure);
  
  sprintf(output_msg, "The next train from %s to %s will leave at %s from platform %s", depart_station_full, dest_station_full, departure, platform);
  Serial.println(output_msg);

  delay(30000);
}


void get_apidata(String &stringpayload) {
  // Create http request
  HTTPClient httpClient;
  httpClient.begin(endpoint);
  httpClient.setAuthorization(SECRET_APIUSERNAME, SECRET_APIKEY);

  // Get request
  int statusCode = httpClient.GET();

  if (statusCode > 0) {
    // Able to send request
    if (statusCode == HTTP_CODE_OK) {
      Serial.println("Server responsed with HTTP status 200");
    }
    else {
      Serial.printf("Got HTTP status: %d", statusCode);
      return;
    }
    
    stringpayload = httpClient.getString();
    // Release the resources used
    httpClient.end();
  }
}


void parse_json(String &unformatted, char* platform, char* departuretime) {
  // Parse json data and extract departure time and platform number
  DeserializationError error = deserializeJson(doc, unformatted);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  }

  JsonObject json_data = doc.as<JsonObject>();

  // Get useful information from json data
  const char* plat_data = json_data["services"][0]["locationDetail"]["platform"];
  const char* depart_time = json_data["services"][0]["locationDetail"]["realtimeDeparture"];
  const char* depart_s = json_data["location"]["name"];
  const char* dest_s = json_data["filter"]["destination"]["name"];
  
  // Copy out for plotting
  strcpy(platform, plat_data);
  strcpy(departuretime, depart_time);
  strcpy(depart_station_full, depart_s);
  strcpy(dest_station_full, dest_s);
}
