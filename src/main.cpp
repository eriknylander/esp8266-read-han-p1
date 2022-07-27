//#include <TimeLib.h>
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "CRC16.h"

//===Change values from here===
const char *ssid = "Dannfelt@elva";
const char *password = "Nyland3r@home1";
//===Change values to here===

// Vars to store meter readings
long mEVLT = 0; // Meter reading Electrics - consumption low tariff
long mEVHT = 0; // Meter reading Electrics - consumption high tariff
long mEOLT = 0; // Meter reading Electrics - return low tariff
long mEOHT = 0; // Meter reading Electrics - return high tariff
long mEAV = 0;  // Meter reading Electrics - Actual consumption
long mEAT = 0;  // Meter reading Electrics - Actual return
long mGAS = 0;  // Meter reading Gas
long prevGAS = 0;

#define MAXLINELENGTH 128 // longest normal line is 47 char (+3 for \r\n\0)
char telegram[MAXLINELENGTH];

int rxPin = 0;           // pin for SoftwareSerial RX
SoftwareSerial mySerial; /*(rxPin, -1, false, MAXLINELENGTH); // (RX, TX. inverted, buffer)*/

unsigned int currentCRC = 0;

bool outputOnSerial = true;

void sendHTTP(String data)
{
  WiFiClient client;
  HTTPClient http;

  String serverPath = "http://192.168.68.76:3000/echo";

  // Your Domain name with URL path or IP address with path
  http.begin(client, serverPath.c_str());

  // Send HTTP GET request
  int httpResponseCode = http.POST(data);

  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Booting");

  mySerial.begin(115200, SWSERIAL_8N1, rxPin, -1, false);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  sendHTTP("ESP8266 connected");
}

bool isNumber(char *res, int len)
{
  for (int i = 0; i < len; i++)
  {
    if (((res[i] < '0') || (res[i] > '9')) && (res[i] != '.' && res[i] != 0))
    {
      return false;
    }
  }
  return true;
}

int FindCharInArrayRev(char array[], char c, int len)
{
  for (int i = len - 1; i >= 0; i--)
  {
    if (array[i] == c)
    {
      return i;
    }
  }
  return -1;
}

long getValidVal(long valNew, long valOld, long maxDiffer)
{
  // check if the incoming value is valid
  if (valOld > 0 && ((valNew - valOld > maxDiffer) && (valOld - valNew > maxDiffer)))
    return valOld;
  return valNew;
}

long getValue(char *buffer, int maxlen)
{
  int s = FindCharInArrayRev(buffer, '(', maxlen - 2);
  if (s < 8)
    return 0;
  if (s > 32)
    s = 32;
  int l = FindCharInArrayRev(buffer, '*', maxlen - 2) - s - 1;
  if (l < 4)
    return 0;
  if (l > 12)
    return 0;
  char res[16];
  memset(res, 0, sizeof(res));

  if (strncpy(res, buffer + s + 1, l))
  {
    if (isNumber(res, l))
    {
      return (1000 * atof(res));
    }
  }
  return 0;
}

