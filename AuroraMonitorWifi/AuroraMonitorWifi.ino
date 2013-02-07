//
//  Aurora Monitor - Wifi Version
//  Arduino Ethernet / Wifi Aurora Monitor and twitter feed (http://www.thingiverse.com/thing:25528)
//
//  Copyright: Jetty, June 2012
//  License: GNU General Public License v3: http://www.gnu.org/licenses/gpl-3.0.txt
//
//  Requires Arduino 1.0.1 or later IDE
//  Requires Arduino with Wifi Board or built in Wifi that is Asynclab BlackWidow WiShield 1.0 compatible
//  Tested on: LinkSprite DiamondBack Arduino: http://www.robotshop.com/productinfo.aspx?pc=RB-Lin-40&lang=en-US
//
//
//  Requirements:
//
//     Libraries:
//        Requires Time / TimeAlarms / MemoryFree / WiShield, which can be obtained here:
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
#include <WiServer.h>
#include <dns.h>
#include <Time.h>
#include <TimeAlarms.h>



////// BEGIN NETWORK SETTINGS
// Wireless configuration parameters ----------------------------------------
unsigned char local_ip[] = {192,168,1,190};   // IP address of WiShield
unsigned char gateway_ip[] = {192,168,1,1};   // router or gateway IP address
unsigned char subnet_mask[] = {255,255,255,0};   // subnet mask for the local network
char ssid[] = {"mynetwork"};      // max 32 bytes

unsigned char security_type = 5;	// 0 - open; 1 - WEP; 2 - WPA; 3 - WPA2; 4 - WPA Precalc; 5 - WPA2 Precalc


// Depending on your security_type, uncomment the appropriate type of security_data
// 0 - None (open)
//const prog_char security_data[] PROGMEM = {};

// 1 - WEP 
// UIP_WEP_KEY_LEN. 5 bytes for 64-bit key, 13 bytes for 128-bit key
// Only supply the appropriate key, do not specify 4 keys and then try to specify which to use
//const prog_char security_data[] PROGMEM = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, };

// 2, 3 - WPA/WPA2 Passphrase
// 8 to 63 characters which will be used to generate the 32 byte calculated key
// Expect the g2100 to take 30 seconds to calculate the key from a passphrase
//const prog_char security_data[] PROGMEM = {"12345678"};

// 4, 5 - WPA/WPA2 Precalc
// The 32 byte precalculate WPA/WPA2 key. This can be calculated in advance to save boot time
// http://jorisvr.nl/wpapsk.html
const prog_char security_data[] PROGMEM = {
  0x12, 0x13, 0x14, 0x15, 0x16, 0x8f, 0xca, 0xf7, 0x6e, 0x17, 0x18, 0x19, 0x8f, 0xa6, 0x20, 0x3c,
  0x21, 0xde, 0x9a, 0x23, 0xa9, 0x24, 0xcc, 0x26, 0x30, 0x29, 0x9e, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
};


// setup the wireless mode
// infrastructure - connect to AP
// adhoc - connect to another WiFi device
//Wireless configuration defines ----------------------------------------
#define WIRELESS_MODE_INFRA   1
#define WIRELESS_MODE_ADHOC   2
unsigned char wireless_mode = WIRELESS_MODE_INFRA;
////// END NETWORK SETTINGS


////// BEGIN TWITTER SETTINGS
#define TWITTER                                   //Comment out if you don't want to update to twitter                      
#define THINGTWEET_API_KEY "29836AGI9020293Q"     //Obtained by following the instructions at the top of this file
byte thingSpeakIP[] = { 184, 106, 153, 149 };    //api.thingspeak.com
#define TWEET_PREFIX "Aurora Prediction: "        //The prefix to go in front of twitter messages
#define THINGTWEET_HOST "api.thingspeak.com"
#define THINGTWEET_URL "/apps/thingtweet/1/statuses/update"
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
byte wingKpIP[] = { 140, 90, 33, 21 };    //www.swpc.noaa.gov
#define WINGKP_HOST "www.swpc.noaa.gov"
#define WINGKP_URL "/wingkp/wingkp_list.txt"
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

int wingkpBytesReceived, wingkpRetryCount;
GETrequest getWingKp(wingKpIP, HTTP_PORT, WINGKP_HOST, WINGKP_URL);
#ifdef TWITTER
    int twitterBytesReceived, twitterRetryCount;
    POSTrequest postTwitter(thingSpeakIP, HTTP_PORT, THINGTWEET_HOST, THINGTWEET_URL, twitterPostData);
