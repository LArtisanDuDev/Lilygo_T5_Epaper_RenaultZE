// Decomment to DEBUG
//#define DEBUG_RENAULTAPI
// #define DEBUG_GRID
//#define DEBUG_WIFI

// Customize with your settings
#include "TOCUSTOMIZE.h"

#include <GxEPD.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <WiFi.h>
#include <MyDumbWifi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include <FreeSansBold50pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <icons.h>

// ESP32 battery (not Car :) )
const int PIN_BAT = 35;        // adc for bat voltage
const float VOLTAGE_100 = 4.2; // Full battery curent li-ion
const float VOLTAGE_0 = 3.5;   // Low battery curent li-ion
int batteryPercentage = 0;
float batteryVoltage = 0.0;

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/17, /*RST=*/16);
GxEPD_Class display(io, /*RST=*/16, /*BUSY=*/4);

// In case of errors
int currentLinePos = 0;

// RenaultZE auth vars
String gigya_login_token = "";
RTC_DATA_ATTR char x_gigya_id_token[1000] = "";

// RenaultZE Battery Datas
String timestamp = "";
String batteryLevel = "";
String batteryAutonomy = "";
String plugStatus = "";
String chargingStatus = "";
String chargingInstantaneousPower = "";
String chargingRemainingTime = "";

// plugStatus
// from https://github.com/hacf-fr/renault-api/blob/main/src/renault_api/kamereon/enums.py#L20
String PLUGSTATUS_UNPLUGGED = "0";
String PLUGSTATUS_PLUGGED = "1";

// chargingStatus
// From https://github.com/hacf-fr/renault-api/blob/main/src/renault_api/kamereon/enums.py#L6
String CHARGINGSTATUS_CHARGE_IN_PROGRESS = "1.0";
String CHARGINGSTATUS_WAITING_FOR_A_PLANNED_CHARGE = "0.1";
String CHARGINGSTATUS_WAITING_FOR_CURRENT_CHARGE = "0.3";

// put function declarations here:
void drawLine(int x0, int y0, int y1, int y2);
void updateBatteryPercentage(int &percentage, float &voltage);
void displayLine(String text);
void displayInfo();
void goToDeepSleepUntilNextWakeup();
void drawDebugGrid();
bool refreshJwt();
bool accounts_login();
bool accounts_getJWT();
bool getBatteryStatus();

void drawLine(int x0, int y0, int x1, int y1)
{
  display.drawLine(x0, y0, x1, y1, GxEPD_BLACK);
}

void setup()
{

  setlocale(LC_TIME, "fr_FR.UTF-8");

  Serial.begin(115200);
  Serial.println("Starting...\n");

  // Gathering battery level (ESP32)
  updateBatteryPercentage(batteryPercentage, batteryVoltage);

  // Initialize display
  display.init();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  Serial.println("Starting...");
  Serial.println("MAC Adress:");
  Serial.println(WiFi.macAddress().c_str());
  Serial.println("Battery:");

  char line[24];
  sprintf(line, "%5.3fv (%d%%)", batteryVoltage, batteryPercentage);
  Serial.println(line);
  // Connect to WiFi
  MyDumbWifi mdw;
#ifdef DEBUG_WIFI
  mdw.setDebug(true);
#endif
  if (!mdw.connectToWiFi(wifi_ssid, wifi_key))
  {
    displayLine("Error connecting wifi");
  }
  else
  {

    // Gathering RenaultZE datas
    bool result = getBatteryStatus();
    if (!result)
    {
      Serial.println("Jwt probably expired, get new one");
      if (refreshJwt())
      {
        result = getBatteryStatus();
      }
      else
      {
        Serial.println("Login failed");
        displayLine("Login failed");
      }
    }

    if (result)
    {
      Serial.println("Start display");
#ifdef DEBUG_GRID
      drawDebugGrid();
#endif
      displayInfo();
    }
    else
    {
      displayLine("Something went wrong");
    }
  }
  display.update();

  goToDeepSleepUntilNextWakeup();
}

void updateBatteryPercentage(int &percentage, float &voltage)
{
  // Lire la tension de la batterie
  voltage = analogRead(PIN_BAT) / 4096.0 * 7.05;
  percentage = 0;
  if (voltage > 1)
  { // Afficher uniquement si la lecture est valide
    percentage = static_cast<int>(2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303);
    // Ajuster le pourcentage en fonction des seuils de tension
    if (voltage >= VOLTAGE_100)
    {
      percentage = 100;
    }
    else if (voltage <= VOLTAGE_0)
    {
      percentage = 0;
    }
  }
}

void displayLine(String text)
{
  if (currentLinePos > 150)
  {
    currentLinePos = 0;
    display.fillScreen(GxEPD_WHITE);
  }
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(10, currentLinePos);
  display.print(text);
  currentLinePos += 10;
}

