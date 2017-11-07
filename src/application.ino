/**The MIT License (MIT)

Derived from https://github.com/squix78/esp8266-weather-station/blob/master/examples/WeatherStationDemoExtended/WeatherStationDemoExtended.ino
*   Original work by Daniel Eichhorn

Original work Copyright (c) 2016 by Daniel Eichhorn
Modified work Copyright 2017 Gianluca Cassarino

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch
*/

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <JsonListener.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <Wire.h>

// Libraries needed for LCD display
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// FONTS (Adafruit_GFX format) for LCD
#include <Fonts/TomThumb.h> // generic text
#include <Fonts/FreeSansBold18pt7b.h> // clock digits
#include "nokiafc224pt7b.h" // used for the date
// WEATHER FONTS (Adafruit_GFX format) for LCD
// NOTE: full icons are uppercase, stroked icons are lowercase equivalent
// (where available, see font project file or ttf file for details)
#include "pe_icon_set_weather6pt7b.h"
#include "pe_icon_set_weather16pt7b.h"

// adapted from OLEDDisplayUi.h for LCD5110 NOKIA
#include "LCDDisplayUi.h"

#include "WundergroundClient.h"


/***************************
 * Begin Settings
 **************************/
// Please read http://blog.squix.org/weatherstation-getting-code-adapting-it
// for setup instructions

// wifi static ip
char ip_static[16] = "192.168.1.28";
char ip_gateway[16] = "192.168.1.1";
char ip_subnet[16] = "255.255.255.0";

#define HOSTNAME "ESP8266-OTA-"

// Setup
const int UPDATE_INTERVAL_SECS = 3600; // update every 1 hour

// **** NOKIA display settings
// Hardware SPI (faster, but must use certain hardware pins):
// SCK is LCD serial clock (SCLK) - this is pin 13 on Arduino Uno
// MOSI is LCD DIN - this is pin 11 on an Arduino Uno
// NOTE with hardware SPI MISO and SS pins aren't used but will still be read
// and written to during SPI transfer. Be careful sharing these pins!
const int8_t RST_PIN = D2;
const int8_t CE_PIN = D1;
const int8_t DC_PIN = D6;
//const int8_t DIN_PIN = D7;  // HW SPI MOSI // Uncomment for Software SPI
//const int8_t CLK_PIN = D5;  // HW SPI CLK // Uncomment for Software SPI
const int8_t BL_PIN = D0;
// ***** NOKIA/

// TimeClient settings
// const float UTC_OFFSET = 1; // ITALY 1 or 2 since DST is not implemented
// will use time/date from Wunderground instead, since manages DST automatically

// Wunderground Settings
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY =
#include "wunderground_api_key.example"
;
const String WUNDERGRROUND_LANGUAGE = "IT";
const String WUNDERGROUND_COUNTRY = "IT";
const String WUNDERGROUND_CITY = "Rome";

// Initialize NOKIA display (Spi)
Adafruit_PCD8544 display = Adafruit_PCD8544(DC_PIN, CE_PIN, RST_PIN);
// Initialize Ui Library
LCDDisplayUi ui( &display );

/***************************
 * End Settings
 **************************/

enum LCDDISPLAY_TEXT_ALIGNMENT {
 TEXT_ALIGN_CENTER = 0,
 TEXT_ALIGN_RIGHT = 1,
 TEXT_ALIGN_LEFT = 2
};

// TimeClient timeClient(UTC_OFFSET);

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClient wunderground(IS_METRIC);

// flag changed in the ticker function every xx minutes, see UPDATE_INTERVAL_SECS
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

Ticker ticker;

