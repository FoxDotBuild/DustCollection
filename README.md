# DustCollection

There are 2 files here:

DustCollectionRelay runs on an ESP32 with an OLED. The board type is "LOLIN ESP32". This board sets up a Soft Access Point and looks for Telnet messages to turn the relay either on or off.

DustCollectionRemote runs on an ESP8266. The board type is "LOLIN(WEMOS) D1 R1 & mini". This board connects to the ESP32 via WiFi. Based on button pushes it sends Telnet commands to either turn on or off the relay.