void drawBatteryLevel(int batteryTopLeftX, int batteryTopLeftY, int percentage)
{
  // Draw battery Level
  const int nbBars = 4;
  const int barWidth = 3;
  const int batteryWidth = (barWidth + 1) * nbBars + 2;
  const int barHeight = 4;
  const int batteryHeight = barHeight + 4;

  // Horizontal
  drawLine(batteryTopLeftX, batteryTopLeftY, batteryTopLeftX + batteryWidth, batteryTopLeftY);
  drawLine(batteryTopLeftX, batteryTopLeftY + batteryHeight, batteryTopLeftX + batteryWidth, batteryTopLeftY + batteryHeight);
  // Vertical
  drawLine(batteryTopLeftX, batteryTopLeftY, batteryTopLeftX, batteryTopLeftY + batteryHeight);
  drawLine(batteryTopLeftX + batteryWidth, batteryTopLeftY, batteryTopLeftX + batteryWidth, batteryTopLeftY + batteryHeight);
  // + Pole
  drawLine(batteryTopLeftX + batteryWidth + 1, batteryTopLeftY + 1, batteryTopLeftX + batteryWidth + 1, batteryTopLeftY + (batteryHeight - 1));
  drawLine(batteryTopLeftX + batteryWidth + 2, batteryTopLeftY + 1, batteryTopLeftX + batteryWidth + 2, batteryTopLeftY + (batteryHeight - 1));

  int i, j;
  int nbBarsToDraw = round(percentage / 25.0);
  for (j = 0; j < nbBarsToDraw; j++)
  {
    for (i = 0; i < barWidth; i++)
    {
      drawLine(batteryTopLeftX + 2 + (j * (barWidth + 1)) + i, batteryTopLeftY + 2, batteryTopLeftX + 2 + (j * (barWidth + 1)) + i, batteryTopLeftY + 2 + barHeight);
    }
  }
}

void displayInfo()
{
  // Portrait
  const int rotation = 0;
  display.setRotation(rotation);

  // esp32 batterie level
  drawBatteryLevel(95, 235, batteryPercentage);

  // car battery level
  display.drawRoundRect(2, 10, 115, 87, 8, GxEPD_BLACK);
  display.setFont(&FreeSansBold50pt7b);
  display.setCursor(5, 85);
  // cheat code to keep free space on screen :
  if (batteryLevel.length() == 1)
  {
    batteryLevel = "0" + batteryLevel;
  }
  if (batteryLevel == "100")
  {
    display.print("99");
  }
  else
  {
    display.print(batteryLevel);
  }

  // Autonomy
  display.setFont(&FreeSans18pt7b);
  display.setCursor(30, 130);
  display.print(batteryAutonomy);
  // Picto
  display.drawBitmap(bitmap_range, 0, 105, 30, 30, GxEPD_BLACK);

  // draw icons
  display.drawRoundRect(5, 140, 110, 50, 8, GxEPD_BLACK);
  if (plugStatus == PLUGSTATUS_PLUGGED)
  {
    display.drawBitmap(bitmap_plug, 10, 145, 40, 40, GxEPD_BLACK);
  }

  if (chargingStatus == CHARGINGSTATUS_CHARGE_IN_PROGRESS)
  {
    display.drawBitmap(bitmap_charging, 66, 145, 40, 40, GxEPD_BLACK);
  }

  // Charging power (not acurate)
  display.setCursor(10, 220);
  display.print(chargingInstantaneousPower + "kW");

  if (chargingStatus == CHARGINGSTATUS_WAITING_FOR_A_PLANNED_CHARGE || chargingStatus == CHARGINGSTATUS_WAITING_FOR_CURRENT_CHARGE)
  {
    display.drawBitmap(bitmap_waiting, 65, 145, 40, 40, GxEPD_BLACK);
  }

  display.setFont(&FreeSans9pt7b);
  display.setCursor(10, 240);
  timestamp.remove(0, 11);
  timestamp.replace("Z", "");
  display.print(timestamp);

#ifdef DEBUG_RENAULTAPI
  display.setFont(&FreeSans9pt7b);
  display.setCursor(90, 115);
  display.print(plugStatus);

  display.setCursor(90, 135);
  display.print(chargingStatus);
#endif
}

bool refreshJwt()
{
  if (accounts_login())
  {
    return accounts_getJWT();
  }
  return false;
}

