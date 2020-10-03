/* Dust Collector Remote
 *
 * Used to turn on/off the dust collector via WiFi from remotes at each machine
 * This program is the remote control.
 * A separate program runs at the dust collector. Remotes are located by each machine
 * so the collection system can be turned on conveniently.
 *
 * Written by Doug Kimber
 */

// This program is to be uploaded onto the ESP8266 arduino.

#include <ESP8266WiFi.h>

#define AP_SSID "DustCollector"
#define AP_PWD "Breathe"

#define PORT 23

// Set these to match the Dust Collector Relay
char *APssid = AP_SSID;
char *APpassword = AP_PWD;
int port = PORT;

// ??? pin assignments
const int OffPin = 4;
const int OnPin = 5;

int Status = WL_IDLE_STATUS;
IPAddress Server(192, 168, 10, 2); // Dust Collector Relay

// declare telnet client (do NOT put in setup())
WiFiClient Telnet;

void ConnectTelnet()
{
  if (Telnet.connect(Server, 23))
  {
    Serial.println("Telnet Connected");
  }
  else
  {
    Serial.println("Telnet Connection Failed!");
  }
}

void ConnectWiFi()
{
  //bool Auto = WiFi.getAutoConnect();
  //Serial.print( "Autoconnect: " );
  //Serial.println( Auto );

  WiFi.setAutoConnect(false); // Reconnect via code
  WiFi.hostname("APssid");    // This is necessary, but not clear why
  WiFi.begin(APssid);         // Connect to the network

  Serial.print("Connecting to ");
  Serial.print(APssid);
  Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i);
    Serial.print(' ');
  }

  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer

  delay(4000);
  ConnectTelnet();
}
void setup()
{
  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(500);
  Serial.println('\n');

  // Make the On and Off pins inputs
  pinMode(OnPin, INPUT_PULLUP);
  pinMode(OffPin, INPUT_PULLUP);

  ConnectWiFi();
}

void loop()
{
  // Check for either the ON or OFF buttons being pressed

  // In Case of a WiFi disconnection, reconnect
  if (WiFi.status() != WL_CONNECTED)
  {
    ConnectWiFi();
  }
  else
  {
    if (Telnet.connected() != true)
    {
      ConnectTelnet();
    }
  }

  if (digitalRead(OffPin) == LOW)
  {
    Serial.println("Turning Off!");
    Telnet.print("Dusty\n");
    delay(1000);
  }

  if (digitalRead(OnPin) == LOW)
  {
    Serial.println("Turning On!");
    Telnet.print("DustFree\n");
    delay(1000);
  }

  delay(1000);
}
