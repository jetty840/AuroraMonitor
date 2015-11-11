//
//  Aurora Monitor V2 - Wifi Version
//  Arduino Ethernet / Wifi Aurora Monitor and twitter feed (http://www.thingiverse.com/thing:47973)
//
//  Copyright: Jetty, Februrary 2013
//  License: GNU General Public License v3: http://www.gnu.org/licenses/gpl-3.0.txt
//
//  Requires Arduino 1.0.1 or later IDE
//  Requires Arduino with Wifi Board or built in Wifi that is Asynclab BlackWidow WiShield 1.0 compatible
//  Tested on: LinkSprite DiamondBack Arduino: http://www.robotshop.com/productinfo.aspx?pc=RB-Lin-40&lang=en-US
//  Tested on: Arduino 1.0.1
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
//     The calculation for this resistor (and you'll need 1 per color) is:
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
//  Push Button Operation:
//     A momentary push toggles the backlight for the LCD.  Note that during a storm the LCD backlight
//     will switch on.  
//
//     To configure the Daylight Savings setting, hold the push button for 7 seconds
//     and release, this will enter DST mode.  Now press the push button momentarily to switch between
//     DST ON and DST OFF.  To exit this mode, hold the push button for 7 seconds and release.
//
//     The DST setting is saved in EEPROM and is maintained when power is off.
//



#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <WiServer.h>
#include <dns.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <avr/wdt.h>



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
#define WIFI_STARTUP_TIMEOUT_SECS  60  //If the wifi network hasn't started in this time, the watchdog timer resets the arduino
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
                                                 //NOTE: THIS IS ONLY USED IF LCD_ENABLED IS NOT DEFINED
                                                 //      IF LCD_ENABLED IS DEFINED, DST IS SET VIA THE PUSHBUTTON SWITCH AND
                                                 //      LCD INTERFACE (HOLD BUTTON FOR 7 SECONDS AND RELEASE TO CONFIGURE)                                                 
#define DAYLIGHT_SAVINGS_EEPROM_ADDR  1          //Don't use location 0, as it will often be corrupted on brownouts
#define TIMEZONE_OFFSET_FROM_UTC  (-7 * 60)      //In minutes, e.g. -7 * 60 = 7 hours behind UTC
#define TIMEZONE                  "MDT"          //Set to your local timezone
#define DAYLIGHT_SAVINGS_OFFSET   60             //In minutes, 60 = 1 hour which is normal, set to 0 if you don't observe daylight savings
//// END TIMEZONE SETTINGS


////// BEGIN LOCAL AURORA SETTINGS
#define TRIGGER_KP  3.33 //The triggering KP index
////// END LOCAL AURORA SETTINGS

////// BEGIN WINGKP SETTINGS
byte wingKpIP[] = { 140, 90, 33, 21 };    //services.swpc.noaa.gov
#define WINGKP_HOST "services.swpc.noaa.gov"
#define WINGKP_URL "/text/wing-kp.txt"
////// END WINGKP SETTINGS