#endif

#define MAX_BUFFER 150        //Used for the http post in twitter and for storing WingKP lines
int bufPos = 0;
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
   {255, 150, 0,   5000, 50},     //MONITOR_STATE_WINGKP_STORM_HIGH,         Aurora Red    3s On, 0.25s Off
   {255, 0,   0,   5000, 50},     //MONITOR_STATE_WINGKP_STORM_HIGH,         Aurora Red    3s On, 0.25s Off
   {255, 70,  255, 5000, 50},     //MONITOR_STATE_WINGKP_STORM_EXTREME,      Aurora Violet 3s On, 0.25s Off
};

volatile bool ledOn = false;
volatile unsigned int flashTimerCounter;

#define FLASH_TIMER_1MSEC_RESET 130

char *bufferPtr;
#define MAX_FLOAT_SCAN_LEN 7

AlarmID_t wingKpAlarm;



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

//If we failed to twitter, we try again, once the connection is
//no longer active

void twitterRetrySubmit(void)
{
   if ( postTwitter.isActive() ) Alarm.timerOnce(1, twitterRetrySubmit);
   else                          postTwitter.submit();
}



//Data received back from the POST http request is processed here

void twitterReplyData(char *data, int len)
{
    //len = 0 indicates end of transmission
   if ( len == 0 )
   {
      serialPgmPrintln("End of transmission");

      //Safety check as occasionally the connection is closed with no data
      if ( twitterBytesReceived < 10 )
      {
         if ( twitterRetryCount < NETWORK_RETRIES )
         {
            twitterRetryCount ++;
            serialPgmPrint("Zero Bytes Received, Retry: ");
            Serial.println(twitterRetryCount);
         
            twitterBytesReceived = 0;
            
            Alarm.timerOnce(1, twitterRetrySubmit);
         }
      }

      return;
   }
  
   twitterBytesReceived += len;
   
#ifdef DEBUG_TWEET_RESPONSE
   for ( int i = 0; i < len; i ++ )  Serial.print(data[i]);
#endif
}



//The data we send for the http POST request

void twitterPostData(void)
{
      // Create HTTP POST Data
    WiServer.print("api_key=");
    WiServer.print(THINGTWEET_API_KEY);
    WiServer.print("&status=");
    WiServer.print(buffer);
}



//Tweets the message in buffer

