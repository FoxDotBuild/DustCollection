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

#define ROTATION 270

#if ROTATION == 270
// 270 degree servo values
#define MIN_ROT 1000
#define MAX_ROT 1700    // 1667 is calculated value, but go a little extra for mechanical slop
#else
// Assume 180 degree servo if not 270
#define MIN_ROT 1000
#define MAX_ROT 2000    // 
#endif

#define AP_SSID "DustCollector"
#define AP_PWD "Breathe"

#define PORT 23

// Info for the servo that controls the blast gate
#define SERVO_PIN 4
Servo GateServo;

// Analog input pin for current measurements
#define ANALOG_IN A0

// Digital Pins that set the ID (0 to 3) for this node
// The Node ID is not used in the current version
#define ID0 12
#define ID1 14
#define MANUAL 13   // Manual activation pin

int NodeID; // ID of this node (0-3), set by jumpers. For future use.

// Number of samples to include in the average
#define AVG_COUNT 10

// Zero current Analog In value
#define ZERO_CURRENT 512

// Current Threshold - turn on above this value
#define CUR_THRESH 40

// Set these to match the Dust Collector Relay
char APssid[] = AP_SSID;
char APpassword[] = AP_PWD;
int port = PORT;

// Heartbeat variables
unsigned long LastHB;     // Time in mS since last heartbeat
#define HB_INTERVAL 300  // Interval in mS between heartbeats
char HB[] = "HB\n";

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
    Telnet.setNoDelay( true );
  }
  else
  {
    Serial.println( "Telnet Connection Failed!" );  
  }
}

void ConnectWiFi()
{
  WiFi.setAutoConnect( true );
  delay( 100 );
  //WiFi.setAutoConnect( false );     // Reconnect via code
  WiFi.setSleepMode( WIFI_NONE_SLEEP ); 
  delay( 100 );
  WiFi.hostname( "APssid" );        // This is necessary, but not clear why 
  delay( 100 );
  WiFi.disconnect();
  delay( 100 );
  WiFi.begin( APssid );             // Connect to the network

  delay( 1000 );  // Forum suggestion to avoid connection drops

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
  Serial.println("Connection, ");  
  Serial.print("IP:\t");
  Serial.println( WiFi.localIP() );         // Send the IP address of the ESP8266 to the computer

  delay( 100 );
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
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  delay(100);
  Serial.println("Setting Up!\n");

  // Set Up the Node ID pins and calculate the Node ID
  pinMode( ID0, INPUT_PULLUP );
  pinMode( ID1, INPUT_PULLUP );
  pinMode( MANUAL, INPUT_PULLUP );

  Serial.print( "ID0:" ); Serial.println( digitalRead( ID0 ) );
  Serial.print( "ID1:" ); Serial.println( digitalRead( ID1 ) );
  Serial.print( "Manual:" ); Serial.println( digitalRead( MANUAL ) );

  NodeID = 0;
  NodeID = NodeID + digitalRead( ID0 );
  NodeID = NodeID + ( (digitalRead( ID1 ) << 1 ) & 0x02 );
  NodeID = NodeID & 0x03; // Invert. No connects pulled high. Need jumper to pulldown.

  Serial.print( "Node ID: " );
  Serial.println( NodeID );

  GateServo.attach( SERVO_PIN );
  GateServo.writeMicroseconds( MIN_ROT );    // Starting position ball valve closed

  // Rotate the valve a bit so we know the servo is working
  delay( 200 );
  GateServo.writeMicroseconds( MIN_ROT + 300 );    // Open a bit
  delay( 200 );
  GateServo.writeMicroseconds( MIN_ROT );    // Starting position ball valve closed

  CollectorOn = false;

  Telnet.setNoDelay( true );
  ConnectWiFi();

  LastHB = millis();

}

long loopCount = 0;

void loop()
{
  unsigned long HoldTime;
 
  // In Case of a WiFi disconnection, reconnect
  if( WiFi.status() != WL_CONNECTED )
  {
    ConnectWiFi();
  }
  else
  {
    if( !Telnet.connected() )
    {
      ConnectTelnet();
    }
  }

  // Are there characters coming from the server?
  while( Telnet.available() )
  {
    char c = Telnet.read(); // Ignore them. They are there to maintain the connection
  }

  // Do we need to send a heartbeat?
  if( ( millis() - LastHB ) > HB_INTERVAL )
  {
    Telnet.print( HB );
    Telnet.flush();
    LastHB = millis();        
  }

  int RMS = GetRMScurrent();
  Serial.println();
  Serial.print( "RMS: ");
  Serial.println( RMS );

  // Is the machine turned on? I.e., is it pulling any current?
  // Or has the manual override been activated?
  if( ( GetRMScurrent() > CUR_THRESH ) || ( digitalRead( MANUAL ) == 0 ) )
  {
    GateServo.writeMicroseconds( MAX_ROT ); // Open the blast gate
    Telnet.print( "DustFree\n");
    Telnet.flush();
    Serial.println("Turning On!" );
    CollectorOn = true;
  }
  else
  {
    if( CollectorOn == true )
    {
        // Keep vacuum on a while to clear hoses
      HoldTime = millis();
      while( ( millis() - HoldTime ) < 5000 )
      {
        if( ( millis() - LastHB ) > HB_INTERVAL )
        {
          Telnet.print( HB );
          Telnet.flush();
          LastHB = millis();        
        }
      }
      //delay( 5000 );
      GateServo.writeMicroseconds( MIN_ROT );  // Close the blast gate
      Telnet.print( "Dusty\n");
      Telnet.flush();
    }
    Telnet.print( "Dusty\n");
    Telnet.flush();
    CollectorOn = false;
  }
#ifdef SHOW_MANUAL  
  // Wait a small amount of time. Note that GetRMScurrent takes > 100mS
  //delay( 10 );
  if( ( loopCount++ % 100 ) == 0 )
  {
    Serial.print(".");
    Serial.print( "Manual:" ); Serial.println( digitalRead( MANUAL ) );
  }
#endif 
}