////// BEGIN WATCHDOG SETTINGS
//     Enabling the watchdog provides some stability, i.e. if the comms hang, wifi doesn't start or the Arduino crashes,
//     the Arduino will restart automatically.  However by default, most Arduino bootloaders have a bug in that
//     will prevent the bootloader running the software after a watchdog reset.  Therefore, you will likely
//     need to install a watchdog friendly bootloader before enabling this setting.
//
//     If your Arduino does not contain the fixed bootloader, after a watchdog reset, the Arduino will just hang and
//     to bring it back again, you'll need to power cycle it and reload the software with WATCHDOG_TIMER_ENABLED disabled, before
//     the watchdog kicks in and the reset happens.
//
//     To burn a bootloader, you'll need an ISP (In System Programmer), or you can build one with another Arduino:
//        http://pdp11.byethost12.com/AVR/ArduinoAsProgrammer.htm
//        http://electronics.stackexchange.com/questions/41700/using-an-arduino-as-an-avr-isp-in-system-programmer
//    
//     To fix and burn a new bootloader on the DiamondBack on Arduino IDE 1.0.1 with watchdog support, here's what I did on a Mac:
//
//     1. cd /Applications/Arduino.app/Contents/Resources/Java/hardware/arduino
//
//     2. Backup boards.txt, and duplicate the Arduino Duemilanove w/ ATmega328 entry, changing these 2 lines to this:
//        atmega328.name=Arduino Duemilanove w/ ATmega328 (watchdog oscillator)
//        atmega328.bootloader.file=ATmegaBOOT_168_atmega328_wdt.hex
//
//     3. cd /Applications/Arduino.app/Contents/Resources/Java/hardware/arduino/bootloaders/atmega
// 
//     4. Backup ATmegaBOOT_168.c and Makefile
//
//     5. Edit Makefile, duplicate atmega328 to the following (note -DWATCHDOG_MODS)
//        atmega328_wdt: TARGET = atmega328
//        atmega328_wdt: MCU_TARGET = atmega328p
//        atmega328_wdt: CFLAGS += '-DMAX_TIME_COUNT=F_CPU>>4' '-DNUM_LED_FLASHES=1' -DBAUD_RATE=57600 -DWATCHDOG_MODS
//        atmega328_wdt: AVR_FREQ = 16000000L
//        atmega328_wdt: LDSECTION  = --section-start=.text=0x7800
//        atmega328_wdt: $(PROGRAM)_atmega328_wdt.hex
//
//     6. From here:  http://n0m1.com/2012/04/01/how-to-compiling-the-arduino-bootloader/ follow Step 2 and Step 3
//
//     7. Edit ATmegaBOOT_168.c, and add:  GPIOR0 = ch;    after:
//        #ifdef WATCHDOG_MODS
//            ch = MCUSR;
//        (this enables the reset reason to be stored and available in the main program in the GPIOR0 register)
//
//     8. Type:  make atmega328_wdt
//
//     9. Restart the Arduino IDE and Connect an ISP to the ICSP header DiamondBack
//
//     10. Select the new board type:  Arduino Deumalinove w/ ATmega328 (watchdog oscillator)
// 
//     11. Select the correct programmer for your ISP
//
//     12. Select Burn Bootloader
//

//#define WATCHDOG_TIMER_ENABLED

#ifdef WATCHDOG_TIMER_ENABLED
  uint8_t reset_reason = 0;
  #define NO_PREDICTION_UPDATE_TIMEOUT_MS  (unsigned long)(34UL * 60UL * 1000UL)  //34 mins (or just over 2 updates)
  volatile unsigned long lastSuccessfullUpdate = 0;
#endif
////// END WATCHDOG SETTNGS


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



// LCD configuration, Hitachi HD44780 driver compatible + Pushbutton switch for control
#define LCD_ENABLED

#ifdef LCD_ENABLED
  #include <LiquidCrystal.h>
  #include <EEPROM.h>

  #define D7            7

  #define LCD_RS        D7
  #define LCD_ENABLE    A4
  #define LCD_BIT4      A3
  #define LCD_BIT5      A2
  #define LCD_BIT6      A1
  #define LCD_BIT7      A0
  #define LCD_BACKLIGHT A5

  LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_BIT4, LCD_BIT5, LCD_BIT6, LCD_BIT7);
  #define LCD_COLUMNS 16
  #define LCD_ROWS    2

  //Interface Switch
  #define D4                4
  #define PUSHBUTTON_SWITCH D4

  enum debounceState 
  {
    DEBOUNCE_STATE_OFF = 0,
    DEBOUNCE_STATE_TRANSITION_TO_ON,
    DEBOUNCE_STATE_ON,
    DEBOUNCE_STATE_TRANSITION_TO_OFF,
  };

  enum debounceState debounceState = DEBOUNCE_STATE_OFF;

  long debounceTimer;

  #define DEBOUNCE_TIMER_MS 50

  long buttonPressedAt;

  #define LONG_BUTTON_PRESS_MS 5000

  enum buttonPushState
  {
    BUTTON_PUSH_STATE_NORMAL = 0,  //Normal operating mode
    BUTTON_PUSH_STATE_DST_MODE,
  };

  enum buttonPushState buttonPushState = BUTTON_PUSH_STATE_NORMAL;
