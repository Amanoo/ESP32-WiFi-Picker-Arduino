#include "wifipicker.hpp" //already includes WiFi.h, SPIFFS.h, ESPAsyncWebServer.h

#include "WiFi.h"
#include "SPIFFS.h"
#include "ESPAsyncWebServer.h"
#include "Wire.h"
#include "Adafruit_INA219.h"
#include "index.h" //Our HTML webpage contents with javascripts

Adafruit_INA219 ina219;

AsyncWebServer server(80); //Server on port 80

float current_mA = 0;
float voltage = 0;

void setup() {
  Serial.begin(115200);
  wifiPicker(); //This will connect to WiFi

  Serial.println("");

  ina219.begin();
  ina219.setCalibration_16V_400mA();

  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP

  server.on("/", [](AsyncWebServerRequest * request) {
    String s = MAIN_page; //Read HTML contents
    request->send(200, "text/html", s); //Send web page
  });

  server.on("/readADC", [](AsyncWebServerRequest * request) {
    String adcValue = String(current_mA)+"&"+String(voltage);
    request->send(200, "text/plane", adcValue); //Send ADC value only to client ajax request
  });

  server.begin();                  //Start server
  Serial.println("HTTP server started");

}


void loop() {

    current_mA = ina219.getCurrent_mA();
    voltage = ina219.getBusVoltage_V();
    delay(100);

}
