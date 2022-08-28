//#include <TimeLib.h>
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "CRC16.h"

const char *ssid = "Dannfelt@elva";
const char *password = "Nyland3r@home1";


void sendHTTP(String data)
{
  WiFiClient client;
  HTTPClient http;

  String serverPath = "http://192.168.68.68:3000/echo";

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

void readTelegram() {
  if (Serial.available()) {
    char message[128];
    int count = 0;
    while (Serial.available()) {
      char buf[1];
      Serial.readBytes(buf, 1);
      message[count] = buf[0];
      yield();
      count++;
      // if(decodeTelegram(len+1))
      // {
      //    UpdateElectricity();
      //    UpdateGas();
      // }
    }
    message[count] = 0;
    // Serial.printf("Read count: %d\n", count);
    sendHTTP(String(count));
    sendHTTP(String(message));
    // Serial.println(String(message));
  }
}



void loop() {
  readTelegram();
}