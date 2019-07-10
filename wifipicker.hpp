#include "WiFi.h"
#include "SPIFFS.h"
#include "ESPAsyncWebServer.h"

#include <AsyncTCP.h>
#include <DNSServer.h>

#include <EEPROM.h>

DNSServer dnsServer;
AsyncWebServer wifiChooserServer(80);

char ssid[32] ;
char pass[63] ;
String networklist = "";

bool APConfigMode = false;


/**
   Scans for available wifi networks
   &answer is a refrence to a string in which the list of available networks will be placed, each line containing one network, including the signal strengths of the networks and whether or not they are password-protectedd
   The returned boolean returns whether the global ssid variable is present in the list.
*/
bool netscan(String &answer) {
  answer = "";
  bool foundPrevnetwork = false;
  Serial.println("received refresh");
  int n = WiFi.scanNetworks(false, false, false, 150);
  if (n > 0)  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      String protection = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "0" : "1";
      answer += WiFi.SSID(i) + "\t" + WiFi.RSSI(i) + "\t" + protection + "\n";
      if (WiFi.SSID(i) == ssid)foundPrevnetwork = true;
    }
  }
  int lastIndex = answer.length() - 1;
  answer.remove(lastIndex);
  return foundPrevnetwork;
}




/**
   This is a long lasting routine that terminates only when the ESP32 has connected to a network.
   1. In the first step, the ESP32 tries to load a previously saved ssid from the first 32 bytes in the EEPROM, and its associated password from the following 63 bytes.
   2. It then scans the available networks, and checks if the available networks contain the previously saved SSID. If present, go to 3. Else, go to 4.
   3. If the available network is present, try to connect to it. If successful, terminate. Else, go to 4.
   4. Create a WiFi access point to which a user can connect. The access point will use a captive portal to redirect the user to its webpage. This webpage is saved in the SPIFFS, and has a number of features to help establish a connection.
      The webpage comes with a few buttons. First of all a refresh button, which refreshed the list of networks, each of which can be clicked on to connect to. The second button can be used to manually set up an SSID and password. This can be useful for hidden SSIDs.
      When a the command has been given to try connecting to a webpage using this webpage, go to 5.
   5. Try to connect to the given network. If successful, save the network credentials in EEPROM and terminate wifiPicker(). If unsuccessful, go to 2, then 4, always skipping 3.

*/
void wifiPicker() {
  //load WiFi credentials from EEPROM
  EEPROM.begin(63 + 32);
  EEPROM.readString(0, ssid, 32);
  EEPROM.readString(32, pass, 63);

  Serial.print("former SSID = ");
  Serial.println(ssid);
  Serial.print("former pass = ");
  Serial.println(pass);


  //Scan available WiFi networks.
  networklist = "";
  bool prevnetfound = netscan(networklist);

  //If we the previously loaded SSID is present in the SSID list, and we didn't come from step 5, try to connect to it.
  if (prevnetfound && !APConfigMode) {
    Serial.println("found prevnet");
    WiFi.begin(ssid, pass);

    int loops = 0;
    while (prevnetfound && WiFi.status() != WL_CONNECTED) {
      if (loops > 10) {
        WiFi.mode(WIFI_OFF);
        prevnetfound = false;
      } else {
        delay(1000);
        Serial.println("Connecting to WiFi..");
        loops++;
      }
    }
  }


  //If connection succeeded, print IP address and terminate, else launch the access point for selecting a WiFi network.
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(WiFi.localIP());
  } else {

    APConfigMode = true;

    if (!SPIFFS.begin()) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }

    //Start AP Mode
    Serial.println("AP mode");
    WiFi.softAP("ESP32 WiFi picker");
    dnsServer.start(53, "*", WiFi.softAPIP());  //Start DNS server (used for captive portal)


    //handlers for various requests

    //Requests of HTML/CSS/JS/TTF/ICO files:
    wifiChooserServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/index.html", "text/html");
    });

    wifiChooserServer.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/index.html", "text/html");
    });

    wifiChooserServer.on("/metro.min.css", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/metro.min.css", "text/css");
    });

    wifiChooserServer.on("/metro.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/metro.min.js", "text/js");
    });

    wifiChooserServer.on("/jquery-3.3.1.min.js", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/jquery-3.3.1.min.js", "text/js");
    });

    wifiChooserServer.on("/metro.ttf", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/metro.ttf", "text/ttf");
    });

    wifiChooserServer.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(SPIFFS, "/favicon.ico", "icon");
    });

    //END OF RESOURCE REQUESTS

    //Refresh button pressed. generate a new list of available WiFi networks
    wifiChooserServer.on("/refresh", HTTP_POST, [](AsyncWebServerRequest * request) {
      Serial.println("received refresh");
      String answer = "";
      netscan(answer);
      networklist = answer;
      request->send(200, "text/plain", answer);
    });

    //Client just connected, and wants the current list of available networks. Don't generate a new list but send the current one.
    wifiChooserServer.on("/firstrefresh", HTTP_POST, [](AsyncWebServerRequest * request) {
      Serial.println("received firstrefresh");
      request->send(200, "text/plain", networklist);
    });

    //Client wants the ESP32 to connect to a network
    wifiChooserServer.on("/connectnetwork", HTTP_POST, [](AsyncWebServerRequest * request) {

      request->arg("ssid").toCharArray(ssid, sizeof(ssid));
      request->arg("pass").toCharArray(pass, sizeof(pass));
      Serial.println("received connect network");
      Serial.println(ssid);
      if (pass == "") {
        Serial.print("nopass");
      }
      Serial.println(pass);



      APConfigMode = false;

      request->send(200);
    });
    //END OF HANDLER SECTION

    wifiChooserServer.begin();

    Serial.println(WiFi.localIP()); //Start the webserver


    //Some DNS shenanigans, needed for captive portal. This will continue to run until we're done with our nwtwork selection, thus halting the code up to this point
    while (APConfigMode) {
      dnsServer.processNextRequest();
    }

    //We've selected a network, stop the webserver and DNS server and disconnect WiFi
    wifiChooserServer.end();
    dnsServer.stop();
    WiFi.mode(WIFI_OFF);

    //Try to connect to the selected network
    WiFi.begin(ssid, pass);

    int loops = 0;
    while (loops < 15 && WiFi.status() != WL_CONNECTED) {

      delay(1000);
      Serial.println("Connecting to WiFi..");
      loops++;

    }


    if (WiFi.status() != WL_CONNECTED) {  //If connection to the selected network has failed, restart the process again
      APConfigMode = true;
      wifiPicker();
    } else {  //If successful, save the WiFi credentials, and terminate
      char  tempssid[32] = "";
      char  temppass[63] = "";
      EEPROM.readString(0, tempssid, 32);
      EEPROM.readString(32, temppass, 63);

      if (strcmp( tempssid, ssid ))EEPROM.writeString(0, ssid);
      if (strcmp( temppass, pass ))EEPROM.writeString(32, pass);

      if (strcmp( tempssid, ssid ) || strcmp( temppass, pass )) {
        EEPROM.commit();
        Serial.println(WiFi.localIP());
      }
    }
  }

}
