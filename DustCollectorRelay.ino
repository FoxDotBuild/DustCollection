/* Dust Collector Relay
 * 
 * Used to turn on/off the dust collector via WiFi from remotes at each machine.
 * This program runs at the dust collector and controls the power relay.
 * A separate program runs in each remote. Remotes are located by each machine
 * so the collection system can be turned on conveniently.
 * 
 * Written by Doug Kimber
 */

// This program is to be uploaded onto the ESP32 arduino.
// Board type is WEMOS LOLIN32 

#define SERIAL_DEBUG

#include <WiFi.h>
//#include <WiFiMulti.h>
#include "SSD1306.h"    // Header file for OLED

// Declare the OLED Display object
// The display is 128x64
// The code divides it into 4 lines of 16 bits in height each
// So lines start at Y values of 0, 16, 32, and 48
SSD1306 display( 0x3c, 5, 4 );

// Bitmaps for heartbeart hearts
// Generated from http://dotmatrixtool.com/
const u_char tinyHeart[] PROGMEM =
{
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0xf0, 0x01, 0xc0, 0x03, 0xc0, 0x03, 0xf0, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const u_char smallHeart[] PROGMEM =
{
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0xf0, 0x01, 0xf8, 0x03, 0xe0, 0x07, 0xe0, 0x07, 0xf8, 0x03, 0xf0, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const u_char mediumHeart[] PROGMEM =
{
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0xf0, 0x01, 0xf8, 0x03, 0xf8, 0x07, 0xe0, 0x0f, 0xe0, 0x0f, 0xf8, 0x07, 0xf8, 0x03, 0xf0, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const u_char largeHeart[] PROGMEM =
{
0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0xf0, 0x01, 0xf8, 0x03, 0xfc, 0x07, 0xfc, 0x0f, 0xf0, 0x1f, 0xf0, 0x1f, 0xfc, 0x0f, 0xfc, 0x07, 0xf8, 0x03, 0xf0, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00
};
const u_char giantHeart[] PROGMEM =
{
0x00, 0x00, 0xe0, 0x00, 0xf0, 0x01, 0xf8, 0x03, 0xfc, 0x07, 0xfc, 0x0f, 0xfc, 0x1f, 0xf0, 0x3f, 0xf0, 0x3f, 0xfc, 0x1f, 0xfc, 0x0f, 0xfc, 0x07, 0xf8, 0x03, 0xf0, 0x01, 0xe0, 0x00, 0x00, 0x00
};

#define AP_SSID "DustCollector"
#define AP_PWD "Breathe"
#define PORT 23

/* Set these to your desired AP credentials. */
const char *APssid = AP_SSID;
const char *APpassword = AP_PWD;
int port = PORT;

// pin assignments
const int RelayPin = 16;

// declare telnet server (do NOT put in setup())
#define MAX_CLIENTS 4
WiFiServer TelnetServer( port );
WiFiClient Clients[ MAX_CLIENTS ];

// Keep track of the time since the last message from each client
#define HB_INTERVAL 3000    // Maximum Milliseconds between heartbeats
unsigned long ClientHB[ MAX_CLIENTS ];

// Heartbeat and On/Off status for each client
bool HBstat[ MAX_CLIENTS ];
bool On[ MAX_CLIENTS ];
// Locations on the display for heartbeats and channel status
typedef struct
{
  int HBx;    // X location of heartbeat icon
  int HBy;    // Y location of heartbeat icon
  int StatX;  // X location of ON/Off Status
  int StatY;  // Y location of ON/Off Status
} DISP_LOCS;
DISP_LOCS Disp_locs[ MAX_CLIENTS ] =
{
  {  0, 32, 16, 32 },
  { 64, 32, 80, 32 },
  {  0, 48, 16, 48 },
  { 64, 48, 80, 48 },

};

// Buffers for messages from each client
#define MAX_LINE 32
typedef struct
{
  int index;
  //int unload;
  char buf[ MAX_LINE ];
}BUFFER;

BUFFER bufs[ MAX_CLIENTS ];

//-------------------------------------------------------------------
void startAP()
{
  Serial.println();
  Serial.println("Starting AP");

  WiFi.mode( WIFI_AP );
  delay( 100 );
  WiFi.setSleep( false );
  delay( 100 );
  IPAddress staticIP( 192, 168, 10, 2 );
  IPAddress gateway( 192, 168, 10, 1 );
  IPAddress subnet( 255, 255, 255, 0 );
  delay( 100 );
  WiFi.softAPConfig( staticIP, gateway, subnet );
  delay( 100 );
  //WiFi.softAPConfig( staticIP );
  WiFi.softAP( APssid ); // Open network without passing password parameter
  delay( 100 );
  WiFi.begin();
  delay( 1000 );
  
  //WiFi.softAPConfig( staticIP, gateway, subnet );
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println( IP );
  
  display.drawString( 0, 16, staticIP.toString() );
  display.display();
  
  delay(1000); // ap delay
}
//-------------------------------------------------------------------
void StartDisplay()
{
  display.init();
  display.flipScreenVertically();
  display.setFont( ArialMT_Plain_16 );  
}
//-------------------------------------------------------------------
void setup()
{
  int i;

  Serial.begin( 115200 );
  
  pinMode( RelayPin, OUTPUT ); 
  digitalWrite (RelayPin, HIGH );

  delay( 1000 ); // serial delay

  StartDisplay();
  display.drawString( 0, 0, "Dust Collector" );
  display.display();

  startAP();   // startup is async

  TelnetServer.begin( );
  //TelnetServer.setNoDelay( true );
  
  Serial.print("Starting telnet server on port " + (String)port);
  TelnetServer.setNoDelay( true ); // ESP BUG ?
  Serial.println();

  // Initialize the heartbeat timers, etc.
  for( int i = 0; i < MAX_CLIENTS; i++ )
  {
    //Clients[ i ] = nullptr;
    ClientHB[ i ] = millis();
    bufs[ i ].index = 0;
    HBstat[ i ] = false;
    On[ i ] = false;
    drawHB( i );
    drawStatus( i );
  }

  delay(100);
}
//-------------------------------------------------------------------
void drawHB( int index )
{
  int x, y;

  x = Disp_locs[ index ].HBx;
  y = Disp_locs[ index ].HBy;
  if( HBstat[ index ] == false )
  {
    display.setColor( BLACK );
    display.fillRect( x, y, 16, 16 );
    display.setColor( WHITE );
    display.drawFastImage( x, y, 16, 16, (const char*)smallHeart );
    display.display();
  }
  else
  {
    display.setColor( BLACK );
    display.fillRect( x, y, 16, 16 );
    display.setColor( WHITE );
    display.drawFastImage( x, y, 16, 16, (const char*)giantHeart );
    display.display();
  }
}
//-------------------------------------------------------------------
void drawStatus( int index )
{
  int x, y;

  x = Disp_locs[ index ].StatX;
  y = Disp_locs[ index ].StatY;
  if( On[ index ] == false )
  {
    display.setColor( BLACK );
    display.fillRect( x, y, 32, 16 );
    display.setColor( WHITE );
    display.drawString( x, y, "Off" );
    display.display();
  }
  else
  {
    display.setColor( BLACK );
    display.fillRect( x, y, 32, 16 );
    display.setColor( WHITE );
    display.drawString( x, y, "On" );
    display.display();
  }
}
//-------------------------------------------------------------------
void TurnOff()
{    
  // Turn off the relay only if all Clients agree
  bool TurnOff = true;
  for( int i = 0; i < MAX_CLIENTS; i++ )
  {
    if( On[ i ] == true )
      TurnOff = false;
  }
  if( TurnOff == true )
  {
    Serial.println( "All Off!" );
    digitalWrite ( RelayPin, HIGH ) ;
  }
}
//-------------------------------------------------------------------
// Process a single command line
void processLine( char* Buf, int index )
{
  // Accept only 3 commands, ignore everything else
  // One side of relay is connected to power, the other to RelayPin
  // Thus, driving RelayPin low turns on the relay
  if( strcmp( Buf, "HB\n" ) == 0 )
  {
    // Toggle between small and large hearts to show beating
    if( HBstat[ index ] == false )
      HBstat[ index ] = true;
    else
      HBstat[ index ] = false;
    drawHB( index);
    Serial.print(" HB");
    Serial.print( index );
    Serial.print( " " );
  }
  else if( strcmp( Buf, "DustFree\n" ) == 0 )
  {
    // Turn on the relay
    //Serial.println( "On!" );
    digitalWrite ( RelayPin, LOW );
    // Set and Display the Status
    On[ index ] = true;
    drawStatus( index );
  }
  else if( strcmp( Buf, "Dusty\n" ) == 0 )
  {
    // Set and Display the Status
    On[ index ] = false;
    drawStatus( index );

    TurnOff();  // Turn off if no client is asking for On
  }
  Clients[ index ].print( "Ack\n" );  // Try to keep the connection up
}
//-------------------------------------------------------------------
void AddClient( int index )
{
  // Accept a newclient  
  Clients[ index ] = TelnetServer.accept();
  //Clients[ i ] = TelnetServer.available();
  Serial.print("New client: ");
  Serial.println( index );  
}
//-------------------------------------------------------------------
unsigned long loopCount = 0;

void loop()
{
  // See if there are any new clients
  if( TelnetServer.hasClient() )
  {
    // find an empty slot if there is one
    int i;
    for( i = 0; i < MAX_CLIENTS; i++ )
    {
      if (!Clients[ i ] || !Clients[i].connected() )
      {
        if( Clients[ i ] )
          Clients[ i ].stop();
        On[ i ] = false;
        Clients[ i ] = TelnetServer.accept();
        if( !Clients[ i ] )
          Serial.println("available broken");
        Serial.print( "New client: " );
        Serial.print( i ); Serial.print(' ');
        Serial.println( Clients[ i ].remoteIP() );
        break;
      }      
    }
    // Maximum number of clients already connected
    if( i == MAX_CLIENTS )
    {
      //TelnetServer.accept().println("busy");
      TelnetServer.accept().stop();
      // hints: server.accept() is a WiFiClient with short-term scope
      // when out of scope, a WiFiClient will
      // - flush() - all data will be sent
      // - stop() - automatically too
      Serial.println( "Max # of clients already connected" );
    }
  }

  // Check for incoming data from each connected client
  for( int i = 0; i < MAX_CLIENTS; i++ )
  {
    if( Clients[ i ] && Clients[ i ].connected() )
    {
      if( Clients[ i ].available() )
      {
        while( Clients[ i ].available() )
        {
          yield();
          
          // Get input one character at a time
          char c =  Clients[ i ].read();
          //Serial.write( c );

          // Save the input character and increment the buffer index
          bufs[ i ].buf[ bufs[ i ].index ] = c;
          bufs[ i ].index++;
          bufs[ i ].buf[ bufs[ i ].index ] = '\0'; // Null terminate

          // Check for newline or carriage return
          if( ( c == '\n' ) || ( c == '\r' ) )
          {
            processLine( bufs[ i ].buf, i );
            bufs[ i ].index = 0;
          }

          // Check for overflow
          if( bufs[ i ].index >= MAX_LINE )
          {
            Serial.print( "Buffer Overflow, Index = " );
            Serial.print( i );
          }
      
          // Update the heartbeat timers since we got data from this client
          ClientHB[ i ] = millis();
        }
      }
    }
    else
    {
      if( Clients[ i ] )
        Clients[ i ].stop();
      HBstat[ i ] = false;
      On[ i ] = false;
    }
  }

  // Have we lost heartbeat from any of the clients?
  for( int i = 0; i < MAX_CLIENTS; i++ )
  {
    if( !Clients[ i ] || !( Clients[ i ].connected() ) )
    {
      continue;
    }
    if( ( millis() - ClientHB[ i ] ) > HB_INTERVAL  )
    { 
      Serial.print( "Lost HB: " );
      Serial.print( i );
      Clients[ i ].stop();
      HBstat[ i ] = false;
      On[ i ] = false;
      TurnOff();    // Turn off if no client is requesting On
      drawHB( i );
      drawStatus( i );
    }
    yield();
  }

  // Print the number of connections for debugging
  if( loopCount > 10000 )
  {
    loopCount = 0;
    int Connected = 0;
    for( int i = 0; i < MAX_CLIENTS; i++ )
    {
      if( Clients[ i ] && ( Clients[ i ].connected() ) )
        Connected++;
    }
    display.setColor( BLACK );
    display.fillRect( 0, 16, 128, 16 );
    display.setColor( WHITE );
    display.drawString( 0, 16, String( Connected ) );
    display.drawString( 10, 16, "Connected" );
    display.display();

#ifdef SERIAL_DEBUG   
    Serial.print( "Connections: " );
    Serial.println( Connected );
#endif
  }
  
  loopCount++;
}