//declaring prototypes
void configModeCallback (WiFiManager *myWiFiManager);
void drawProgress(Adafruit_PCD8544 *display, int percentage, String label);
void drawProgressBar(Adafruit_PCD8544 *display, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress);
void drawOtaProgress(unsigned int, unsigned int);
void updateData(Adafruit_PCD8544 *display);
void drawDateTime(Adafruit_PCD8544 *display, LCDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(Adafruit_PCD8544 *display, LCDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(Adafruit_PCD8544 *display, LCDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(Adafruit_PCD8544 *display, int x, int y, int dayIndex);
void drawHeaderOverlay(Adafruit_PCD8544 *display, LCDDisplayUiState* state);
void drawAstronomyMoon(Adafruit_PCD8544 *display, LCDDisplayUiState* state, int16_t x, int16_t y);
void drawAstronomySun(Adafruit_PCD8544 *display, LCDDisplayUiState* state, int16_t x, int16_t y);
//
void setReadyForWeatherUpdate();
int8_t getWifiQuality();
uint8_t getCursorPosX(uint8_t textLength, uint8_t fontWidth, LCDDISPLAY_TEXT_ALIGNMENT textAlignment);


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast, drawAstronomyMoon, drawAstronomySun };
int numberOfFrames = 5;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

void setup() {

  Serial.begin(115200);

  // Initialize LCD
  display.begin();
  // display.setContrast(55);
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.clearDisplay();
  display.display();
  // LCD backlight
  pinMode(BL_PIN, OUTPUT);
  digitalWrite(BL_PIN, HIGH);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // NOTE Uncomment for testing wifi manager
  // wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  //or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();

  //set static ip
  IPAddress _ip,_gw,_sn;
  _ip.fromString(ip_static);
  _gw.fromString(ip_gateway);
  _sn.fromString(ip_subnet);
  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //Manual Wifi
  //WiFi.begin(WIFI_SSID, WIFI_PWD);
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // display.drawString(64, 10, "Connecting to WiFi");
    // display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbol : inactiveSymbol);
    // display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbol : inactiveSymbol);
    // display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbol : inactiveSymbol);
    // display.display();
    display.setCursor(8, 8);
    display.print("Connecting to WiFi");
    display.display();

    counter++;
  }

  // UI
  ui.setTargetFPS(30);
  ui.setTimePerFrame(10000); //Set the approx. time (ms) a frame is displayed
  ui.disableIndicator();
  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setTimePerTransition(2000); // ms
  ui.setFrames(frames, numberOfFrames);
  ui.setOverlays(overlays, numberOfOverlays);
  // Inital UI takes care of initalising the display too.
  ui.init();

  // Setup OTA
  Serial.println("Hostname: " + hostname);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.onProgress(drawOtaProgress);
  ArduinoOTA.begin();

  updateData(&display);

  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);

}

void loop() {

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    ArduinoOTA.handle();
    delay(remainingTimeBudget);
  }

}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setCursor(8, 0); // The GFX library doesn't support centering text on the screen
  display.print("WIFI MANAGER");
  display.setCursor(0, 8);
  display.print("Connect to AP:");
  display.setCursor(0, 16);
  display.print(myWiFiManager->getConfigPortalSSID());
  display.setCursor(0, 24);
  display.print("To setup Wifi Configuration");
  display.display();
}

void drawProgress(Adafruit_PCD8544 *display, int percentage, String label) {
  display->clearDisplay();
  drawProgressBar(display, 2, 12, 80, 10, percentage);
  display->setTextSize(1);
  display->setTextWrap(false);
  display->setFont(&TomThumb);
  display->setCursor(4, 28);
  label.toUpperCase();
  display->print(label);
  display->setFont();
  display->display();
}

void drawProgressBar(Adafruit_PCD8544 *display, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress) {
  uint16_t radius = height / 2;
  uint16_t xRadius = x + radius;
  uint16_t yRadius = y + radius;
  uint16_t innerRadius = radius - 2;
  display->drawRoundRect(x, y, width, height, radius, 1);
  uint16_t maxProgressWidth = (width - 2) * progress / 100;
  display->fillRoundRect(x + 1, y + 2, maxProgressWidth, height - 4, innerRadius, 1);
}

void drawOtaProgress(unsigned int progress, unsigned int total) {
  digitalWrite(BL_PIN, HIGH); // LCD backlight ON
  display.clearDisplay();
  drawProgressBar(&display, 2, 12, 80, 10, progress / (total / 100));
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setCursor(4, 24);
  display.print("OTA Update");
  display.display();
}

void updateData(Adafruit_PCD8544 *display) {
  digitalWrite(BL_PIN, HIGH);
  // drawProgress(display, 10, "updating time...");
  // timeClient.updateTime(); // using wunderground time instead
  drawProgress(display, 30, "updating conditions...");
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 60, "updating forecasts...");
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 90, "updating astronomy...");
  wunderground.updateAstronomy(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  // lastUpdate = timeClient.getFormattedTime();
  lastUpdate = wundergroundGetFormattedTime();
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "              Done");
  delay(1000);
  digitalWrite(BL_PIN, LOW); // LCD backlight OFF
}

