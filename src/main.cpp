//#include <TimeLib.h>
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "CRC16.h"
#include <cstdlib>
#include <PubSubClient.h>

// WiFi
const char *ssid = "----";
const char *password = "--";

// Mosquitto broker
const char* mosquittoHost="raspberry-ip";
const int mosquittoPort=1883;
const char* mosquittoTopic="home/power/usage";

WiFiClient espClient;
PubSubClient client(espClient);

#define MAXLINELENGTH 128 // longest normal line is 47 char (+3 for \r\n\0)
char telegram[MAXLINELENGTH];

double mTotal = 0.0;
double mActive = 0.0;

void updateMeterData() {
  char message[128];
  sprintf(message, "{\"mTotal\": %.3f,\"mActive\": %.3f}", mTotal, mActive);
  
  // Publish
  client.publish(mosquittoTopic, message);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Booting");

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

  //connecting to a mqtt broker
  client.setServer(mosquittoHost, mosquittoPort);

  String client_id = "esp8266-client";
  client_id += String(WiFi.macAddress());
  while(!client.connected()) {
    if(client.connect(client_id.c_str())) {
      Serial.println("Connected to Mosquitto broker");
    } else {
      Serial.print("Failed with state ");
      Serial.println(client.state());
      delay(200);
    }
  }
}

int indexOfChar(char *raw, char target)
{
  for (int i = 0; i < strlen(raw); i++)
  {
    if (raw[i] == target)
    {
      return i;
    }
  }
  return -1;
}

double parseDouble(char *raw)
{
  int start = indexOfChar(raw, '(');
  int end = indexOfChar(raw, '*');

  int len = end - start;

  char rawValue[len + 1];
  memcpy(rawValue, &raw[start + 1], len - 1);
  rawValue[end - start - 1] = '\0';

  return std::atof(rawValue);
}

bool decodeTelegram(char *raw)
{
  if (raw[0] == '!')
    return true;

  if (strncmp(raw, "1-0:1.8.0", strlen("1-0:1.8.0")) == 0)
    mTotal = parseDouble(raw);

  if (strncmp(raw, "1-0:1.7.0", strlen("1-0:1.7.0")) == 0)
    mActive = parseDouble(raw);

  return false;
}

void readTelegram()
{
  if (Serial.available())
  {
    memset(telegram, 0, sizeof(telegram));
    while (Serial.available())
    {
      int len = Serial.readBytesUntil('\n', telegram, MAXLINELENGTH);
      telegram[len] = '\n';
      telegram[len + 1] = 0;
      yield();

      char raw[len + 2];
      strcpy(raw, telegram);

      if (decodeTelegram(raw))
      {
        // Serial.println("Decoded message");
        // Serial.printf("mTotal: %f\n", mTotal);
        // Serial.printf("mActive: %f\n", mActive);
        updateMeterData();
      }
    }
  }
}

void loop()
{
  readTelegram();
}