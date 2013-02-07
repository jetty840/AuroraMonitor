//
//  Aurora Monitor - Ethernet Version
//  Arduino Ethernet / Wifi Aurora Monitor and twitter feed (http://www.thingiverse.com/thing:25528)
//
//  Copyright: Jetty, June 2012
//  License: GNU General Public License v3: http://www.gnu.org/licenses/gpl-3.0.txt
//
//  Requires Arduino 1.0.1 or later IDE
//  Requires Arduino Ethernet or Arduino with Ethernet Shield
//  Tested on:  Arduino Ethernet: http://arduino.cc/en/Main/ArduinoBoardEthernet
//
//
//  Requirements:
//
//     Libraries:
//        Requires Time / TimeAlarms / MemoryFree, which can be obtained here:
//        https://github.com/jetty840/AuroraMonitor
//
//     Twitter:
//        To post aurora updates to twitter, you need to setup a twitter account
//        at http://www.twitter.com
//
//        You also need to setup ThingTweet which acts as a bridge between your arduino
//        and twitter.
//
//        Setting up ThingTweet: 
//           Visit: http://www.thingspeak.com/
//           Click "Sign up", create username / password and record somewhere safe,
//           don't use the same username/password as your twitter account.
//           Click on "Apps", click "Link Twitter Account", click "Authorize App"
//           Click "Back to ThingTweet"
//           Copy the API key, and enter as THINGTWEET_API_KEY below
//
//        Disabling twitter:
//           If you don't wish to use twitter, comment out #define TWITTER below
//
//
//  LED Usage:
//     Note about LED usage, larger LEDs tend to use more current and there's
//     a limit to how much current the Arduino can source.  Pins are rated for 40ma
//     max on newer Arduinos, but due to using an RGB led and another LED, 4 x 40 may
//     exceed the current that can be supplied via the power pins to the Arduino.
//     Therefore it's generally considered safe to limit usage to 20ma per pin with a serial
//     resistor.  
//
//     The calculation for this resistor is (and you'll need 1 per color) is:
//     Resistor (Ohms) = (5Volt - LED Forward Voltage Drop (per color)) / 0.020 (Current Amps)
//
//     For example, if the forward voltage of the LED is 2.2V, then:   (5-2.2) / 0.020 = 140 Ohms
//     If you have any doubt about this calculation, a 220 Ohm resistor will work with any LED that
//     can handle 22ma or more.
//
//     RGB Led's can be Common Cathode or Common Anode, set the appropriate type in the LED settings.
//     Also, RGB led's have various spectrums, forward voltages etc., all of which effect the color, you
//     will likely need to tune the color table below for your LED
//



#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <SPI.h>
#include <Ethernet.h>
#include <dns.h>
#include <Time.h>
#include <TimeAlarms.h>



////// BEGIN NETWORK SETTINGS
byte mac[] = { 0x13, 0x14, 0x16, 0x18, 0x20, 0x21 };  //Mac address of ethernet shield, for newer shields/boards, use the number on the sticker on the back
//If there's no sticker, make one up, making sure it's 6 bytes long.

//DHCP increases the sketch size siginificantly, and increases startup time
// comment out the following line if you don't use DHCP, and set the ip
//gateway, dns and subnet mask, otherwise leave uncommented
#define DHCP

#ifdef DHCP
#define DHCP_LEASE_RENEW_INTERVAL  (60*60)    //How often to renew the DHCP lease (in seconds)
#else
//The following are only required to be set if #define DHCP is commented out
//Fill this out with your network details
IPAddress           localIP( 192, 168, 1, 189 );
byte      localGateway[] = { 192, 168, 1, 1 };
byte       localSubnet[] = { 255, 255, 255, 0 };
byte          localDns[] = { 192, 168, 1, 1 };
#endif
////// END NETWORK SETTINGS


////// BEGIN TWITTER SETTINGS
#define TWITTER                                   //Comment out if you don't want to update to twitter                      
#define THINGTWEET_API_KEY "29836AGI9020293Q"     //Obtained by following the instructions at the top of this file
IPAddress thingSpeakIP ( 184, 106, 153, 149 );    //api.thingspeak.com
#define TWEET_PREFIX "Aurora Prediction: "        //The prefix to go in front of twitter messages
#define TWITTER_ALL_KP                            //If defined a twitter is sent every 15 mins, if not defined, it's only sent when the predicted kp index is >= TRIGGER_KP
////// END TWITTER SETTINGS


