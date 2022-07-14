# DustCollection

There are 3 files here:

DustCollectionRelay runs on an ESP32 with an OLED. The board type is "LOLIN ESP32". This board sets up a Soft Access Point and looks for Telnet messages to turn the relay either on or off. The remotes connect to the access point and send Telnet messages (ASCII strings) to tell the ESP32 to turn the relay (and thus the dust collector) on or off.

DustCollectionRemote runs on an ESP8266. The board type is "LOLIN(WEMOS) D1 R1 & mini". This board connects to the ESP32 via WiFi. Based on button pushes it sends Telnet commands to either turn on or off the relay. This version uses 2 buttons, one for 'ON' and one for 'OFF', to send the corresponding messages to the ESP32 relay board. This allows a manual control of the dust collector from a distance. Note that any sorresponding blast gates must also be configured manually.

DustCollectionServo runs on an ESP8266. It uses a Hall effect sensor to determine when a machine plugged into it is turned on. It then sends a Telnet message to the ESP32 relay  board to turn the dust collector on. Simultaneously it sends servo control to an RC hobby servo to automatically open a ball valve blast gate. After the connected machine is turned off there is a delay before sending the 'OFF' command to the relay board, This allows the dust collector time to evacuate the ductwork between the machine and the dust collector.