#endif



// Global stuff - Shouldn't need to change anything past this point
#ifdef MEMORY_USAGE_REPORTING
   #include <MemoryFree.h>
   int memoryLoopCounter = 0;
#endif

#define HTTP_PORT 80
#define NETWORK_RETRIES  5  //The number of times to retry tweets and wingkp index retreivals if the connection fails

long wingkpBytesReceived;
int wingkpRetryCount;
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
  int   target1Day, target1Month, target1Year, target1Time;  //1 hour prediction
  float target1Index;
  int   target4Day, target4Month, target4Year, target4Time;  //4 hour prediction
  float target4Index;
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
#ifdef LCD_ENABLED
  enum MonitorState          lastMonitorState;

  prog_char monitorStateString_0[] PROGMEM = "Trying Comms...";
  prog_char monitorStateString_1[] PROGMEM = "WingKp: Request";
  prog_char monitorStateString_2[] PROGMEM = "WingKp: Download";
  prog_char monitorStateString_3[] PROGMEM = "WingKp: NO COMMS";
  prog_char monitorStateString_4[] PROGMEM = "WingKp: DOWN/-1";
  prog_char monitorStateString_5[] PROGMEM = "Status: Quiet";
  prog_char monitorStateString_6[] PROGMEM = "Storm: LOW";
  prog_char monitorStateString_7[] PROGMEM = "Storm: MODERATE";
  prog_char monitorStateString_8[] PROGMEM = "Storm: HIGH";
  prog_char monitorStateString_9[] PROGMEM = "Storm: EXTREME";

  PROGMEM const char *monitorStateTable[] =
  {   
    monitorStateString_0,
    monitorStateString_1,
    monitorStateString_2,
    monitorStateString_3,
    monitorStateString_4,
    monitorStateString_5,
    monitorStateString_6,
    monitorStateString_7,
    monitorStateString_8,
    monitorStateString_9,
  };

  prog_char lcd_info_blank_line_str[]  PROGMEM = "                ";
#endif

prog_char twitterStatus_none[]     PROGMEM = "";
prog_char twitterStatus_down[]     PROGMEM = "Down";
prog_char twitterStatus_quiet[]    PROGMEM = "Quiet";
prog_char twitterStatus_low[]      PROGMEM = "Low Storm";
prog_char twitterStatus_moderate[] PROGMEM = "Moderate Storm";
prog_char twitterStatus_major[]    PROGMEM = "Major Storm";
prog_char twitterStatus_extreme[]  PROGMEM = "Extreme Storm";

PROGMEM const char *twitterStatusTable[] =
{   
  twitterStatus_none,
  twitterStatus_none,
  twitterStatus_none,
  twitterStatus_none,
  twitterStatus_down,
  twitterStatus_quiet,
  twitterStatus_low,
  twitterStatus_moderate,
  twitterStatus_major,
  twitterStatus_extreme,
};


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
volatile unsigned int  flashTimerCounter;

#define FLASH_TIMER_1MSEC_RESET           130

#ifdef LCD_ENABLED
  volatile unsigned int  lcdKpInfoTimerCounter;
  volatile long          autoBacklightTimer;

  #define LCD_KP_INFO_ROTATION_INTERVAL_MS  5000

  #define AUTO_BACKLIGHT_TIMEOUT_MS         120000L  //2 mins
#endif

char *bufferPtr;
#define MAX_FLOAT_SCAN_LEN 7

AlarmID_t wingKpAlarm;