////// BEGIN TIMEZONE SETTINGS
#define DAYLIGHT_SAVINGS_ACTIVE   true           //Set to true if it's currently daylight savings, otherwise false
#define TIMEZONE_OFFSET_FROM_UTC  (-7 * 60)      //In minutes, e.g. -7 * 60 = 7 hours behind UTC
#define TIMEZONE                  "MDT"          //Set to your local timezone
#define DAYLIGHT_SAVINGS_OFFSET   60             //In minutes, 60 = 1 hour which is normal, set to 0 if you don't observe daylight savings
//// END TIMEZONE SETTINGS


////// BEGIN LOCAL AURORA SETTINGS
#define TRIGGER_KP  3.33 //The triggering KP index
////// END LOCAL AURORA SETTINGS

////// BEGIN WINGKP SETTINGS
IPAddress wingKpIP ( 140, 90, 33, 21 );    //www.swpc.noaa.gov
#define WINGKP_URL "http://www.swpc.noaa.gov/wingkp/wingkp_list.txt"
////// END WINGKP SETTINGS


////// BEGIN RGB LED SETTINGS
#define D3 3
#define D5 5
#define D6 6
#define LED_RED_PIN   D3
#define LED_GREEN_PIN D5
#define LED_BLUE_PIN  D6
#define LED_COMMON_ANODE      //If defined, the LED is Common Anode, if undefined, the LED is Common Cathode
#define BRIGHTNESS 1.0        //Values between 0.0 -> 1.0.  1.0 is full brightness
#define LED_TEST_AT_STARTUP   //If uncommented, the colors for the various LED statuses are cycled through at startup
////// END RGB LED SETTINGS


////// BEGIN DEBUGGING SETTINGS
#define BAUD_RATE 115200  //The baud rate for Arduino Serial output
#define DEBUG_TWEET_RESPONSE    //Comment out to stop the display of the results from the tweet http query
#define DEBUG_WINGKP_RESPONSE   //Comment out to stop the display of the results from the Wing Kp http query
//#define MEMORY_USAGE_REPORTING  //Comment out to stop memory usage reporting
///// END DEBUGGING SETTINGS



// Global stuff - Shouldn't need to change anything past this point
#ifdef MEMORY_USAGE_REPORTING
   #include <MemoryFree.h>
   int memoryLoopCounter = 0;
#endif

#define HTTP_PORT 80
#define NETWORK_RETRIES  5  //The number of times to retry tweets and wingkp index retreivals if the connection fails

EthernetClient wingkpClient;
#ifdef TWITTER
   EthernetClient twitterClient;
#endif

#define MAX_BUFFER 150        //Used for the http post in twitter and for storing WingKP lines
char buffer[MAX_BUFFER + 1];


#define WINGKP_SAMPLING_INITIAL       14  //Mins
#define WINGKP_SAMPLING_NORMAL        15  //Mins
#define WINGKP_SAMPLING_SWITCH_DELAY  2   //Mins
int wingKpSamplingInterval;               //Minutes.  At startup we get the WingKp index every 14mins until it hasn't changed since the last call, then we set it to 15 min


int lastPredTime = 193;  //Set to an arbitary value that won't be found in the data



//Structure to hold a line of wingkp data
struct WingKpData 
{
  int   predDay, predMonth, predYear, predTime;              //Date/Time the prediction was made (UTC)
  int   status;                                              //Status
  int   target1Day, target1Month, target1Year, target1Time;  //1 hour prediction
  float target1Index, target1LeadTimeMins;
  int   target4Day, target4Month, target4Year, target4Time;  //4 hour prediction
  float target4Index, target4LeadTimeMins;
  float actual;                                              //Actual earth read kp
};

struct WingKpData wdataLast;    //The last line we received


