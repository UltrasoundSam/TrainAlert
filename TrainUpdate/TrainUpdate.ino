#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include "configuration_secrets.h"

// Create wifi client and bot to receive telegram messages
WiFiClientSecure client;
UniversalTelegramBot bot(SECRET_TELEGRAMTOKEN, client);

// Use dedicated hardware SPI pins
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Create all char arrays to hold relevant strings
char endpoint[52];
char departure[5]; 
char platform[5];
const byte name_length = 12;
char depart_station_full[name_length];
char dest_station_full[name_length];
char output_msg[128];

// Define stations to depart from and arrive at.
char depart_station[] = "SHF";
char dest_station[] = "YRK";

// Timeout values
unsigned long previousMillis = 0;
const long interval = 2000;


void get_apidata(String &stringpayload) {
  // Create http request
  HTTPClient httpClient;
  httpClient.begin(endpoint);
  httpClient.setAuthorization(SECRET_APIUSERNAME, SECRET_APIKEY);

  // Get request
  int statusCode = httpClient.GET();

  if (statusCode > 0) {
    // Able to send request
    if (statusCode != HTTP_CODE_OK) {
      Serial.printf("Got HTTP status: %d", statusCode);
    }
    
    stringpayload = httpClient.getString();
  }
  // Release the resources used
  httpClient.end();
}

void parse_json(String &unformatted, char* platform, char* departuretime) {
  // Variables to hold http response and JSON doc
  DynamicJsonDocument json_data(8192);
  // Parse json data and extract departure time and platform number
  DeserializationError error = deserializeJson(json_data, unformatted);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  }

  // Check to make sure services exist
  const char* plat_data = json_data["services"][0]["locationDetail"]["platform"];
  const char* depart_time = json_data["services"][0]["locationDetail"]["realtimeDeparture"];
  const char* depart_s = json_data["location"]["name"];
  const char* dest_s = json_data["filter"]["destination"]["name"];

  // Transfer platform data (if valid)
  if (plat_data) {
    strlcpy(platform, plat_data, 5);
  }
  else {
    strlcpy(platform, "N/A", 5);
  }

  // Transfer departure time (if valid)
  if (depart_time) {
    strlcpy(departuretime, depart_time, 5);
  }
  else {
    strlcpy(departuretime, "None", 5);
  }
  // Get useful information from json data
  strlcpy(depart_station_full, json_data["location"]["name"], name_length);
  strlcpy(dest_station_full, json_data["filter"]["destination"]["name"], name_length);
}

void parse_telegram(int numNewMessages) {
  for (int i=0; i<numNewMessages; i++) {
    String nchat_id = String(bot.messages[i].chat_id);
    if (nchat_id != SECRET_USERID) {
      bot.sendMessage(nchat_id, "Unauthorized user", "");
      continue;
    }

    // Get received message
    char* text = (char*) bot.messages[i].text.c_str();

    // Parse command
    if (strcmp(text, "/next") == 0) {
      // Send details of next train
      bot.sendMessage(nchat_id, output_msg, "");
      continue;
    }

    if (strncmp(text, "/depart", 7) == 0) {
      // Update departure station
      strlcpy(depart_station, text+8, 4);
      // Confirm updated station
      char confirm[46];
      sprintf(confirm, "The departure station has been updated to %s", depart_station);
      bot.sendMessage(nchat_id, confirm, "");
      // Update screen
      tft.fillScreen(ST77XX_BLACK);
      continue;
    }

    if (strncmp(text, "/arrive", 7) == 0) {
      // Update arrival station
      strlcpy(dest_station, text+8, 4);
      // Confirm updated station
      char confirm[46];
      sprintf(confirm, "The arrival station has been updated to %s", dest_station);
      bot.sendMessage(nchat_id, confirm, "");
      // Update screen
      tft.fillScreen(ST77XX_BLACK);
      continue;
    }

    if (strcmp(text, "/config") == 0) {
      // Prepare information to be sent back to confirm setup
      char config[128];
      sprintf(config, "Looking at trains leaving %s and arriving in %s", depart_station_full, dest_station_full);
      bot.sendMessage(nchat_id, config, "");
      continue;
    }

    // If we have got to this point, command not set correctly so send help
    char helpmsg[] = "Commands for StationRun are:\n\t/next\n\t/depart\n\t/arrive\n\t/config\n";
    bot.sendMessage(nchat_id, helpmsg, "");
  }
}

void update_tft() {
  // Set font size
  tft.setTextSize(2);

  // Set cursor to correct position
  tft.setCursor(0, 0);

  // Update colour
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextWrap(true);
  char screen_msg1[64];
  char screen_msg2[64];
  sprintf(screen_msg1, "Next Train:\nFrom - %s\nTo - %s\nLeaves from\n\n", depart_station_full, dest_station_full);
  sprintf(screen_msg2, "Platform %s  \nat %s\n\n\n", platform, departure);
  tft.print(screen_msg1);
  tft.setTextSize(3);
  tft.print(screen_msg2);
}

void Wifi_disconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Reconnecting...");
  WiFi.begin(SECRET_SSID, SECRET_PASS);
}

void setup() {
  // Setup Serial
  Serial.begin(115200);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  // WiFi.onEvent(Wifi_disconnected, wifi_event_sta_disconnected_t);

  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(WiFi.localIP());
  Serial.print("\n");
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  // Setup TFT display - turn on backlite
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  // Turn on the TFT / I2C power supply
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  // Initialize TFT
  tft.init(135, 240); // Init ST7789 240x135
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
}

void loop() {
  // Find current time
  unsigned long currentMillis = millis();

  // Check to see whether interval has elapsed
  if (currentMillis - previousMillis >= interval) {
    // Update previous millis
    previousMillis = currentMillis;

    // Check for new telegram messages
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      parse_telegram(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    // Format endpoint for Realtime Trains API
    sprintf(endpoint, "https://api.rtt.io/api/v1/json/search/%s/to/%s", depart_station, dest_station);

    // Get data from api
    String payload;
    get_apidata(payload);

    // Get useful information from json data
    parse_json(payload, platform, departure);

    // Print out message to serial port
    sprintf(output_msg, "The next train from %s to %s will leave at %s from platform %s", depart_station_full, dest_station_full, departure, platform);
    update_tft();
  }
}