void drawDateTime(Adafruit_PCD8544 *display, LCDDisplayUiState* state, int16_t x, int16_t y) {
  String date = wunderground.getDate();

  // removes the comma after the name of the day
  date.remove(3, 1);

  // Calculates the position based on the width occupied by the character
  // + white space between characters (kerning)
  // in the case of the nokiafc224pt7b font is bxh (6+1)x7 screen pixels
  // NOTE these values are valid in case the setTextSize = 1, if 2 they are doubled
  uint8_t cursorAlign = getCursorPosX(date.length(), 7, TEXT_ALIGN_CENTER);

  display->setTextSize(1);
  display->setTextWrap(false);
  display->setCursor(x + cursorAlign, y);
  display->setFont(&nokiafc224pt7b); //&TomThumb);
  date.toUpperCase();
  display->print(date);
  display->setFont();

  display->setFont(&FreeSansBold18pt7b);
  // String time = timeClient.getFormattedTime();

  // using TimeClient
  // String hours = timeClient.getHours();
  // String minutes = timeClient.getMinutes();
  // String seconds = timeClient.getSeconds();

  // using Wunderground, as suggested here https://github.com/squix78/esp8266-weather-station/issues/24
  String hours = wunderground.getHours();
  String minutes = wunderground.getMinutes();
  String seconds = wunderground.getSeconds();

  display->setCursor(x, y + 34);
  // display->print(time);
  display->print(hours);
  display->setCursor(x + 38, y + 31);
  if (seconds.toInt() % 2) {
    display->print(":");
  } else {
    display->print(" ");
  }
  display->setCursor(x + 46, y + 34);
  display->print(minutes);

  // seconds
  display->setFont(&TomThumb);
  display->setCursor(x + 39, y + 26);
  display->print(seconds);
  display->setFont();

  display->display();
}

void drawCurrentWeather(Adafruit_PCD8544 *display, LCDDisplayUiState* state, int16_t x, int16_t y) {
  String temp = wunderground.getCurrentTemp(); //+ char(247) + "C"; // ° degree symbol
  String weatherIcon = wunderground.getTodayIcon();
  String weather = wunderground.getWeatherText();
  uint8_t cursorAlign = getCursorPosX(weather.length(), 4, TEXT_ALIGN_CENTER);

  display->setTextSize(1);
  display->setTextWrap(false);
  display->setFont(&TomThumb);
  display->setCursor(x + cursorAlign, y + 5);
  weather.toUpperCase();
  display->print(weather);
  display->setFont();

  display->setTextSize(2);
  display->setFont(&nokiafc224pt7b);
  display->setCursor(x, y + 20);
  display->print(temp);
  display->setFont();

  display->setTextSize(1);
  display->setCursor(x + 40, y + 8);
  display->print(char(247)); // ° degree symbol
  display->print("C");

  display->setFont(&pe_icon_set_weather16pt7b);
  display->setCursor(x + 50, y + 34);
  // weatherIcon.toLowerCase(); // use alternate symbol (stroked icon)
  display->print(weatherIcon); //weatherIcon);

  // draw today forecast details
  display->setFont(&pe_icon_set_weather6pt7b);
  display->setCursor(x + 4, y + 35);
  display->print(wunderground.getForecastIcon(0));
  display->setCursor(x + 20, y + 36);
  display->print("T"); // pe-is-w-thermometer-1-f ("t" for alt version)
  display->setFont(&TomThumb);
  display->setCursor(x + 25, y + 30);
  display->print(wunderground.getForecastHighTemp(0));
  display->setCursor(x + 26, y + 36);
  display->print(wunderground.getForecastLowTemp(0));

  display->setFont();

  display->display();
}

void drawForecast(Adafruit_PCD8544 *display, LCDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextSize(1);
  display->setTextWrap(false);
  drawForecastDetails(display, x, y, 2); // 0
  drawForecastDetails(display, x + 32, y, 4); // 2
  drawForecastDetails(display, x + 64, y, 6); // 4
  display->display();
}

// NOTE this function should be called by drawForecast
void drawForecastDetails(Adafruit_PCD8544 *display, int x, int y, int dayIndex) {
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  display->setCursor(x, y);
  display->print(day);
  display->setFont(&pe_icon_set_weather6pt7b);
  display->setCursor(x + 3, y + 19);
  display->print(wunderground.getForecastIcon(dayIndex));
  display->setCursor(x + 3, y + 34);
  display->print("T"); // pe-is-w-thermometer-1-f ("t" for alt version)
  display->setFont(&TomThumb);
  display->setCursor(x + 8, y + 28);
  display->print(wunderground.getForecastHighTemp(dayIndex));
  display->setCursor(x + 9, y + 34);
  display->print(wunderground.getForecastLowTemp(dayIndex));
  display->setFont();
}

void drawAstronomyMoon(Adafruit_PCD8544 *display, LCDDisplayUiState* state, int16_t x, int16_t y){
  String moonPctIlum = wunderground.getMoonPctIlum() + "%";
  String moonPhase = wunderground.getMoonPhase() + " ";
  moonPhase.toUpperCase();

  display->setTextSize(1);
  display->setTextWrap(false);
  // MOON icon
  display->setFont(&pe_icon_set_weather16pt7b);
  display->setCursor(2 + x, 34 + y);
  display->print("O"); // "o" for alternate icon
  display->setFont();

  display->setFont(&TomThumb);
  display->setCursor(0 + x, 6 + y);
  display->print(moonPhase);

  display->setCursor(2 + x, 14 + y);
  display->print("LUM");
  display->setCursor(2 + x, 20 + y);
  display->print(moonPctIlum);

  display->setCursor(44 + x, 14 + y);
  display->print("SORGE:"); // rise
  display->setCursor(32 + x, 21 + y);
  display->print("TRAMONTA:"); // fall
  display->setCursor(68 + x, 14 + y);
  display->print(wunderground.getMoonriseTime());
  display->setCursor(68 + x, 21 + y);
  display->print(wunderground.getMoonsetTime());

  display->setCursor(32 + x, 30 + y);
  display->print(wunderground.getMoonAge() + " GIORNI");

  display->setFont();

  display->display();

}