enum MonitorState
{
    MONITOR_STATE_STARTUP = 0,                //Initial state until the first wingkp is started
    MONITOR_STATE_STARTUP_FIRST_REQUEST,      //First request has been made, but no reply yet
    MONITOR_STATE_STARTUP_FIRST_DOWNLOADING,  //First request has been made and we're downloading data
    MONITOR_STATE_WINGKP_NO_COMMS,            //Unable to contact WingKp server
    MONITOR_STATE_WINGKP_DOWN,                //WingKp index is returning -1 indicating it's down
    MONITOR_STATE_WINGKP_BELOW_TRIGGER,       //WingKp index is below the TRIGGER_KP
    MONITOR_STATE_WINGKP_STORM_LOW,           //WingKp index is just above TRIGGER_KP, storm visible to North
    MONITOR_STATE_WINGKP_STORM_MODERATE,      //WingKp index is above TRIGGER_KP by 1KP, storm visible overhead
    MONITOR_STATE_WINGKP_STORM_HIGH,          //WingKp index is above the TRIGGER_KP by 2KP, storm visiable overhead
    MONITOR_STATE_WINGKP_STORM_EXTREME,       //WingLp index is above the TRIGGER_KP by 3KP, storm maybe visible and  intense or too far south
    
    MONITOR_STATE_LAST = MONITOR_STATE_WINGKP_STORM_EXTREME,  //Indicates last state
};

volatile enum MonitorState monitorState;

char *twitterStatus[] = {"", "", "", "", "Down", "Quiet", "Low Storm", "Moderate Storm", "Major Storm", "Extreme Storm"};

struct LedStatus
{
   unsigned char red, green, blue;  //Color 0 = off, 255 = on with a gradient with values in between
   unsigned int flashOn;    //in ms
   unsigned int flashOff;   //in ms
};


//Maps colors and flash patterns to all states

static const struct LedStatus PROGMEM ledStatuses[] =
{
   {255, 0,   255, 50,   3000},   //MONITOR_STATE_STARTUP,                   Magenta       0.25s On, 3s off
   {255, 0,   255, 50,   1000},   //MONITOR_STATE_STARTUP_FIRST_REQUEST,     Magenta       0.25s On, 1s Off
   {255, 0,   255, 50,   250},    //MONITOR_STATE_STARTUP_FIRST_DOWNLOADING, Magenta       0.25s On, 0.25s Off
   {255, 255, 0,   1000, 1000},   //MONITOR_STATE_WINGKP_NO_COMMS,           Yellow        1s On, 1s Off
   {120, 255, 60,  3000, 3000},   //MONITOR_STATE_WINGKP_DOWN,               White         3s On, 3s Off
   {0,   255, 11,  50,   10000},  //MONITOR_STATE_WINGKP_BELOW_TRIGGER,      Aurora Green  0.5s On, 3s Off
   {0,   255, 11,  5000, 50},     //MONITOR_STATE_WINGKP_STORM_LOW,          Aurora Green  3s On, 0.25s Off 
   {255, 150, 0,   5000, 50},     //MONITOR_STATE_WINGKP_STORM_MODERATE,     Aurora Orange 3s On, 0.25s Off
   {255, 0,   0,   5000, 50},     //MONITOR_STATE_WINGKP_STORM_HIGH,         Aurora Red    3s On, 0.25s Off
   {255, 70,  255, 5000, 50},     //MONITOR_STATE_WINGKP_STORM_EXTREME,      Aurora Violet 3s On, 0.25s Off
};

volatile bool ledOn = false;
volatile unsigned int flashTimerCounter;

#define FLASH_TIMER_1MSEC_RESET 130

char *bufferPtr;
#define MAX_FLOAT_SCAN_LEN 7



//Serial print routines to print from PGMSPACE

#define serialPgmPrint(x)   SerialPrint_P(PSTR(x))
#define serialPgmPrintln(x) SerialPrintln_P(PSTR(x))

void SerialPrint_P(PGM_P str)
{
  for (byte c; (c = pgm_read_byte(str)); str++) Serial.write(c);
}



void SerialPrintln_P(PGM_P str)
{
  SerialPrint_P(str);
  Serial.println();
}



#ifdef TWITTER

//Returns true if the tweet was successful, otherwuse false

