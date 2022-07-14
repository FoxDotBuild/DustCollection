/* Dust Collector Remote
 * 
 * Used to turn on/off the dust collector via WiFi from remotes at each machine
 * This program is the remote control.
 * A separate program runs at the dust collector. Remotes are located by each machine
 * so the collection system can be turned on conveniently.
 * 
 * This version detects a machine being turned on via an ACS712 current sensor.
 * It monitors the RMS current and when it goes over a threshold sends a message to turn on the dust collector.
 * 
 * Each machine has a separate ID programmable by DIP switch or shunts.
 * 
 * Written by Doug Kimber
 */

// This program is to be uploaded onto the ESP8266 arduino.
// Board type is LOLIN(WEMOS) D1 R2 & mini 


#include <ESP8266WiFi.h>
#include <Servo.h>

#define AP_SSID "DustCollector"
#define AP_PWD "Breathe"

#define PORT 23

// Info for the servo that controls the blast gate
#define SERVO_PIN 4
Servo GateServo;

// Analog input pin for current measurements
#define ANALOG_IN A0

// Digital Pins that set the ID (0 to 15) for this node
#define ID0 12
#define ID1 13
#define ID2 14
//#define ID3 14

int NodeID; // ID of this node (0-15), set by jumpers

// Number of samples to include in the average
#define AVG_COUNT 100

// Zero current Analog In value
#define ZERO_CURRENT 512

// Set these to match the Dust Collector Relay
char APssid[] = AP_SSID;
char APpassword[] = AP_PWD;
int port = PORT;

// Flag to indicate we were just on
bool CollectorOn;

int Status = WL_IDLE_STATUS;
IPAddress Server( 192, 168, 10, 2 );    // Dust Collector Relay
#define RETRIES 100

// declare telnet client (do NOT put in setup())
WiFiClient Telnet;

void ConnectTelnet()
{
  if( Telnet.connect( Server, 23 ) )
  {
    Serial.println( "Telnet Connected" );
  }
  else
  {
    Serial.println( "Telnet Connection Failed!" );  
  }
}

void ConnectWiFi()
{
  //bool Auto = WiFi.getAutoConnect();
  //Serial.print( "Autoconnect: " );
  //Serial.println( Auto );
  
  WiFi.setAutoConnect( false );     // Reconnect via code  
  WiFi.hostname( "APssid" );        // This is necessary, but not clear why  
  WiFi.begin( APssid );             // Connect to the network

  Serial.print("Connecting to ");
  Serial.print( APssid ); Serial.println(" ...");

  // Wait for the Wi-Fi to connect. This can take a while
  int i = 0;
  while( (WiFi.status() != WL_CONNECTED) && ( i < RETRIES ) )
  { 
    delay(200);
    Serial.print(++i); Serial.print('_');
    i++;
  }
  if( i >= RETRIES )
     return;
    
  Serial.println('\n');
  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println( WiFi.localIP() );         // Send the IP address of the ESP8266 to the computer

  delay( 100 );
  ConnectTelnet(); 
}

// Read the current from the ACS712 sensor.
// An RMS value is returned 
int GetRMScurrent()
{
  float Sum = 0;
  float Sample;
  int i;

   for( i = 0; i < AVG_COUNT; i++ )
   {
     Sample = analogRead( ANALOG_IN ) - ZERO_CURRENT; // Get the sample, subtract the 0 offset
     Sample = Sample * Sample;    // Square the value to make it positive
     Sum = Sum + Sample;
     delay( 1 );    // Wait 1 mS before getting the next sample
   }

   Sum = Sum / AVG_COUNT;   // Get the average of the squared values
   return( sqrt( Sum ) );     // and return the root mean square
}

void setup()
{
  Serial.begin(9600);         // Start the Serial communication to send messages to the computer
  delay(100);
  Serial.println("Setting Up!\n");

  // Set up the Servo
  //GateServo.attach( SERVO_PIN );
  
  // Set Up the Node ID pins and calculate the Node ID
  pinMode( ID0, INPUT_PULLUP );
  pinMode( ID1, INPUT_PULLUP );
  pinMode( ID2, INPUT_PULLUP );
  //pinMode( ID3, INPUT_PULLUP );
  Serial.print( "ID0:" ); Serial.println(digitalRead( ID0 ));
  Serial.print( "ID1:" ); Serial.println(digitalRead( ID1 ));
  Serial.print( "ID2:" ); Serial.println(digitalRead( ID2 ));
  //Serial.print( "ID3:" ); Serial.println(digitalRead( ID3 ));
  NodeID = 0;
  NodeID = NodeID + digitalRead( ID0 );
  NodeID = NodeID + ( (digitalRead( ID1 ) << 1 ) & 0x02 );
  NodeID = NodeID + ( (digitalRead( ID2 ) << 2 ) & 0x04 );
  //NodeID = NodeID + ( (digitalRead( ID3 ) << 3 ) & 0x08 );
  NodeID = NodeID ^ 0x07; // Invert. No connects pulled high. Need jumper to pulldown.

  Serial.print( "Node ID: " );
  Serial.println( NodeID );

  GateServo.attach( SERVO_PIN );
  GateServo.writeMicroseconds( 1000 );    // Starting position is 45* - stay away from ends of servo rotation
  
  ConnectWiFi();

  CollectorOn = false;
}

void loop()
{
  // In Case of a WiFi disconnection, reconnect
  if( WiFi.status() != WL_CONNECTED )
  {
    ConnectWiFi();
  }
  else
  {
    if( Telnet.connected() != true )
    {
      ConnectTelnet();
    }
  }

  // Is the machine turned on? I.e., is it pulling any current?
  if( GetRMScurrent() > 80 )
  {
    Serial.println( "Machine is On!" );
    GateServo.writeMicroseconds( 2000 ); // Open the blast gate
    Telnet.print( "DustFree\n");
    CollectorOn = true;
  }
  else
  {
    Serial.println( "Machine is Off." );
    if( CollectorOn == true )
    {
      delay( 5000 );  // Keep vacuum on a while to clear hoses
      GateServo.writeMicroseconds( 1000 );  // Close the blast gate
      Telnet.print( "Dusty\n");
    }
    Telnet.print( "Dusty\n");
    CollectorOn = false;
  }
  
  // Wait a small amount of time. Note that GetRMScurrent takes > 100mS
  delay( 10 );
  Serial.print(".");

  

}