void tweet(void)
{
  serialPgmPrintln("Connecting to ThingTweet...");

  twitterBytesReceived = 0;
  twitterRetryCount = 0;

  //Connect to TweetSpeak
  postTwitter.submit();
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



//Converts a kp index into a string
//We do this because sprintf can't handle floats

char *kpToStr(float kp)
{
   static char buf[5+1];
   dtostrf((double)kp, 2, 2, buf);
   return buf;
}



//This is called when all the data from Noaa is received, and it
//processes the last line of data, updates the LED for
//the current status and twitters the status if necessary

void processWingKpResponse(void)
{
   serialPgmPrint("Last Line Read: <");
   Serial.print(buffer);
   serialPgmPrintln(">");

   //Process the last line of the txt containing the wingkp index
   if ( ! scanLastLineWingKp() )
   {
       setState(MONITOR_STATE_WINGKP_DOWN);
       return;
   }
      
   updateLEDForKp(wdataLast.target1Index);
      
   //Tweet the wingkp index, we do it this weird way to save memory
   int local1Time = convertFromUTCToLocalTime(wdataLast.target1Time);
   int local4Time = convertFromUTCToLocalTime(wdataLast.target4Time);
   sprintf(buffer, "%sKp: %s(%02d:%02d) ", TWEET_PREFIX, kpToStr(wdataLast.target1Index), local1Time / 100, local1Time % 100);
   bufferPtr = buffer + strlen(buffer);
   sprintf(bufferPtr, "%s(%02d:%02d) %s (earth ", kpToStr(wdataLast.target4Index), local4Time / 100, local4Time % 100, TIMEZONE); 
   bufferPtr += strlen(bufferPtr);
   sprintf(bufferPtr, "%s) %d%%25 Status: %s", kpToStr(wdataLast.actual), 100 - wdataLast.status * 25, twitterStatus[monitorState] );
          
#ifdef TWITTER
   //Only tweet if TWITTER_ALL_KP is set, or kp index >= TRIGGER_KP
#ifndef TWITTER_ALL_KP
   if ( wdataLast.target1Index >= TRIGGER_KP )
   {
#endif
      Serial.println(buffer);
      Alarm.timerOnce(5, tweet);

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
      Alarm.disable(wingKpAlarm);
      Alarm.timerOnce( WINGKP_SAMPLING_SWITCH_DELAY * 60, processWingKp);

      serialPgmPrintln("WingKp Index Interval: Synced to 15min boundary");
   }

   lastPredTime = wdataLast.predTime;
}



//If we failed to get the WingKp data, we try again, once the connection is
//no longer active

void wingkpRetrySubmit(void)
{
   if ( getWingKp.isActive() ) Alarm.timerOnce(1, wingkpRetrySubmit);
   else                        getWingKp.submit();
}



//Data received back from the GET http request is processed here

void wingkpReplyData(char *data, int len)
{
   //len = 0 indicates end of transmission
   if ( len == 0 )
   {
      bufPos = 0;
      serialPgmPrintln("End of transmission");

      //Safety check as occasionally the connection is closed with no data
      if ( wingkpBytesReceived < 10 )
      {
         if ( wingkpRetryCount < NETWORK_RETRIES )
         {
            wingkpRetryCount ++;
            serialPgmPrint("Zero Bytes Received, Retry: ");
            Serial.println(wingkpRetryCount);
         
            wingkpBytesReceived = 0;
            
            Alarm.timerOnce(1, wingkpRetrySubmit);
         }
         else setState(MONITOR_STATE_WINGKP_NO_COMMS);
      }
      else
      {
         //We have data process it
         processWingKpResponse();
      }

      return;
   }
  
   static int firstCalled = true;
   
   if (( firstCalled ) && ( monitorState == MONITOR_STATE_STARTUP_FIRST_REQUEST ))
   {
      serialPgmPrintln("Connected");
      setState(MONITOR_STATE_STARTUP_FIRST_DOWNLOADING);
      firstCalled = false;
   }
   
   for (int i = 0; i < len; i ++ )
   {
        //We accumulate each line in the wingkpLineBuffer
        if (( data[i] == '\n' ) || ( data[i] == NULL ))
        {
          buffer[bufPos] = NULL;
          bufPos = 0;
        }
        else  buffer[bufPos++] = data[i];

        //Shouldn't happen, but we might overflow, wrap around, it'll corrupt the line
        //but let's keep it simple
        if ( bufPos >= MAX_BUFFER )
        {
           serialPgmPrintln("Buffer overflow");
           bufPos = 0;
        }
   }

   wingkpBytesReceived += len;
   
#ifdef DEBUG_WINGKP_RESPONSE
   for ( int i = 0; i < len; i ++ )  Serial.print(data[i]);
#endif
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



//Starts a request for the WingKp index from Noaa

void processWingKp(void)
{
   if ( wingKpSamplingInterval == WINGKP_SAMPLING_INITIAL )
   {
      serialPgmPrintln("WingKp Index Interval: Initial");
   }

   //Set an alarm for the next time we call processWingKp
   wingKpAlarm = Alarm.timerOnce(wingKpSamplingInterval * 60, processWingKp);

   serialPgmPrintln("Connecting to Noaa...");

   if ( monitorState == MONITOR_STATE_STARTUP )
       setState(MONITOR_STATE_STARTUP_FIRST_REQUEST);

   wingkpBytesReceived = 0;
   wingkpRetryCount = 0;
   bufPos = 0;
   buffer[MAX_BUFFER] = NULL;  //Terminate at the end of the buffer just in case

   //Get the wing kp index
   getWingKp.submit();
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

  //Start wifi
  serialPgmPrintln("Starting Wifi Network...");
  WiServer.init(NULL);
  
  //Print out the local ip address
  serialPgmPrint("Local IP: ");
  Serial.print(local_ip[0]);serialPgmPrint(".");
  Serial.print(local_ip[1]);serialPgmPrint(".");
  Serial.print(local_ip[2]);serialPgmPrint(".");
  Serial.println(local_ip[3]);

  //To allow the network to fire up
  delay(1000);
  
  //Setup the reply data return functions for the http get / post
  getWingKp.setReturnFunc(wingkpReplyData);
#ifdef TWITTER
  postTwitter.setReturnFunc(twitterReplyData);
#endif

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
  WiServer.server_task(); 
 
   //Need to always use "Alarm.delay" instead of "delay" if we're using alarms  
   Alarm.delay(10);
     
#ifdef MEMORY_USAGE_REPORTING
   memoryLoopCounter ++;
   if ( memoryLoopCounter >= 2000 )  //20 seconds
   {
      memoryReport();
      memoryLoopCounter = 0;
   }
#endif
}