bool tweet(char *msg)
{
  //If we have an empty message, we don't tweet, but we signal everything is ok
  if ( strlen(msg) == 0 )  return true;

  serialPgmPrintln("Connecting to ThingTweet...");

  //Connect to TweetSpeak
  if (twitterClient.connect( thingSpeakIP, HTTP_PORT ))
  { 
    serialPgmPrintln("Connected");

    // Create HTTP POST Data
    strcpy(buffer, "api_key=");
    strcat(buffer, THINGTWEET_API_KEY);
    strcat(buffer, "&status=");
    strcat(buffer, msg);

    twitterClient.print("POST /apps/thingtweet/1/statuses/update HTTP/1.1\n");
    twitterClient.print("Host: api.thingspeak.com\n");
    twitterClient.print("Connection: close\n");
    twitterClient.print("Content-Type: application/x-www-form-urlencoded\n");
    twitterClient.print("Content-Length: ");
    twitterClient.print(strlen(buffer));
    twitterClient.print("\n\n");
    twitterClient.print(buffer);

    serialPgmPrint("Posting: ");
    Serial.println(buffer);

    //Wait 1/2 second for transmission to happen
    Alarm.delay(500);

    //Read the response and display to Serial if we're connected
    while ( twitterClient.connected() )
    {
      if (twitterClient.available())
      {
        char c = twitterClient.read();

#ifdef DEBUG_TWEET_RESPONSE
        Serial.print(c);
#endif
      }
    }

    //Connection should have been terminated by web server, but just
    //in case it wasn't, we make sure so that it doesn't cause issues later
    Alarm.delay(1000);
    twitterClient.flush();
    twitterClient.stop();

    return true;
  }
  else
  {
    serialPgmPrintln("Error: Unable to connect");   
    return false;
  }
}



//Tweet retrying numTries in case of failure

bool tweetWithRetries(char *msg, byte numTries)
{
  for ( byte i = 0; i < numTries; i ++ )
  {
    if ( tweet(msg) )  return true;
    serialPgmPrint("Retry ");
    Serial.println(i + 1);
    Alarm.delay(1000);
  }   

  return false;
}

#endif



//Converts a time in UTC to local time

int convertFromUTCToLocalTime(int utcTime)
{
  //Convert utcTime into minutes since midnight
  int mins  = utcTime % 100;
  int hours = utcTime / 100;
  int localTime = hours * 60 + mins;

  //Correct for the timezone offset
  localTime += TIMEZONE_OFFSET_FROM_UTC;

  //Correct for daylight savings
  if ( DAYLIGHT_SAVINGS_ACTIVE )  localTime += DAYLIGHT_SAVINGS_OFFSET;

  //With the above adjustments, the time could be outside the 24 hour range,
  //we need to normalize it to fit
  localTime = localTime % (24 * 60);
  if ( localTime < 0 )  localTime += 24 * 60;

  //Now convert back into a string type time
  hours = localTime / 60;
  mins = localTime % 60;
  localTime = hours * 100 + mins;

  return localTime;
}



//Send a GET request to Noaa to obtain the WingKp index list
//and process the list, storing the last line in the returned data

bool wingkp(void)
{
  serialPgmPrintln("Connecting to Noaa...");

  if ( monitorState == MONITOR_STATE_STARTUP )
      setState(MONITOR_STATE_STARTUP_FIRST_REQUEST);

  if ( wingkpClient.connect(wingKpIP, HTTP_PORT) )
  {
    serialPgmPrintln("Connected");

    if ( monitorState == MONITOR_STATE_STARTUP_FIRST_REQUEST )
        setState(MONITOR_STATE_STARTUP_FIRST_DOWNLOADING);

    //Send the http query
    wingkpClient.print("GET ");
    wingkpClient.println(WINGKP_URL);
    wingkpClient.println();

    //Wait 1/2 second for transmission to happen
    Alarm.delay(500);

    //Read the response
    int bufPos = 0;
    buffer[MAX_BUFFER] = NULL;  //Terminate at the end of the buffer just in case
    while( wingkpClient.connected() )
    {
      if ( wingkpClient.available() )
      {
        char c = wingkpClient.read();

        //We accumulate each line in the wingkpLineBuffer
        if ( c == '\n' )
        {
          buffer[bufPos] = NULL;
          bufPos = 0;
        }
        else  buffer[bufPos++] = c;

        //Shouldn't happen, but we might overflow, wrap around, it'll corrupt the line
        //but let's keep it simple
        if ( bufPos >= MAX_BUFFER )  bufPos = 0;

#ifdef DEBUG_WINGKP_RESPONSE
        Serial.print(c);
#endif
      }
    }
    if ( bufPos != 0 )  buffer[bufPos] = NULL;

    //Connection should have been terminated by web server, but just
    //in case it wasn't, we make sure so that it doesn't cause issues later
    Alarm.delay(1000);
    wingkpClient.flush();
    wingkpClient.stop();

    serialPgmPrint("Last Line Read: <");
    Serial.print(buffer);
    serialPgmPrintln(">");

    return true;
  }
  else
  {
    serialPgmPrintln("Error: Unable to connect");   
    return false;
  }
}