#ifdef LCD_ENABLED
  enum lcdInfoDisplayState
  {
     LCD_INFO_DISPLAY_STATE_NONE = 0,
     LCD_INFO_DISPLAY_STATE_1HR_KP,
     LCD_INFO_DISPLAY_STATE_4HR_KP,
     LCD_INFO_DISPLAY_STATE_EARTH_3HR_KP,
     LCD_INFO_DISPLAY_STATE_LAST_UPDATE_TIME,
     LCD_INFO_DISPLAY_STATE_UPTIME,
  };
  #define LCD_INFO_DISPLAY_STATE_LAST LCD_INFO_DISPLAY_STATE_UPTIME

  enum lcdInfoDisplayState lcdInfoDisplayState = LCD_INFO_DISPLAY_STATE_NONE;
  enum lcdInfoDisplayState lastLcdInfoDisplayState = LCD_INFO_DISPLAY_STATE_NONE;
#endif



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



//Converts a time in UTC to local time string

char *convertFromUTCToLocalTimeStr(int utcTime)
{
  static char ret[5+1];

  //Convert utcTime into minutes since midnight
  int mins  = utcTime % 100;
  int hours = utcTime / 100;
  int localTime = hours * 60 + mins;

  //Correct for the timezone offset
  localTime += TIMEZONE_OFFSET_FROM_UTC;

  //Correct for daylight savings
#ifdef LCD_ENABLED
  uint8_t dst = EEPROM.read(DAYLIGHT_SAVINGS_EEPROM_ADDR);
  if ( dst == 255 )
  {  
    dst = 1;
    EEPROM.write(DAYLIGHT_SAVINGS_EEPROM_ADDR, dst);
  }
  
  if ( dst )  localTime += DAYLIGHT_SAVINGS_OFFSET;
#else
  if ( DAYLIGHT_SAVINGS_ACTIVE )  localTime += DAYLIGHT_SAVINGS_OFFSET;
#endif

  //With the above adjustments, the time could be outside the 24 hour range,
  //we need to normalize it to fit
  localTime = localTime % (24 * 60);
  if ( localTime < 0 )  localTime += 24 * 60;

  //Now convert back into hours and mins
  hours = localTime / 60;
  mins = localTime % 60;

  //Now convert time into a str
  ret[0] = '0' + hours  / 10;
  ret[1] = '0' + hours  % 10;
  ret[2] = ':';
  ret[3] = '0' + mins / 10;
  ret[4] = '0' + mins % 10;
  ret[5] = '\0';
  
  return ret;
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
   sprintf_P(buffer, PSTR("%sKp: %s(%s) "), TWEET_PREFIX, kpToStr(wdataLast.target1Index), convertFromUTCToLocalTimeStr(wdataLast.target1Time));
   bufferPtr = buffer + strlen(buffer);
   sprintf_P(bufferPtr, PSTR("%s(%s) %s (earth "), kpToStr(wdataLast.target4Index), convertFromUTCToLocalTimeStr(wdataLast.target4Time), TIMEZONE); 
   bufferPtr += strlen(bufferPtr);
   sprintf_P(bufferPtr, PSTR("%s) Status: "), kpToStr(wdataLast.actual));
   bufferPtr += strlen(bufferPtr);
   sprintf_P(bufferPtr, (prog_char *)pgm_read_word(&(twitterStatusTable[monitorState])) );
          
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

#ifdef LCD_ENABLED
  lcdKpInfoTimerCounter = 0;
  lcdInfoDisplayState = LCD_INFO_DISPLAY_STATE_1HR_KP;
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
   
#ifdef WATCHDOG_TIMER_ENABLED
   lastSuccessfullUpdate = millis();
#endif
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
      if ( wingkpBytesReceived < 10L )
      {
         if ( wingkpRetryCount < NETWORK_RETRIES )
         {
            wingkpRetryCount ++;
            serialPgmPrint("Zero Bytes Received, Retry: ");
            Serial.println(wingkpRetryCount);
         
            wingkpBytesReceived = 0L;
            
            Alarm.timerOnce(1, wingkpRetrySubmit);
            
            if ( wingkpRetryCount == (NETWORK_RETRIES - 1) )
            {
                startWifi();
                serialPgmPrint("** WIFI REINITIALIZED");
            }
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

   wingkpBytesReceived += (long)len;
   
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
  wdataLast.target1Year            = scanFloat();
  wdataLast.target1Month           = scanFloat();
  wdataLast.target1Day             = scanFloat();
  wdataLast.target1Time            = scanFloat();
  wdataLast.target1Index           = scanFloat();
  wdataLast.target4Year            = scanFloat();
  wdataLast.target4Month           = scanFloat();
  wdataLast.target4Day             = scanFloat();
  wdataLast.target4Time            = scanFloat();
  wdataLast.target4Index           = scanFloat();
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

   wingkpBytesReceived = 0L;
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
   flashTimerCounter         ++;
#ifdef LCD_ENABLED
   lcdKpInfoTimerCounter     ++;
#endif

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
   
#ifdef LCD_ENABLED
   //Rotate the LCD Kp Index Info Banner
   if ( lcdKpInfoTimerCounter >= LCD_KP_INFO_ROTATION_INTERVAL_MS )
   {
      if      ( lcdInfoDisplayState == LCD_INFO_DISPLAY_STATE_NONE )  ;  //DO NOTHING
      else if ( lcdInfoDisplayState == LCD_INFO_DISPLAY_STATE_LAST )
            lcdInfoDisplayState = LCD_INFO_DISPLAY_STATE_1HR_KP;
      else  lcdInfoDisplayState = (enum lcdInfoDisplayState)(lcdInfoDisplayState + 1);

      lcdKpInfoTimerCounter = 0;     
   }
   
   //If device is in a non-storm state, timeout the backlight and switch off after 2 mins to save the backlight LEDs
   if ( monitorState < MONITOR_STATE_WINGKP_STORM_LOW && (millis() - autoBacklightTimer) >= AUTO_BACKLIGHT_TIMEOUT_MS )
   {
     lcdBacklightOff();
     autoBacklightTimer = millis();
   }
#endif
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



#ifdef LCD_ENABLED

void lcdPrintProgStr(const prog_char str[])
{
  char c;
  while ((c = pgm_read_byte(str++)))
    lcd.write(c);
}



void lcdPrintStatusStr(const prog_char str[])
{
  lcd.clear();
  lcdPrintProgStr(str);
}



void lcdDisplayState(void)
{
  lcd.clear();
  lcdPrintProgStr((prog_char *)pgm_read_word(&(monitorStateTable[monitorState])));
}



void lcdDisplayNumber(uint8_t num)
{
  uint8_t remainder = 0, q = 0;
  bool leadingSpace = true;

  //100's  
  q = num/100;
  if ( q != 0 )
  {
    lcd.write('0' + q);
    leadingSpace = false;
  }
  remainder = num - q * 100;

  //10's
  q = remainder / 10;
  if ( q != 0 || leadingSpace == false )
  {
    lcd.write('0' + q);
    leadingSpace = false;
  }
  remainder = remainder - q * 10;

  //1's
  lcd.write('0' + remainder);
}


void lcdDisplayLocalIp(void)
{
  lcd.clear();
  lcdDisplayNumber(local_ip[0]);
  lcd.print('.');
  lcdDisplayNumber(local_ip[1]);
  lcd.print('.');
  lcdDisplayNumber(local_ip[2]);
  lcd.print('.');
  lcdDisplayNumber(local_ip[3]);
}



void lcdPrintKpInfo(void)
{
  lcd.setCursor(0,1);
  lcdPrintProgStr(lcd_info_blank_line_str);
  lcd.setCursor(0,1);
  
  switch(lcdInfoDisplayState)
  {
   case LCD_INFO_DISPLAY_STATE_NONE:
     break;
     
   case LCD_INFO_DISPLAY_STATE_1HR_KP:
     lcdPrintProgStr(PSTR("1h Kp "));
     lcd.print(kpToStr(wdataLast.target1Index));
     lcd.write(32);
     lcd.print(convertFromUTCToLocalTimeStr(wdataLast.target1Time));
     break;
   
   case LCD_INFO_DISPLAY_STATE_4HR_KP:
     lcdPrintProgStr(PSTR("4h Kp "));
     lcd.print(kpToStr(wdataLast.target4Index));
     lcd.write(32);
     lcd.print(convertFromUTCToLocalTimeStr(wdataLast.target4Time));
     break;
   
   case LCD_INFO_DISPLAY_STATE_EARTH_3HR_KP:
     lcdPrintProgStr(PSTR("Earth 3hr   "));
     lcd.print(kpToStr(wdataLast.actual));
     break;
   
   case LCD_INFO_DISPLAY_STATE_LAST_UPDATE_TIME:
     lcdPrintProgStr(PSTR("LastUpdate "));
     lcd.print(convertFromUTCToLocalTimeStr(wdataLast.predTime));
     break;
     
   case LCD_INFO_DISPLAY_STATE_UPTIME:
     lcdPrintProgStr(PSTR("Uptime "));
     {
     unsigned long t = millis() / 1000UL;
     unsigned long days = t / (60UL * 60UL * 24UL);
     unsigned long hours = (t / (60UL * 60UL)) % 24UL;
     unsigned long mins = (t / 60UL ) % 60UL;
     lcd.print(days);
     lcd.write('d');
     lcd.print(hours);
     lcd.write('h');
     lcd.print(mins);
     lcd.write('m');
     }
     break;
   
   default:
     break;
  } 
}



void lcdBacklightOn(void)
{
  autoBacklightTimer = millis();
  digitalWrite(LCD_BACKLIGHT, HIGH);
}



void lcdBacklightOff(void)
{
  autoBacklightTimer = millis();
  digitalWrite(LCD_BACKLIGHT, LOW);
}



void lcdToggleBacklight(void)
{
  if ( digitalRead(LCD_BACKLIGHT) == HIGH )  lcdBacklightOff();
  else
  {
    lcdBacklightOn();
    lcdKpInfoTimerCounter = 0;
    lcdInfoDisplayState = LCD_INFO_DISPLAY_STATE_1HR_KP;
  }
}



void buttonPush(bool longPush)
{
  switch(buttonPushState)
  {
    case BUTTON_PUSH_STATE_NORMAL:
      if ( longPush )
      {
        buttonPushState = BUTTON_PUSH_STATE_DST_MODE;
        lcdBacklightOn();
      }
      else lcdToggleBacklight();
      break;
      
    case BUTTON_PUSH_STATE_DST_MODE:
      if ( longPush )
      {
        buttonPushState = BUTTON_PUSH_STATE_NORMAL;
      }
      else
      {
        uint8_t dst = EEPROM.read(DAYLIGHT_SAVINGS_EEPROM_ADDR);
        if ( dst == 255 )
        {
          dst = 1;
          EEPROM.write(DAYLIGHT_SAVINGS_EEPROM_ADDR, dst);
        }
        else
        {
          if ( dst == 1 )  dst = 0;
          else             dst = 1;
          EEPROM.write(DAYLIGHT_SAVINGS_EEPROM_ADDR, dst);
        }
      }
      break;
  }
}



void displayDstConfiguration(void)
{
  lcd.setCursor(0,1);
  lcdPrintProgStr(lcd_info_blank_line_str);
  lcd.setCursor(0,1);
  
  uint8_t dst = EEPROM.read(DAYLIGHT_SAVINGS_EEPROM_ADDR);
  if ( dst == 255 )
  {
    dst = 1;
    EEPROM.write(DAYLIGHT_SAVINGS_EEPROM_ADDR, dst);
  }
  else
  {
    if ( dst )  lcdPrintProgStr(PSTR("Dst ON"));
    else        lcdPrintProgStr(PSTR("Dst OFF"));
  }
}



void handlePushButton(void)
{
   uint8_t switchRead = digitalRead(PUSHBUTTON_SWITCH);
 
   switch(debounceState)
   {
      case DEBOUNCE_STATE_OFF:
        if ( switchRead == LOW )
        {
          debounceState = DEBOUNCE_STATE_TRANSITION_TO_ON;
          debounceTimer = millis();
        }
        break;
        
      case DEBOUNCE_STATE_TRANSITION_TO_ON:
        if (( millis() - debounceTimer ) >= DEBOUNCE_TIMER_MS )
        {
          if ( switchRead == HIGH )  debounceState = DEBOUNCE_STATE_OFF;
          else                      
          {
            debounceState = DEBOUNCE_STATE_ON;
            //Button has been pushed down
            buttonPressedAt = millis();
          }
        }
        break;
        
      case DEBOUNCE_STATE_ON:
        if ( switchRead == HIGH )
        {
          debounceState = DEBOUNCE_STATE_TRANSITION_TO_OFF;
          debounceTimer = millis();
        }
        break;
        
      case DEBOUNCE_STATE_TRANSITION_TO_OFF:
        if (( millis() - debounceTimer ) >= DEBOUNCE_TIMER_MS )
        {
          if ( switchRead == LOW )  debounceState = DEBOUNCE_STATE_ON;
          else                      
          {
              debounceState = DEBOUNCE_STATE_OFF;
              if (( millis() - buttonPressedAt ) >= LONG_BUTTON_PRESS_MS )
              {
                buttonPush(true);
              }
              else
              {
                buttonPush(false);
              }
          }
        }
        break;
   }
}

#endif



//Start the wifi module and connect to the access point

void startWifi(void)
{
  wdt_reset();

  if ( ! WiServer.init(NULL, WIFI_STARTUP_TIMEOUT_SECS) )
  {
    serialPgmPrintln("WiServer Failed");
    while(true);
  }
  
  wdt_reset();
  
  delay(1000);
  
  wdt_reset();
  
  //Setup the reply data return functions for the http get / post
  getWingKp.setReturnFunc(wingkpReplyData);
#ifdef TWITTER
  postTwitter.setReturnFunc(twitterReplyData);
#endif
}



void setup()
{
#ifdef WATCHDOG_TIMER_ENABLED
  reset_reason = GPIOR0;  //Store the reason for the reset for future use

  //Enable the watchdog timer
  wdt_enable(WDTO_8S);
#endif

#ifdef LCD_ENABLED
  lastMonitorState    = (enum MonitorState)-1;
  lcdInfoDisplayState = LCD_INFO_DISPLAY_STATE_NONE;
  
  //Initialize the LCD
  lcd.begin(LCD_COLUMNS, LCD_ROWS);
  pinMode(LCD_BACKLIGHT, OUTPUT);
  lcdBacklightOn();  //Backlight initially on
  lcdPrintStatusStr(PSTR("AuroraMonitor V2"));

  wdt_reset();
  delay(2000);
  wdt_reset();

  #ifdef WATCHDOG_TIMER_ENABLED
    //Print out the reason for the reset
    if ( reset_reason & _BV(WDRF))   lcdPrintStatusStr(PSTR("Watchdog Reset"));
    if ( reset_reason & _BV(BORF))   lcdPrintStatusStr(PSTR("Brownout Reset"));
    if ( reset_reason & _BV(EXTRF))  lcdPrintStatusStr(PSTR("External Reset"));
    if ( reset_reason & _BV(PORF))   lcdPrintStatusStr(PSTR("Power-on Reset"));

    wdt_reset();
    delay(2000);
    wdt_reset();
  #endif
#else
  //Time for hardware to startup
  wdt_reset();
  delay(1000);
  wdt_reset();
#endif
  
#ifdef LCD_ENABLED
  //Turn on pullup resistor on switch input
  pinMode(PUSHBUTTON_SWITCH, INPUT);
  digitalWrite(PUSHBUTTON_SWITCH, HIGH);
#endif

  Serial.begin(BAUD_RATE);  

#ifdef MEMORY_USAGE_REPORTING
   memoryReport();
#endif
 
#ifdef LED_TEST_AT_STARTUP
  //Cycle through all the colors/states at start up
  serialPgmPrintln("LED Test Sequence");
#ifdef LCD_ENABLED
  lcdPrintStatusStr(PSTR("LED Testing..."));
#endif
  for ( monitorState = MONITOR_STATE_STARTUP; monitorState <= MONITOR_STATE_LAST; monitorState = (enum MonitorState)(monitorState + 1) )
  {
      setColorForState(true);
      wdt_reset();
      delay(2000);
      setColorForState(false);
      wdt_reset();
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
#ifdef LCD_ENABLED
  lcdPrintStatusStr(PSTR("Starting Wifi..."));
#endif

  startWifi();
  
  //Print out the local ip address
#ifdef LCD_ENABLED
  lcdDisplayLocalIp();
#endif
  serialPgmPrint("Local IP: ");
  Serial.print(local_ip[0]);serialPgmPrint(".");
  Serial.print(local_ip[1]);serialPgmPrint(".");
  Serial.print(local_ip[2]);serialPgmPrint(".");
  Serial.println(local_ip[3]);

  //To allow the network to fire up
#ifdef LCD_ENABLED
  delay(5000);  //We delay extra when we have an LCD to display the IP address
#else
  delay(1000);
#endif
  
  //Setup the initial interval time
  wingKpSamplingInterval = WINGKP_SAMPLING_INITIAL;
  
  //Setoff an alarm to get the wingkp data the first time (2 seconds)
  Alarm.timerOnce(2, processWingKp);
  
#ifdef LCD_ENABLED
  autoBacklightTimer = millis();
#endif
  
#ifdef MEMORY_USAGE_REPORTING
  memoryReport();
#endif

#ifdef WATCHDOG_TIMER_ENABLED
  //Set the last successfull update time as now
  lastSuccessfullUpdate = millis();
#endif  
  
  wdt_reset();
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
  
#ifdef WATCHDOG_TIMER_ENABLED
   //Only reset the watchdog, if we've updated recently
   //This is used to forece an automatic restart of the arduino if we're not getting regular updates
   if (( millis() - lastSuccessfullUpdate ) < NO_PREDICTION_UPDATE_TIMEOUT_MS ) wdt_reset();
#endif
   
#ifdef LCD_ENABLED
   if      ( monitorState != lastMonitorState )
   {
     lcdDisplayState();
     lcdPrintKpInfo();  //We reprint the Kp Info, because Display State wiped it
     
     //If we've transitioned from non-storm to storm, then switch the backlight on
     if ( lastMonitorState < MONITOR_STATE_WINGKP_STORM_LOW && monitorState >= MONITOR_STATE_WINGKP_STORM_LOW)
       lcdBacklightOn();
     
     //If we've transitioned from storm to non-storm then switch the backlight off
     if ( lastMonitorState >= MONITOR_STATE_WINGKP_STORM_LOW && monitorState < MONITOR_STATE_WINGKP_STORM_LOW)
       lcdBacklightOff();

     lastMonitorState = monitorState;
   }
   else if ( buttonPushState == BUTTON_PUSH_STATE_DST_MODE )
   {
      //Handle the dst configuration state
      displayDstConfiguration();      
   }
   else if ( lcdInfoDisplayState != lastLcdInfoDisplayState )
   {
     lcdPrintKpInfo();
     lastLcdInfoDisplayState = lcdInfoDisplayState;
   }
   
   //Handle pushbutton switch and debounce
   handlePushButton();
#endif
   
#ifdef MEMORY_USAGE_REPORTING
   memoryLoopCounter ++;
   if ( memoryLoopCounter >= 2000 )  //20 seconds
   {
      memoryReport();
      memoryLoopCounter = 0;
   }
#endif
}