bool decodeTelegram(int len)
{
  // need to check for start
  int startChar = FindCharInArrayRev(telegram, '/', len);
  int endChar = FindCharInArrayRev(telegram, '!', len);
  bool validCRCFound = false;
  if (startChar >= 0)
  {
    // start found. Reset CRC calculation
    currentCRC = CRC16(0x0000, (unsigned char *)telegram + startChar, len - startChar);
    if (outputOnSerial)
    {
      for (int cnt = startChar; cnt < len - startChar; cnt++) {
        Serial.print(telegram[cnt]);
        sendHTTP(String(telegram[cnt]));
      }
    }
    // Serial.println("Start found!");
  }
  else if (endChar >= 0)
  {
    // add to crc calc
    currentCRC = CRC16(currentCRC, (unsigned char *)telegram + endChar, 1);
    char messageCRC[5];
    strncpy(messageCRC, telegram + endChar + 1, 4);
    messageCRC[4] = 0; // thanks to HarmOtten (issue 5)
    if (outputOnSerial)
    {
      for (int cnt = 0; cnt < len; cnt++)
        Serial.print(telegram[cnt]);
    }
    validCRCFound = (strtol(messageCRC, NULL, 16) == currentCRC);
    if (validCRCFound) {
      Serial.println("\nVALID CRC FOUND!");
      sendHTTP("\nVALID CRC FOUND!");
    }
    else {
      Serial.println("\n===INVALID CRC FOUND!===");
      sendHTTP("===INVALID CRC FOUND!===");
    }
    currentCRC = 0;
  }
  else
  {
    currentCRC = CRC16(currentCRC, (unsigned char *)telegram, len);
    if (outputOnSerial)
    {
      for (int cnt = 0; cnt < len; cnt++) {
        Serial.print(telegram[cnt]);
        sendHTTP(String(telegram[cnt]));
      }
    }
  }

  long val = 0;
  long val2 = 0;
  // 1-0:1.8.1(000992.992*kWh)
  // 1-0:1.8.1 = Elektra verbruik laag tarief (DSMR v4.0)
  if (strncmp(telegram, "1-0:1.8.1", strlen("1-0:1.8.1")) == 0)
    mEVLT = getValue(telegram, len);

  // 1-0:1.8.2(000560.157*kWh)
  // 1-0:1.8.2 = Elektra verbruik hoog tarief (DSMR v4.0)
  if (strncmp(telegram, "1-0:1.8.2", strlen("1-0:1.8.2")) == 0)
    mEVHT = getValue(telegram, len);

  // 1-0:2.8.1(000348.890*kWh)
  // 1-0:2.8.1 = Elektra opbrengst laag tarief (DSMR v4.0)
  if (strncmp(telegram, "1-0:2.8.1", strlen("1-0:2.8.1")) == 0)
    mEOLT = getValue(telegram, len);

  // 1-0:2.8.2(000859.885*kWh)
  // 1-0:2.8.2 = Elektra opbrengst hoog tarief (DSMR v4.0)
  if (strncmp(telegram, "1-0:2.8.2", strlen("1-0:2.8.2")) == 0)
    mEOHT = getValue(telegram, len);

  // 1-0:1.7.0(00.424*kW) Actueel verbruik
  // 1-0:2.7.0(00.000*kW) Actuele teruglevering
  // 1-0:1.7.x = Electricity consumption actual usage (DSMR v4.0)
  if (strncmp(telegram, "1-0:1.7.0", strlen("1-0:1.7.0")) == 0)
    mEAV = getValue(telegram, len);

  if (strncmp(telegram, "1-0:2.7.0", strlen("1-0:2.7.0")) == 0)
    mEAT = getValue(telegram, len);

  // 0-1:24.2.1(150531200000S)(00811.923*m3)
  // 0-1:24.2.1 = Gas (DSMR v4.0) on Kaifa MA105 meter
  if (strncmp(telegram, "0-1:24.2.1", strlen("0-1:24.2.1")) == 0)
    mGAS = getValue(telegram, len);

  return validCRCFound;
}

void readTelegram()
{
  if (mySerial.available())
  {
    sendHTTP("mySerial.available() was true!");
    memset(telegram, 0, sizeof(telegram));
    while (mySerial.available())
    {
      int len = mySerial.readBytesUntil('\n', telegram, MAXLINELENGTH);
      telegram[len] = '\n';
      telegram[len + 1] = 0;
      yield();
      if (decodeTelegram(len + 1))
      {
        sendHTTP("decoded telegram");
        char buffer[1024];
        snprintf(buffer, 1024, "mEAV:%d", mEAV);
        sendHTTP(String(buffer));
      }
    }
  }
}

void loop()
{
  readTelegram();
}