//Request the WingKp list from Noaa retrying numTries in case of failure

bool wingkpWithRetries(byte numTries)
{
  for ( byte i = 0; i < numTries; i ++ )
  {
    if ( wingkp() )  return true;
    serialPgmPrint("Retry ");
    Serial.println(i + 1);
    Alarm.delay(1000);
  }   

  return false;
}



//Scans a float from buffer
//Returns -10.0 if a float can't be scanned
//We can't use sscanf because it doesn't handle floats

float scanFloat(void)
{
   byte p = 0;
   char buf[MAX_FLOAT_SCAN_LEN + 1];
   
   //Ignore any leading white space
   while (( *bufferPtr ) && ( *bufferPtr == ' ' ) || ( *bufferPtr == '\t' ))
   {
      bufferPtr ++;
   }
   
   //Read the number into the buffer
   while (( *bufferPtr ) && (( *bufferPtr >= '0') && ( *bufferPtr <= '9')) || ( *bufferPtr == '.' ) || ( *bufferPtr == '-' ))
   {
      buf[p++] = *bufferPtr;
      bufferPtr ++;
      if ( p == MAX_FLOAT_SCAN_LEN ) break;
   }
   
   buf[p] = NULL;
   
   if (( p == 0 ) || ( p == MAX_FLOAT_SCAN_LEN ))  return -10.0;
   
   return atof(buf);
}



//Scan the last line of the wingkp page

bool scanLastLineWingKp(void)
{
  bufferPtr = buffer;
  
  //Scan in the data
  wdataLast.predYear               = scanFloat();
  wdataLast.predMonth              = scanFloat();
  wdataLast.predDay                = scanFloat();
  wdataLast.predTime               = scanFloat();
  wdataLast.status                 = scanFloat();
  wdataLast.target1Year            = scanFloat();
  wdataLast.target1Month           = scanFloat();
  wdataLast.target1Day             = scanFloat();
  wdataLast.target1Time            = scanFloat();
  wdataLast.target1Index           = scanFloat();
  wdataLast.target1LeadTimeMins    = scanFloat();
  wdataLast.target4Year            = scanFloat();
  wdataLast.target4Month           = scanFloat();
  wdataLast.target4Day             = scanFloat();
  wdataLast.target4Time            = scanFloat();
  wdataLast.target4Index           = scanFloat();
  wdataLast.target4LeadTimeMins    = scanFloat();
  wdataLast.actual                 = scanFloat();
  
  return true;
}



//Updates the LED for the current kp index

void updateLEDForKp(float kpIndex)
{
   if      ( kpIndex < 0 )                    setState(MONITOR_STATE_WINGKP_DOWN);
   else if ( kpIndex < TRIGGER_KP )           setState(MONITOR_STATE_WINGKP_BELOW_TRIGGER);
   else if ( kpIndex < (TRIGGER_KP + 1.0) )   setState(MONITOR_STATE_WINGKP_STORM_LOW);
   else if ( kpIndex < (TRIGGER_KP + 2.0) )   setState(MONITOR_STATE_WINGKP_STORM_MODERATE);
   else if ( kpIndex < (TRIGGER_KP + 3.0) )   setState(MONITOR_STATE_WINGKP_STORM_HIGH);
   else                                       setState(MONITOR_STATE_WINGKP_STORM_EXTREME);
}



//Converts a kp index into a string
//We do this because sprintf can't handle floats

char *kpToStr(float kp)
{
   static char buf[5+1];
   dtostrf((double)kp, 2, 2, buf);
   return buf;
}



//Requests and process data from Noaa for the WingKp index
//Additional, it updates the LED for the current kp index.
//It tweeting is enabled, it will also tweet the status