void drawAstronomySun(Adafruit_PCD8544 *display, LCDDisplayUiState* state, int16_t x, int16_t y){
  display->setTextSize(1);
  display->setTextWrap(false);
  // SUN icon
  display->setFont(&pe_icon_set_weather16pt7b);
  display->setCursor(0 + x, 28 + y);
  display->print("B"); // "b" for alternate
  display->setFont();

  display->setFont(&TomThumb);
  display->setCursor(44 + x, 10 + y);
  display->print("SORGE:"); // rise
  display->setCursor(32 + x, 17 + y);
  display->print("TRAMONTA:"); // fall
  display->setCursor(68 + x, 10 + y);
  display->print(wunderground.getSunriseTime());
  display->setCursor(68 + x, 17 + y);
  display->print(wunderground.getSunsetTime());

  // WIND icon
  display->setFont(&pe_icon_set_weather6pt7b);
  display->setCursor(38 + x, 32 + y);
  display->print("X"); // "X" for alternate

  display->setFont(&TomThumb);
  display->setCursor(51 + x, 27 + y);
  display->print(wunderground.getWindSpeed());
  display->setCursor(51 + x, 33 + y);
  display->print(wunderground.getWindDir());

  display->setFont();

  display->display();
}

void drawHeaderOverlay(Adafruit_PCD8544 *display, LCDDisplayUiState* state) {
  // String time = timeClient.getFormattedTime().substring(0, 5);
  String time = wundergroundGetFormattedTime().substring(0, 5);
  String temp = wunderground.getCurrentTemp() + "C";

  display->setTextSize(1);
  display->setTextWrap(false);
  display->setFont(&TomThumb);
  // currentFrame/numberOfFrames
  display->setCursor(0, 48);
  display->printf("%d/%d\n", state->currentFrame + 1, numberOfFrames);
  // time
  display->setCursor(18, 48);
  display->print(time);
  // current temperature
  display->setCursor(52, 48);
  display->print(temp);

  display->setFont();

  // wifi signal quality
  int8_t quality = getWifiQuality();
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        display->drawPixel(76 + 2 * i, 47 - j, 1);
      }
    }
  }

  // today weather (icon)
  display->setFont(&pe_icon_set_weather6pt7b);
  String weatherIcon = wunderground.getTodayIcon();
  display->drawChar(37, 44, weatherIcon[0], 1, 0, 1);
  display->setFont();
  //
  display->drawLine(0, 40, 35, 40, 1);
  display->drawLine(50, 40, 84, 40, 1);
}

// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if(dbm <= -100) {
      return 0;
  } else if(dbm >= -50) {
      return 100;
  } else {
      return 2 * (dbm + 100);
  }
}

// Simple text alignment implementation (the GFX library doesn't support text alignment)
// Calculates the position based on the width occupied by the character
// + white space between characters (kerning)
// in the case of the nokiafc224pt7b font is wxh (6+1)x7 screen pixels
// in the case of the TomThumb font is wxh (3+1)x5 screen pixels
// NOTE these values are valid in case the setTextSize = 1, if 2 they are doubled
// Returns cursor x position based on text alignment choosed
uint8_t getCursorPosX(uint8_t textLength, uint8_t fontWidth, LCDDISPLAY_TEXT_ALIGNMENT textAlignment) {
  // Specifies relative to which anchor point
  // the text is rendered. Available constants:
  // TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER, TEXT_ALIGN_RIGHT
  uint8_t xCursor = 0;
  if ( (textLength * fontWidth) > LCDWIDTH ) {
    // the text is longer than the row (84px)
    return 0;
  }
  switch (textAlignment) {
    case TEXT_ALIGN_CENTER:
      xCursor = (LCDWIDTH - (textLength * fontWidth)) / 2;
    break;
    case TEXT_ALIGN_RIGHT:
      xCursor = LCDWIDTH - (textLength * fontWidth);
    break;
    case TEXT_ALIGN_LEFT:
    defaut:
      xCursor = 0;
  }
  return xCursor;
}

String wundergroundGetFormattedTime() {
  return wunderground.getHours() + ":" + wunderground.getMinutes() + ":" + wunderground.getSeconds();
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}
