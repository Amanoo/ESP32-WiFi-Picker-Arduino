# ESP32-WiFi-Picker-Arduino
Keeps the last connected WiFi network in EEPROM. If connection to this network fails or the network is not found, it creates a WiFi access point with captive portal for selecting a new WiFi network that the ESP32 will then try to connect to.

Built for use in Arduino IDE.
Depends on the ESPAsyncWebServer and AsyncTCP libraries by me-no-dev
https://github.com/me-no-dev/ESPAsyncWebServer
https://github.com/me-no-dev/AsyncTCP

To use, simply include "wifipicker.hpp", and add "wifiPicker();" in the setup, like so:

```C++
#include "wifipicker.hpp"
void setup() {
  wifiPicker();
  // put the rest of your setup code here.
}```

wifiPicker() will terminate once connection to a WiFi network has been established.