void processWingKp(void)
{
   if ( wingKpSamplingInterval == WINGKP_SAMPLING_INITIAL )
      serialPgmPrintln("WingKp Index Interval: Initial");

   //Set an alarm for the next time we call processWingKp
   AlarmID_t alarm = Alarm.timerOnce(wingKpSamplingInterval * 60, processWingKp);

   //Get the wing kp index
   if ( wingkpWithRetries(NETWORK_RETRIES) )
   {
      //Process the last line of the txt containing the wingkp index
      if ( ! scanLastLineWingKp() )
      {
          setState(MONITOR_STATE_WINGKP_DOWN);
          return;
      }
      
      updateLEDForKp(wdataLast.target1Index);
      
      //Tweet the wingkp index, we do it this weird way to save memory
      char tweetMsg[100];
      char *tweetMsgPtr = tweetMsg;
      int local1Time = convertFromUTCToLocalTime(wdataLast.target1Time);
      int local4Time = convertFromUTCToLocalTime(wdataLast.target4Time);
      sprintf(tweetMsg, "%sKp: %s(%02d:%02d) ", TWEET_PREFIX, kpToStr(wdataLast.target1Index), local1Time / 100, local1Time % 100);
      tweetMsgPtr = tweetMsg + strlen(tweetMsg);
      sprintf(tweetMsgPtr, "%s(%02d:%02d) %s (earth ", kpToStr(wdataLast.target4Index), local4Time / 100, local4Time % 100, TIMEZONE); 
      tweetMsgPtr += strlen(tweetMsgPtr);
      sprintf(tweetMsgPtr, "%s) %d%%25 Status: %s", kpToStr(wdataLast.actual), 100 - wdataLast.status * 25, twitterStatus[monitorState] );
              
#ifdef TWITTER

      //Only tweet if TWITTER_ALL_KP is set, or kp index >= TRIGGER_KP
#ifndef TWITTER_ALL_KP
      if ( wdataLast.target1Index >= TRIGGER_KP )
      {
#endif
         Serial.println(tweetMsg);
         tweetWithRetries(tweetMsg, NETWORK_RETRIES);

#ifndef TWITTER_ALL_KP
      }
#endif

#endif


      //We get the wingkp index every 14 mins, until we get the same
      //result as the last call.  When this happens, we've synced to
      //the 15 min interval, so we cancel the alarm, add on 2 mins and 
      //call every 15 mins from then on.  If we lose sync again, this will
      //automatically resync over time.
      //Then we avoid having to get the external time.
      //Should sync over about 3 hours.
      if ( wdataLast.predTime == lastPredTime )
      {
         wingKpSamplingInterval = WINGKP_SAMPLING_NORMAL;

         //Cancel the existing alarm, and create one for 2 mins into the future instead
         Alarm.disable(alarm);
         Alarm.timerOnce( WINGKP_SAMPLING_SWITCH_DELAY * 60, processWingKp);

         serialPgmPrintln("WingKp Index Interval: Synced to 15min boundary");
      }

      lastPredTime = wdataLast.predTime;
   }
   else setState(MONITOR_STATE_WINGKP_NO_COMMS);
}



//If on is true, then the color is switched on, otherwise it's switched off

void setColorForState(bool on)
{
   //Don't write any serial statements here, as it's called from the interrupt
   //These Serial lines should only be used temporarily to check the interrupt
   //is working, you should except the crashes if uncommented
   //serialPgmPrint("Flash: ");
   //Serial.println(on);
   
   byte r = pgm_read_byte(&ledStatuses[monitorState].red);
   byte g = pgm_read_byte(&ledStatuses[monitorState].green);
   byte b = pgm_read_byte(&ledStatuses[monitorState].blue);
   
#ifdef LED_COMMON_ANODE
   analogWrite(LED_RED_PIN,   (on) ? (255 - ( r * BRIGHTNESS)) : 255); 
   analogWrite(LED_GREEN_PIN, (on) ? (255 - ( g * BRIGHTNESS)) : 255); 
   analogWrite(LED_BLUE_PIN,  (on) ? (255 - ( b * BRIGHTNESS)) : 255);
#else  
   analogWrite(LED_RED_PIN,   (on) ? (r * BRIGHTNESS)   : 0); 
   analogWrite(LED_GREEN_PIN, (on) ? (g * BRIGHTNESS) : 0); 
   analogWrite(LED_BLUE_PIN,  (on) ? (b * BRIGHTNESS)  : 0);
#endif
}



#ifdef DHCP

//Note that if the lease doesn't renew, it keeps using the last ip address used

void renewDhcpLease(void)
{
  serialPgmPrintln("Renewing Dhcp lease");

  int status = Ethernet.maintain();
  if ( (status == 2) || (status == 4 ))
  {
    //We possibly changed the ip address, display it here
    serialPgmPrint("Local IP: ");
    Serial.println(Ethernet.localIP());
  }

  //Setup the next dhcp lease renewal
  Alarm.timerOnce(DHCP_LEASE_RENEW_INTERVAL, renewDhcpLease);
}

#endif



// Timer2 led flasher, called every 1ms

