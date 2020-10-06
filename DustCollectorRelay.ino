/* Dust Collector Relay
 * 
 * Used to turn on/off the dust collector via WiFi from remotes at each machine
 * This program runs at the dust collector and controls the power relay.
 * A separate program runs in each remote. Remotes are located by each machine
 * so the collection system can be turned on conveniently.
 * 
 * Written by Doug Kimber
 */

// This program is to be uploaded onto the ESP32 arduino.
// Board type is WEMOS LOLIN32 


#include <WiFi.h>
#include "SSD1306.h"    // Header file for OLED

// Declare the OLED Display object
SSD1306 display( 0x3c, 5, 4 );

// Arrays to store the strings on each line of the display
// Max is 4 lines of 16 chars (really more like 14 chars)
#define DISP_WIDTH 16
#define DISP_LINES 4
#define LINE_HEIGHT 16    // A line is 16 pixels tall

typedef struct Line
{
  int offset; // Vertical offset in pixels of this line
  char string[ DISP_WIDTH ];
} LINE;

LINE Lines[ DISP_LINES ];

int lineCount;  // Current # of displayed lines

#define AP_SSID "DustCollector"
#define AP_PWD "Breathe"
#define PORT 23

/* Set these to your desired AP credentials. */
const char *APssid = AP_SSID;
const char *APpassword = AP_PWD;
int port = PORT;

// pin assignments
const int RelayPin = 16;

// Buffer for an incoming line from the serial port
// Serial I/O is event (interrupt) driven
char lineBuf[ 256 ];
int lineIdx;

// declare telnet server (do NOT put in setup())
WiFiServer TelnetServer( port );
WiFiClient Telnet;

// Implement a scrolling display - scrolls lines up the screen
void DisplayLine( char* theLine )
{
  int i;

  // If this is the first line then clear the display
  if( lineCount == 0 )
    display.clear();
    
  // Decide if we need to scroll lines up
  if( lineCount >= DISP_LINES )
  {
    // Need to scroll up. Delete top line and move all others up
    for( i = 0; i < DISP_LINES - 1; i++ )
    {
      strcpy( Lines[ i ].string, Lines[ i + 1 ].string );
    }
    strcpy( Lines[ DISP_LINES - 1 ].string, theLine );

    // Now display the lines
    display.clear();
    for( i = 0; i < DISP_LINES; i++ )
    {
      display.drawString( 0, Lines[ i ].offset, Lines[ i ].string );
    }
  }
  else
  {
    // Save the characters of the new line
    strcpy( Lines[ lineCount ].string, theLine );
    display.drawString( 0, Lines[ lineCount ].offset, theLine );
    lineCount++;
  }
  display.display();
}

void doChar( char c )
{
  // Is this the end of a line?
  if( (c == '\n') || (c == '\r') )
  {
    // This is the end of an input line
    processLine();
    lineIdx = 0;
  }
  else
  {
    lineBuf[ lineIdx ] = c;
    lineIdx++;
    lineBuf[ lineIdx ] = '\0';  // Null terminate for safety
  }
 
}

void handleTelnet(){
  if( TelnetServer.hasClient() ){
    // client is connected
    if( !Telnet || !Telnet.connected() ){
      if( Telnet ) Telnet.stop();          // client disconnected
      Telnet = TelnetServer.available(); // ready for new client
    } else {
      TelnetServer.available().stop();  // have client, block new conections
      DisplayLine( "New Connect" );
      Telnet = TelnetServer.available(); // ready for new client
    }
  }

  if( Telnet && Telnet.connected() && Telnet.available()){
    // client input processing
    while( Telnet.available() )
    {
      char c = Telnet.read();
      c = c & 0x7f;         // Strip parity bit
      doChar( c );  // Add character to the buffer, handle command if command is complete
    }
  } 
}

void startAP()
{
  Serial.println();
  Serial.println("Starting AP");

  WiFi.mode( WIFI_AP );
  IPAddress staticIP( 192, 168, 10, 2 );
  IPAddress gateway( 192, 168, 10, 1 );
  IPAddress subnet( 255, 255, 255, 0 );
  WiFi.softAPConfig( staticIP, gateway, subnet );
  //WiFi.softAPConfig( staticIP );
  WiFi.softAP( APssid ); // Open network without passing password parameter

  //WiFi.softAPConfig( staticIP, gateway, subnet );
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  //Serial.print( "IP address: " );
  //Serial.println( staticIP );
  //Serial.println( WiFi.localIP() );
  
  display.drawString( 0, Lines[ 1 ].offset, staticIP.toString() );
  display.display();
  
  delay(1000); // ap delay
}

void StartDisplay()
{
  display.init();
  display.flipScreenVertically();
  display.setFont( ArialMT_Plain_16 );  
}
void setup()
{
  int i;

  Serial.begin( 115200 );
  
  pinMode( RelayPin, OUTPUT ); 

  delay(1000); // serial delay

  //Set up the array that holds line characters so we can scroll
  for( i = 0; i < DISP_LINES; i++ )
  {
    Lines[ i ].offset = i * LINE_HEIGHT;
  }
  lineCount = 0;

  StartDisplay();
  display.drawString( 0, 0, "Dust Collector" );
  display.display();

  startAP();   // startup is async

  TelnetServer.begin( );
  
  Serial.print("Starting telnet server on port " + (String)port);
  //TelnetServer.setNoDelay(true); // ESP BUG ?
  Serial.println();
  delay(100);

  // Initialize global variables
  lineIdx = 0;
}

// Process a single command line
void processLine()
{
  int val;

  DisplayLine( lineBuf );

  // Accept only 2 commands, ignore everything else
  // One side of relay is connected to power, the other to RelayPin
  // Thus, driving RelayPin low turns on the relay
  if( strcmp( lineBuf, "DustFree" ) == 0 )
  {
    // Turn on the relay
    Serial.println("Relay On!");
    digitalWrite (RelayPin, LOW);
  }

  if( strcmp( lineBuf, "Dusty" ) == 0 )
  {
    // Turn on the relay
    Serial.println("Relay Off!");
    digitalWrite (RelayPin, HIGH);
  }

  lineIdx = 0;
}

int loopCount = 0;

void loop()
{
  // See if we got any wireless data
  handleTelnet();

  delay(200); // Give recipient time to process any transmissions

  // Restart WiFi and the display periodically
  loopCount++;
  if( loopCount >= 500 ) // > 10 seconds with 200mS delay above
  {
    Serial.println( "Restarting Display" );
    StartDisplay();
    Serial.println( "Restarting WiFi" );
    //ESP.restart();    // Can't reboot - need to rpeserve the relay state
    Telnet.stop();
    WiFi.disconnect();
    startAP();   // startup is async
    delay( 1000 );
    TelnetServer.begin( );
    delay( 1000 );
    loopCount = 0; 
  }

  delay( 1000 );
}