bool accounts_getJWT()
{
  bool retour = false;
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    String renaultZEJwtPayload = "ApiKey=" + gigya_api_key + "&login_token=" + gigya_login_token + "&fields=data.personId,data.gigyaDataCenter&expiration=900";

#ifdef DEBUG_RENAULTAPI
    Serial.println(renaultZEJwtPayload);
#endif

    http.begin(gigya_root_url + "/accounts.getJWT");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST(renaultZEJwtPayload);
    Serial.print("getJWT httpcode ");
    Serial.println(httpCode);
    if (httpCode == 200)
    {
      DynamicJsonDocument doc(2048);
      String payload = http.getString();
#ifdef DEBUG_RENAULTAPI
      Serial.println("body getJWT:");
      Serial.println(payload);
#endif
      deserializeJson(doc, payload);

      // get jwt
      String jwt_token = doc["id_token"].as<String>();
      strcpy(x_gigya_id_token, jwt_token.c_str());
      retour = true;
    }
    else
    {
      Serial.println("/accounts.getJWT : " + http.errorToString(httpCode));
    }
    http.end();
  }
  return retour;
}

bool accounts_login()
{
  bool retour = false;
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    String renaultZELoginPayload = "ApiKey=" + gigya_api_key + "&loginID=" + myRenaultLogin + "&password=" + myRenaultPassword;

#ifdef DEBUG_RENAULTAPI
    Serial.println(renaultZELoginPayload);
#endif

    http.begin(gigya_root_url + "/accounts.login");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpCode = http.POST(renaultZELoginPayload);
    Serial.print("login httpcode ");
    Serial.println(httpCode);
    if (httpCode == 200)
    {
      DynamicJsonDocument doc(2048);
      String payload = http.getString();
#ifdef DEBUG_RENAULTAPI
      Serial.println("body login:");
      Serial.println(payload);
#endif
      deserializeJson(doc, payload);

      // get cookieValue
      gigya_login_token = doc["sessionInfo"]["cookieValue"].as<String>();
      retour = true;
    }
    else
    {
      Serial.println("/accounts.login : " + http.errorToString(httpCode));
    }
    http.end();
  }
  return retour;
}

bool getBatteryStatus()
{
  bool retour = false;
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;

    String kamereon_url = kamereon_root_url + "/commerce/v1/accounts/" + accound_id + "/kamereon/kca/car-adapter/v2/cars/" + vin + "/battery-status?country=" + country;
    Serial.println(kamereon_url);

    http.begin(kamereon_url);

    http.addHeader("Content-type", "application/vnd.api+json");
    http.addHeader("apikey", kamereon_api_key);
    http.addHeader("x-gigya-id_token", x_gigya_id_token);

    int httpCode = http.GET();
    Serial.print("battery-status httpcode ");
    Serial.println(httpCode);
    if (httpCode == 200)
    {
      DynamicJsonDocument doc(1024);
      String payload = http.getString();
#ifdef DEBUG_RENAULTAPI
      Serial.println("body battery-status:");
      Serial.println(payload);
#endif
      deserializeJson(doc, payload);

      timestamp = doc["data"]["attributes"]["timestamp"].as<String>();
      batteryLevel = doc["data"]["attributes"]["batteryLevel"].as<String>();
      batteryAutonomy = doc["data"]["attributes"]["batteryAutonomy"].as<String>();
      plugStatus = doc["data"]["attributes"]["plugStatus"].as<String>();
      chargingStatus = doc["data"]["attributes"]["chargingStatus"].as<String>();
      chargingInstantaneousPower = doc["data"]["attributes"]["chargingInstantaneousPower"].as<String>();
      chargingRemainingTime = doc["data"]["attributes"]["chargingRemainingTime"].as<String>();

#ifdef DEBUG_RENAULTAPI
      Serial.println("timestamp :" + timestamp);
      Serial.println("batteryLevel :" + batteryLevel);
      Serial.println("batteryAutonomy :" + batteryAutonomy);
      Serial.println("plugStatus :" + plugStatus);
      Serial.println("chargingStatus :" + chargingStatus);
      Serial.println("chargingInstantaneousPower :" + chargingInstantaneousPower);
      Serial.println("chargingRemainingTime :" + chargingRemainingTime);
#endif
      retour = true;
    }
  }
  return retour;
}

void drawDebugGrid()
{
  int gridSpacing = 10; // Espacement entre les lignes de la grille
  int screenWidth = 122;
  int screenHeight = 250;

  Serial.print("Width : ");
  Serial.print(screenWidth);
  Serial.print(" Eight : ");
  Serial.println(screenHeight);

  // Dessiner des lignes verticales
  for (int x = 0; x <= screenWidth; x += gridSpacing)
  {
    drawLine(x, 0, x, screenHeight);
  }

  // Dessiner des lignes horizontales
  for (int y = 0; y <= screenHeight; y += gridSpacing)
  {
    drawLine(0, y, screenWidth, y);
  }
}

void goToDeepSleepUntilNextWakeup()
{
  time_t sleepDuration = WAKEUP_INTERVAL;
  Serial.print("Sleeping duration (seconds): ");
  Serial.println(sleepDuration);

  // Configure wake up
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  esp_deep_sleep_start();
}

void loop()
{
  // put your main code here, to run repeatedly:
}