ISR(TIMER2_OVF_vect)
{
   flashTimerCounter ++;
  
   //Reset for the next 1 msec interrupt
   TCNT2 = FLASH_TIMER_1MSEC_RESET;
   TIFR2 = 0x00;
   
   //Handle flashing the led according to the flash pattern specified

   if      (( ledOn ) && ( flashTimerCounter >= pgm_read_word(&ledStatuses[monitorState].flashOn) ))
   {
      ledOn = false;
      flashTimerCounter = 0;
      setColorForState(ledOn);
   }
   else if (( ! ledOn ) && ( flashTimerCounter >= pgm_read_word(&ledStatuses[monitorState].flashOff) ))
   {
      ledOn = true;
      flashTimerCounter = 0;
      setColorForState(ledOn);
   }   
}



//Sets the state and resets the led flasher at the same time

void setState(int state)
{
   serialPgmPrint("State: ");
   Serial.println(state);
   
   flashTimerCounter = 0;
   monitorState = (enum MonitorState)state;
   ledOn = true;
   setColorForState(ledOn);
}



//Sets up the LED flasher interrupt on timer 2

void setupLedFlasherInterrupt(void)
{
   ledOn = true;
   setColorForState(ledOn);
   flashTimerCounter = 0;
   
   TCCR2B = 0x00;                       //Disable Timer2
   TCNT2  = FLASH_TIMER_1MSEC_RESET;    //Reset timer for 1ms count
   TIFR2  = 0x00;                       //Clear timer overflow flag
   TIMSK2 = 0x01;                       //Timer overflow interrupt enable
   TCCR2A = 0x00;   
   TCCR2B = 0x05;                       //Prescaler 128   
}



void setup()
{
  //Time for hardware to startup
  delay(1000);

  Serial.begin(BAUD_RATE);
  
#ifdef MEMORY_USAGE_REPORTING
   memoryReport();
#endif
  
#ifdef LED_TEST_AT_STARTUP
  //Cycle through all the colors/states at start up
  serialPgmPrintln("LED Test Sequence");  
  for ( monitorState = MONITOR_STATE_STARTUP; monitorState <= MONITOR_STATE_LAST; monitorState = (enum MonitorState)(monitorState + 1) )
  {
      setColorForState(true);
      delay(2000);
      setColorForState(false);
      delay(1000);
  }
#endif
  
  monitorState = MONITOR_STATE_STARTUP;
  
  serialPgmPrintln("Setting up LED interrupt");
  
  //Setup the RGB led pins as outputs and start it flashing
  pinMode( LED_RED_PIN,   OUTPUT);
  pinMode( LED_GREEN_PIN, OUTPUT);
  pinMode( LED_BLUE_PIN,  OUTPUT);
  setupLedFlasherInterrupt();

  //Start ethernet
  serialPgmPrintln("Starting Network...");
#ifdef DHCP
  Ethernet.begin(mac);
  Alarm.timerOnce(DHCP_LEASE_RENEW_INTERVAL, renewDhcpLease);  //Setup dhcp lease renewal
  serialPgmPrintln("Successfully negotiated dhcp");
#else
  Ethernet.begin(mac, localIP, localDns, localGateway, localSubnet);
#endif
  serialPgmPrint("Local IP: ");
  Serial.println(Ethernet.localIP());

  //To allow the network to fire up
  Alarm.delay(1000);

  //Setup the initial interval time
  wingKpSamplingInterval = WINGKP_SAMPLING_INITIAL;
  
  //Setoff an alarm to get the wingkp data the first time (2 seconds)
  Alarm.timerOnce(2, processWingKp);

#ifdef MEMORY_USAGE_REPORTING
   memoryReport();
#endif
}



#ifdef MEMORY_USAGE_REPORTING

//Used for debugging purposes

void memoryReport(void)
{
   serialPgmPrint("Memory Usage: Stack(");
   Serial.print(StackCount());
   serialPgmPrint(") Free(");
   Serial.print(freeMemory());
   serialPgmPrintln(")");
}

#endif



void loop()
{ 
   //Need to always use "Alarm.delay" instead of "delay" if we're using alarms    
   Alarm.delay(1000);  
   
#ifdef MEMORY_USAGE_REPORTING
   memoryLoopCounter ++;
   if ( memoryLoopCounter >= 20 )  //20 seconds
   {
      memoryReport();
      memoryLoopCounter = 0;
   }
#endif
}


