#include <WiFi.h>
#include <time.h>
#include <math.h> // Include math.h for pi -> used in sin function for calculating brightness 
#include <HTTPClient.h>
#include <WiFiUdp.h>

#include <FastLED.h>
#include <ArduinoJson.h>

  #define LED_COUNT 69
  #define LED_PIN 23


// Global variables for WIFI AND API
// ---------------------------------------------------------------------------------------------
// Write your Wifi id and password to connect through the ESP32
const char* ssid = "";
const char* password = "";
const char *ntpServer = "pool.ntp.org";
static bool alreadyReceivedTodaysData = false; // Flag to check if the action has already been triggered at 6:00 AM
// Coordinates
const float LATITUDE = ;
const float LONGITUDE = ;
 
// Build the URL for the sunrisesunset API.
String weatherURL = "https://api.sunrisesunset.io/json?lat=" + String(LATITUDE, 5) + "&lng=" + String(LONGITUDE, 5) + "&time_format=unix";
// ---------------------------------------------------------------------------------------------

// Global variables for time
// ---------------------------------------------------------------------------------------------
// Variable to store the current time in unix format

// Variables for different important time markers through the day
unsigned int dawn, sunrise, sunset, dusk, noon, goldenHourMorning, goldenHourNight, currentTime;
// Floats to store the ratios of how far between important time markers are. e.g. Dawn[--------|--]Sunrise means the current time is 80% of the through the dawn-sunrise cycle
float currentTimeProgress, goldenHourProgress;
const int TIME_OFFSET = -21600;

// ---------------------------------------------------------------------------------------------

// Global variables for the LED strip
// ---------------------------------------------------------------------------------------------
CRGB leds[LED_COUNT];
unsigned int ledBrightness = 0;
uint8_t ledColor = 255; // 0-255 as an index for the following pallete.
// This color palette is for the transition between orange light and white light during golden hours(sunrise -> day, day -> sunset)
DEFINE_GRADIENT_PALETTE(goldenHourColors) { 
  0,   225,  70,  10,    // Orange at index 0
  255, 210, 235, 250   // daylight White at index 255
};
CRGBPalette16 goldenHourPalette = goldenHourColors;
// ---------------------------------------------------------------------------------------------



// ---------------------------------------------------------------------------------------------
// FUNCTIONS
// ---------------------------------------------------------------------------------------------

// Function to get the current unix time.
unsigned int getCurrentTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

// Function to connect the ESP32 to the wifi network.
void initWifi(){
  WiFi.mode(WIFI_STA); // Enable station mode
  // Open the connection to wifi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");
  return;
}

// Function to disconnect the ESP32 from the wifi network.
void disconnectWifi() {
  WiFi.disconnect(false);  // Using false as a parameter so network details aren't forgotton - this increases subsequent connections when grabbing new data each day.
  WiFi.mode(WIFI_OFF);    
  Serial.println("Wi-Fi disconnected.");
}


// This function Retrieves todays dawn, sunrise, sunset, and dusk times from https://sunrisesunset.io/api/
// Parses the json and stores important times in global variables
void getDateData(){
  initWifi();
  // Configure the time (GMT-6 offset, 0 bc no daylight savings, ntp server to get time from)
  configTime(TIME_OFFSET, 0, ntpServer);
  currentTime = getCurrentTime();

  // Create an http client instance and make a get request to the API url.
  HTTPClient http;
  http.begin(weatherURL);  // This uses root CA verification (optional override available)
  int httpCode = http.GET();

  if (httpCode > 0){
    // If the http request is successful, store the json payload 
    String payload = http.getString();
    // Serial.println("Received JSON:");
    // Serial.println(payload);

    // Parse JSON
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error){
      Serial.print("JSON parse failed: ");
      Serial.println(error.c_str());
      return;
    }

    // Access values
    JsonObject Date_Data = doc["results"];

    // Set global variables to current days values
    // currentDay = Date_Data["date"];
    dawn = Date_Data["dawn"];
    sunrise = Date_Data["sunrise"];
    sunset = Date_Data["sunset"];
    dusk = Date_Data["dusk"];
    noon = Date_Data["solar_noon"];
    // Set golden hour times to be 1hr away from sunrise and sunset
    goldenHourMorning = sunrise + 3600;
    goldenHourNight = sunset - 3600;
  } 
  else {
    Serial.print("HTTP request failed: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }

  http.end();
  disconnectWifi();
  return;
}

// Function to loop through the led's on the lightstrip and set their color based on the current time.
void updateLightColor(){
  for (int i = 0; i < LED_COUNT; i++) {
    leds[i] = ColorFromPalette(goldenHourPalette, ledColor);
  }
}

void updateLights(){
// if beween dawn and sunrise
  currentTime = getCurrentTime();
  // If it's night, lights will be off.
  if(currentTime < dawn || currentTime > dusk){
    ledBrightness = 0;
    ledColor = 0;
  }
  // If it's day, calculate the brightness of the lights as brightness = sin(currentTime). (Normalize first)
  else{
    float x = M_PI * float(currentTime - dawn) / float(dusk - dawn); // Set up the current time as a "progress" between 0 and pi.
    float brightnessSin = sin(x); // The peak of the sin graph will be noon, where y = 1.
    ledBrightness = int(brightnessSin * 100); // Max brightness value is 100. brightnessSin represents the current percentage of the max brightness.
  }



  // If its golden hour (MORNING), transition the lights from orange to white
  if(currentTime > dawn && currentTime < goldenHourMorning){
    goldenHourProgress = float(currentTime - sunrise) / float(goldenHourMorning - sunrise);
    ledColor = floor(goldenHourProgress * 255);
    if(ledColor > 255){
      ledColor = 255;
    }
    // updateLightColor();
  }
  else if(currentTime > goldenHourNight && currentTime < dusk){
      // Reducing from 100% to 0% so (1 - progress) * maxvalue.
      goldenHourProgress = float(currentTime - goldenHourNight) / float(sunset - goldenHourNight);
      ledColor = 255.0 - floor(goldenHourProgress * 255.0);
       if(ledColor < 0){
        ledColor = 0;
      }
      // updateLightColor();
    
  }
  updateLightColor();
  FastLED.setBrightness(ledBrightness);
  FastLED.show();
  
}


void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  // delay(5000);
  
  // Get the date data from API. Wifi connection terminates at the end of this function.
  getDateData();

  // Set up the light strip with FastLED
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);

  // Alternate light colors between orange and red
  updateLightColor();
  // Serial.println("Current Time = ");
  // Serial.println(currentTime);
  updateLights();

}


void loop() {
  // Temporary time struct for the current time. This is used to check if the time is 6:01 AM. 
  struct tm refreshTime; 
  if (getLocalTime(&refreshTime)) {
    // If the current time is 6:01 AM and the data hasn't been retrieved for the new day, then we will update the date data from the API.
    if ((refreshTime.tm_hour == 6 || refreshTime.tm_hour == 1 || refreshTime.tm_hour == 12) && refreshTime.tm_min == 1) { 
      if (!alreadyReceivedTodaysData) {
        
        alreadyReceivedTodaysData = true;

        // Get the dateData for the new day
        initWifi();
        getDateData();
        disconnectWifi();
      }
    } else {
      alreadyReceivedTodaysData = false;
    }
  }
  updateLights();
}


