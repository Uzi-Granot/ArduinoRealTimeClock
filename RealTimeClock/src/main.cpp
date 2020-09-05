/////////////////////////////////////////////////////////////////////
//
//	Arduino RealTimeClock
//
//	Author: Uzi Granot
//
//	Original publication date: August 12, 2020
//	Copyright (C) 2020 Uzi Granot. All Rights Reserved
//
//	Arduino RealTimeClock application is free software.
//	It is distributed under the Code Project Open License (CPOL).
//  You must read this document and agree
//	with the conditions specified in order to use this software.
//
//	Source Code and Executable Files can be used in commercial applications;
//	Source Code and Executable Files can be redistributed; and
//	Source Code can be modified to create derivative works.
//	No claim of suitability, guarantee, or any warranty whatsoever is
//	provided. The software is provided "as-is".
//	The Article accompanying the Work may not be distributed or republished
//	without the Author's consent
//
//	Version History:
//
//	Version 1.0 2020/08/12
//		Original revision
//	Version 1.1 2020/09/03
//		change of #include file Adafruit_MonoOLED.h
//    /* original include */
//    #include <Adafruit_I2CDevice.h>
//    #include <Adafruit_SPIDevice.h>
//    /* modified include */
//    #include <..\Adafruit BusIO\Adafruit_I2CDevice.h>
//    #include <..\Adafruit BusIO\Adafruit_SPIDevice.h>
//	Version 1.2 2020/09/04
//		Daylight saving time 
//		Restore #include file Adafruit_MonoOLED.h to original
//    Add following include to main.cpp
//    #include <Adafruit_I2CDevice.h>
//    #include <Adafruit_SPIDevice.h>
//    
/////////////////////////////////////////////////////////////////////

#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_SPIDevice.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define SET_BUTTON 9  // Set button is connected to Arduino pin D9
#define INC_BUTTON 8  // Inc button is connected to Arduino pin D8
#define DEC_BUTTON 7  // Dec button is connected to Arduino pin D7
#define ALARM_BUZZER 3 // alarm buzzer is connected to D3

#define ONE_WIRE_BUS 2

#define EEPROM_SETUP_INDEX 0
#define EEPROM_ALARM_MINUTE 1
#define EEPROM_ALARM_HOUR 2
#define EEPROM_ALARM_LENGTH 3

#define MAIN_MENU_ALARM 0
#define MAIN_MENU_DATE_TIME 1
#define MAIN_MENU_DAYLIGHT 2
#define MAIN_MENU_DISP_STYLE 3

#define DATE_FORMAT_YMD 0
#define DATE_FORMAT_DMY 1
#define DATE_FORMAT_MDY 2
#define DATE_FORMAT_MASK 3

#define DAYLIGHT_CANCEL 0
#define DAYLIGHT_SPRING 1
#define DAYLIGHT_FALL 2

#define TIME_FORMAT_24 0
#define TIME_FORMAT_12 1
#define TIME_FORMAT_MASK 4

#define TEMP_FORMAT_C 0
#define TEMP_FORMAT_F 1
#define TEMP_FORMAT_MASK 8

#define ALARM_SET_MASK 16
#define ALARM_NOT_ACTIVE 0
#define ALARM_ACTIVE 1
#define ALARM_RESET 2

#define SETUP_SUB_MENU 0

#define SETUP_ALARM_START 1
#define SETUP_ALARM_SET 1
#define SETUP_ALARM_HOUR 2
#define SETUP_ALARM_MINUTE 3
#define SETUP_ALARM_LENGTH 4
#define SETUP_ALARM_END 5

#define SETUP_DATE_TIME_START 5
#define SETUP_YEAR 5
#define SETUP_MONTH 6
#define SETUP_DAY 7
#define SETUP_HOUR 8
#define SETUP_MINUTE 9
#define SETUP_DATE_TIME_END 10

#define SETUP_DAYLIGHT_START 10
#define SETUP_DAYLIGHT_END 11

#define SETUP_FORMAT_START 11
#define SETUP_DATE_STYLE 11
#define SETUP_TIME_STYLE 12
#define SETUP_TEMP_UNIT 13
#define SETUP_FORMAT_END 14
#define SETUP_COUNT 15

#define STATE_CLOCK 0
#define STATE_SET_MENU 1
#define STATE_DISP_MENU 2
#define STATE_WAIT_RELEASE 3
#define STATE_SET_PARAM 4

#define BUTTONS_OFF 0
#define BUTTON_SET 1
#define BUTTON_INC 2
#define BUTTON_DEC 4

#define byteToChar1(X) (char) (((X) / 10) + '0')
#define byteToChar2(X) (char) (((X) % 10) + '0')

void drawText(byte y_pos, char *text, byte text_size);
void drawText(byte x_pos, byte y_pos, char *text, byte text_size);
void drawText(byte x_pos, byte y_pos, const char* text, byte text_size, bool highlight);
void drawText(byte y_pos, const char* text, byte text_size, bool highlight);
void displayClock();
void setClockModule(bool setDaylight);
void saveDisplayFormat();
void saveAlarmParameters();
void displaySetupMenu();
void displaySetupMenuParameters();
void drawMainMenuParam();
void drawAlarmOnOffMenu();
void drawDaylightMenu();
void drawDateStyleMenu();
void drawTimeStyleMenu();
void drawTempUnitMenu();
void tempToStr(int temp);
byte readRS3231();
void writeRS3231(byte value);
byte dayOfTheWeek();
byte hourToAMPM(char* ampm);
void getFreeMemory();
 
// Adafruit_SSD1306 display 128x64 constructor 
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// temperature probe class pointers
OneWire *oneWire = NULL;
DallasTemperature *probeSensor = NULL;
DeviceAddress deviceAddress;
bool probeAddressValid;

byte *paramPtr;
byte setupIndex;

byte eepromFlags;
byte eepromAlarmHour;
byte eepromAlarmMinute;
byte eepromAlarmLength;

byte dateStyle;
byte timeStyle;
byte tempUnit;

byte alarmSet;
byte alarmHour;
byte alarmMinute;
byte alarmLength;
byte alarmState;
unsigned long alarmTimer;

byte submenu; // 0 to 3
byte second; // 0 to 59
byte minute; // 0 to 59
byte hour; // 0 to 23
byte dayOfWeek; // 1-sunday to 7-saturday
byte day; // 1 to last day of the month
byte month; // 1 to 12
byte year; // 0 to 99

byte daylight;

byte state;
byte buttonsState;
unsigned long setMenuTimer;
unsigned long setMenuTimeout;

char dispStr[22];

// last day of the month
byte lastDayOfMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// day of the week
char dayText[7][10] =
  {
    "SUNDAY",
    "MONDAY",
    "TUESDAY",
    "WEDNESDAY",
    "THURSDAY",
    "FRIDAY",
    "SATURDAY"
  };

// parameter maximum value
byte paramMax[SETUP_COUNT] = {3, 1, 23, 59, 99, 99, 12, 31, 23, 59, 2, 2, 1, 1};

// save text strings in program memory
const PROGMEM char SetupStr1[] = {"CLOCK"};
const PROGMEM char SetupStr2[] = {"Alarm, Calendar,"};
const PROGMEM char SetupStr3[] = {"Temperature"};
const PROGMEM char SetupStr4[] = {"Local and Probe"};
const PROGMEM char UziGranot[] = {" By: Uzi Granot "};

const PROGMEM char selectMenuHeading[] = {"SELECT MENU"};
const PROGMEM char selectMenuAlarm[] = {" 1. ALARM CLOCK "};
const PROGMEM char selectMenuDateTime[] = {" 2. DATE TIME "};
const PROGMEM char selectMenuDaylight[] = {" 3. DAYLIGHT "};
const PROGMEM char selectMenuDispStyle[] = {" 4. DISP STYLE "};

const PROGMEM char DaylightMenuHeading[] = {"DAYLIGHT ADJUST"};
const PROGMEM char DaylightMenuCancel[] = {" CANCEL  "};
const PROGMEM char DaylightMenuSpring[] = {" SPRING +1 HOUR "};
const PROGMEM char DaylightMenuFall[] = {" FALL -1 HOUR  "};

const PROGMEM char alarmOnOffMenuHeading[] = {"ALARM CLOCK"};
const PROGMEM char alarmOnOffMenuOff[] = {" 1. ALARM OFF "};
const PROGMEM char alarmOnOffMenuOn[] = {" 2. ALARM ON "};

const PROGMEM char dateStyleMenuHeading[] = {"DATE STYLE MENU"};
const PROGMEM char dateStyleMenuYMD[] = {" 1. YYYY/MM/DD "};
const PROGMEM char dateStyleMenuDMY[] = {" 2. DD/MM/YYYY "};
const PROGMEM char dateStyleMenuMDY[] = {" 3. MM/DD/YYYY "};

const PROGMEM char timeStyleMenuHeading[] = {"TIME STYLE MENU"};
const PROGMEM char timeStyleMenu24[] = {" 1. 24 HOUR "};
const PROGMEM char timeStyleMenu12[] = {" 2. 12 HOUR "};

const PROGMEM char tempUnitMenuHeading[] = {"TEMPERATURE UNIT"};
const PROGMEM char tempUnitMenuC[] = {" 1. CELSIUS "};
const PROGMEM char tempUnitMenuF[] = {" 2. FAHRENHEIT "};

const PROGMEM char yearStr[] = {"YEAR"};
const PROGMEM char monthStr[] = {"MONTH"};
const PROGMEM char dayStr[] = {"DAY"};
const PROGMEM char hourStr[] = {"HOUR"};
const PROGMEM char minuteStr[] = {"MINUTE"};
const PROGMEM char lengthStr[] = {"LENGTH"};
const PROGMEM char setupStr[] = {"SETUP MENU"};
const PROGMEM char alarmSetStr[] = {"ALARM AT: "};

/////////////////////////////////////////////////////////////////////////
// Arduino setup method
// will be executed once at startup
/////////////////////////////////////////////////////////////////////////
void setup()
  {
  // wait a lttle
  delay(200);

  // menu mode buttons
  pinMode(SET_BUTTON, INPUT_PULLUP);
  pinMode(INC_BUTTON, INPUT_PULLUP);
  pinMode(DEC_BUTTON, INPUT_PULLUP);

  // alarm clock buzzer
  pinMode(ALARM_BUZZER, OUTPUT);
  digitalWrite(ALARM_BUZZER, HIGH);

  // display screen SSD1306 initialization
 	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE,BLACK);

  // Clear the display buffer.
  display.clearDisplay();

  // display initialization message
  drawText(0, SetupStr1, 2, false);
  drawText(18, SetupStr2, 1, false);
  drawText(30, SetupStr3, 1, false);
  drawText(42, SetupStr4, 1, false);
  drawText(54, UziGranot, 1, true);

  display.display();

  // wait 2 seconds
  delay(2000);
  
  // get saved parameters
  eepromFlags = EEPROM.read(EEPROM_SETUP_INDEX);
  if((eepromFlags & 0xe0) != 0 || (eepromFlags & DATE_FORMAT_MASK) == DATE_FORMAT_MASK)
    {
    eepromFlags = 0;
    EEPROM.write(EEPROM_SETUP_INDEX, eepromFlags);
    }
  
  // extract flags parameters
  dateStyle = eepromFlags & DATE_FORMAT_MASK;
  timeStyle = (eepromFlags & TIME_FORMAT_MASK) >> 2;
  tempUnit = (eepromFlags & TEMP_FORMAT_MASK) >> 3;
  alarmSet = (eepromFlags & ALARM_SET_MASK) >> 4;

  // get alarm time and length
  eepromAlarmHour = EEPROM.read(EEPROM_ALARM_HOUR);
  alarmHour = eepromAlarmHour;
  if(alarmHour >= 24) alarmHour = 12;
  eepromAlarmMinute = EEPROM.read(EEPROM_ALARM_MINUTE);
  alarmMinute = eepromAlarmMinute;
  if(alarmMinute >= 60) alarmMinute = 30;
  eepromAlarmLength = EEPROM.read(EEPROM_ALARM_LENGTH);
  alarmLength = eepromAlarmLength;
  if(alarmLength > 99) alarmLength = 5;

  // set one wire for temperature sensor
	oneWire = new OneWire();
	oneWire->begin(ONE_WIRE_BUS);

	// set temperature sensor
	probeSensor = new DallasTemperature();
	probeSensor->setOneWire(oneWire);
	probeSensor->begin();

  // temperature probe address
  probeAddressValid = false;
  if(probeSensor->getAddress(deviceAddress, 0)) probeAddressValid = true;

  // set clock display state
  state = STATE_CLOCK;
  return;
  }

/////////////////////////////////////////////////////////////////////////
// Arduino main program loop
/////////////////////////////////////////////////////////////////////////
void loop()
  {
  // buttons state
  buttonsState = BUTTONS_OFF;
  if(!digitalRead(SET_BUTTON)) buttonsState |= BUTTON_SET;
  if(!digitalRead(INC_BUTTON)) buttonsState |= BUTTON_INC;
  if(!digitalRead(DEC_BUTTON)) buttonsState |= BUTTON_DEC;

  // switch based on program state
  switch(state)
    {
    // normal state
    case STATE_CLOCK:
      // read clock module and display normal clock plus temperature screen
      displayClock();

      // set button is not pressed
      // and daylight set is not active
      if((buttonsState & BUTTON_SET) == 0 || daylight != DAYLIGHT_CANCEL) return;

      // set button is pressed
      // save current time
      setMenuTimer = millis();

      // and switch to menu state
      state = STATE_SET_MENU;  
      return;

    // set menu button was pressed
    case STATE_SET_MENU:
      // display normal clock plus temperature screen
      displayClock();

      // set button was pressed for at least 2 seconds
      if((int) (millis() - setMenuTimer) > 2000)
        {
        // go to set menu
        setupIndex = SETUP_SUB_MENU;
        state = STATE_DISP_MENU;
        return;
        }

      // set button was released before 2 seconds
      // go back to normal clock temperature display
      if((buttonsState & BUTTON_SET) == 0) state = STATE_CLOCK;

      // if set button is still pressed wait for 2 seconds
      return;

    // display one of the setup menu screens based on setupIndex
    // go to wait release button before allowing the user to
    // change setup values
    case STATE_DISP_MENU:
      displaySetupMenu();
      state = STATE_WAIT_RELEASE;
      return;

    // wait until set button is released
    case STATE_WAIT_RELEASE:
      // set button still pressed
      // stay in wait release button before allowing the user to
      // change setup values
      if((buttonsState & BUTTON_SET) != 0) return;

      // set button is released
      // save current time for no-action timeout test
      setMenuTimer = millis();
      setMenuTimeout = setMenuTimer;
      state = STATE_SET_PARAM;  
      return;

    case STATE_SET_PARAM:
      // look for timeout 12 seconds of no inc or dec buttons activity
      if((int) (millis() - setMenuTimeout) > 12000)
        {
        // all partial changes made will be ignored
        // restore parameters
        dateStyle = eepromFlags & DATE_FORMAT_MASK;
        timeStyle = (eepromFlags & TIME_FORMAT_MASK) >> 2;
        tempUnit = (eepromFlags & TEMP_FORMAT_MASK) >> 3;
        alarmSet = (eepromFlags & ALARM_SET_MASK) >> 4;
        alarmHour = eepromAlarmHour;
        alarmMinute = eepromAlarmMinute;
        alarmLength = eepromAlarmLength;

        // go back to normal display clock temperature state
        state = STATE_CLOCK;
        return;
        }

      // after 0.5 seconds and set button is pressed
      // move to next menu
      if((int) (millis() - setMenuTimer) > 500 && (buttonsState & BUTTON_SET) != 0)
        {
        // main menu exit
        if(setupIndex == SETUP_SUB_MENU)
          {
          if(submenu == MAIN_MENU_ALARM)
            {
            setupIndex = SETUP_ALARM_START;
            }
          else if(submenu == MAIN_MENU_DATE_TIME)
            {
            setupIndex = SETUP_DATE_TIME_START;
            }
          else if(submenu == MAIN_MENU_DAYLIGHT)
            {
            daylight = DAYLIGHT_CANCEL;
            setupIndex = SETUP_DAYLIGHT_START;
            }
          else
            {
            setupIndex = SETUP_FORMAT_START;
            }
          state = STATE_DISP_MENU;
          return;
          }
          
        // next state
        setupIndex++;

        // alarm parameters are done
        if(setupIndex == SETUP_ALARM_END ||
          (setupIndex == SETUP_ALARM_HOUR && alarmSet == 0))
          {
          // save new alarm settings
          saveDisplayFormat();
          saveAlarmParameters();
          state = STATE_CLOCK;
          return;
          }

        // date and time setup is done
        if(setupIndex == SETUP_DATE_TIME_END)
          {
          // upload new date ant time to clock module
          setClockModule(false);
          state = STATE_CLOCK;
          return;
          }

        // daylight setup is done
        if(setupIndex == SETUP_DAYLIGHT_END)
          {
          state = STATE_CLOCK;
          return;
          }

        // format setup is done (calendar date format, time format and temperature units)
        if(setupIndex == SETUP_FORMAT_END)
          {
          saveDisplayFormat();
          state = STATE_CLOCK;
          return;
          }

        // go to next step in the setup process
        state = STATE_DISP_MENU;
        return;
        }

      // both inc and dec buttons are off
      // just stay on the same setup menu
      if((buttonsState & (BUTTON_INC | BUTTON_DEC)) == 0) break;

      // either inc or dec button is pressed
      // reset the 12 second timeout
      setMenuTimeout = millis();

      // increment button was pressed
      if((buttonsState & BUTTON_INC) != 0)
        {
        // current parameter is at maximum value
        if(paramPtr[0] == paramMax[setupIndex])
          {
          // go back to minimum value
          paramPtr[0] = 0;

          // for month and day the minimum is 1
          if(setupIndex == SETUP_MONTH || setupIndex == SETUP_DAY) paramPtr[0]++;
          }
        // increment to next value
        else
          {
          paramPtr[0]++;
          }
        }

      // decrement
      else
        {
        // current parameter is at minimum value
        if(paramPtr[0] == 0 || (paramPtr[0] == 1 && (setupIndex == SETUP_MONTH || setupIndex == SETUP_DAY)))
          {
          // go to maximum value
          paramPtr[0] = paramMax[setupIndex];
          }
        // decrement to previous value
        else
          {
          paramPtr[0]--;
          }
        }

      // display new value
      displaySetupMenuParameters();

      // wait 0.5 second
      delay(500);
      return;
    }
  return;
  }

/////////////////////////////////////////////////////////////////////////
// display clock, calendar and temperature screen
/////////////////////////////////////////////////////////////////////////
void displayClock()
  {
  // get date and time
  // Start I2C protocol with DS3231 address and register 0
  Wire.beginTransmission(0x68);
  Wire.write(0);
  Wire.endTransmission(false);

  // Request 7 bytes from DS3231 and release I2C bus at end of reading
  // read registers 0 to 6
  Wire.requestFrom(0x68, 7);
  second = readRS3231();
  minute = readRS3231();
  hour = readRS3231();
  dayOfWeek = readRS3231();
  day = readRS3231();
  month = readRS3231();
  year = readRS3231();

  // daylight saving time adjustment
  if(daylight != DAYLIGHT_CANCEL)
    {
    // make sure adjustment is not done in the last 3 seconds of an hour
    if(minute < 59 && second < 57)
      {
      setClockModule(true);
      daylight = DAYLIGHT_CANCEL;
      }
    }

  // get temperature
  // Start I2C protocol with DS3231 address and register 17
  Wire.beginTransmission(0x68);
  Wire.write(17);
  Wire.endTransmission(false);

  // read temperature most and least significant bytes
  // Request 2 bytes from DS3231 and release I2C bus at end of reading
  Wire.requestFrom(0x68, 2);
  int temp_msb = Wire.read();
  int temp_lsb = Wire.read();

  // clock module temperature in 1/100 celsius
  int clockTemp = 25 * (((temp_msb << 8) | temp_lsb) >> 6);

  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus (we have only one)
  probeSensor->requestTemperatures();

  // get probe temperatore
  int probeTemp = probeSensor->getTemp((uint8_t*) deviceAddress);

  // convert to degree celcius
  // if error result will be PROBE_TEMP_ERROR -5500
  probeTemp = (int) ((25 * (long) probeTemp) >> 5);

  // clear display
  display.clearDisplay();

  // copy day of the week
  char* dayName = dayText[dayOfWeek - 1];
  byte strlen;
  for(strlen = 0; dayName[strlen] != 0; strlen++) dispStr[strlen] = dayName[strlen];

  // add one space
  dispStr[strlen++] = ' ';

  // Display the date year month day
  switch(dateStyle)
    {
    case DATE_FORMAT_YMD:
      dispStr[strlen++] = '2';
      dispStr[strlen++] = '0';
      dispStr[strlen++] = byteToChar1(year);
      dispStr[strlen++] = byteToChar2(year);
      dispStr[strlen++] = '/';
      dispStr[strlen++] = byteToChar1(month);
      dispStr[strlen++] = byteToChar2(month);
      dispStr[strlen++] = '/';
      dispStr[strlen++] = byteToChar1(day);
      dispStr[strlen++] = byteToChar2(day);
      break;

    case DATE_FORMAT_DMY:
      dispStr[strlen++] = byteToChar1(day);
      dispStr[strlen++] = byteToChar2(day);
      dispStr[strlen++] = '/';
      dispStr[strlen++] = byteToChar1(month);
      dispStr[strlen++] = byteToChar2(month);
      dispStr[strlen++] = '/';
      dispStr[strlen++] = '2';
      dispStr[strlen++] = '0';
      dispStr[strlen++] = byteToChar1(year);
      dispStr[strlen++] = byteToChar2(year);
      break;

    case DATE_FORMAT_MDY:
      dispStr[strlen++] = byteToChar1(month);
      dispStr[strlen++] = byteToChar2(month);
      dispStr[strlen++] = '/';
      dispStr[strlen++] = byteToChar1(day);
      dispStr[strlen++] = byteToChar2(day);
      dispStr[strlen++] = '/';
      dispStr[strlen++] = '2';
      dispStr[strlen++] = '0';
      dispStr[strlen++] = byteToChar1(year);
      dispStr[strlen++] = byteToChar2(year);
      break;
    }

  // Display the date year month day
  dispStr[strlen++] = 0;
  drawText(0, dispStr, 1);

  // Display the time
  char ampm;
  if(timeStyle == TIME_FORMAT_24)
    {
    dispStr[0] = byteToChar1(hour);
    dispStr[1] = byteToChar2(hour);
    }
  else
    {
    byte hour1 = hourToAMPM(&ampm);
    dispStr[0] = byteToChar1(hour1);
    dispStr[1] = byteToChar2(hour1);
    }
  dispStr[2] = ':';
  dispStr[3] = byteToChar1(minute);
  dispStr[4] = byteToChar2(minute);
  dispStr[5] = ':';
  dispStr[6] = byteToChar1(second);
  dispStr[7] = byteToChar2(second);

  if(timeStyle == TIME_FORMAT_24)
    {
    dispStr[8] = 0;
    }
  else
    {
    dispStr[8] = ampm;
    dispStr[9] = 'M';
    dispStr[10] = 0;
    }

  // display time
  drawText(15, dispStr, 2);
 
  // alarm is set
  if(alarmSet != 0)
    {
    strcpy_P(dispStr, (PGM_P) alarmSetStr);
    strlen = 10;
    if(timeStyle == TIME_FORMAT_24)
      {
      dispStr[strlen++] = byteToChar1(alarmHour);
      dispStr[strlen++] = byteToChar2(alarmHour);
      }
    else
      {
      byte hour1 = hourToAMPM(&ampm);
      dispStr[strlen++] = byteToChar1(hour1);
      dispStr[strlen++] = byteToChar2(hour1);
      }
    dispStr[strlen++] = ':';
    dispStr[strlen++] = byteToChar1(alarmMinute);
    dispStr[strlen++] = byteToChar2(alarmMinute);
    if(timeStyle == TIME_FORMAT_12)
      {
      dispStr[strlen++] = ampm;
      dispStr[strlen++] = 'M';
      }
    dispStr[strlen] = 0;
    drawText(33, dispStr, 1);
    }

  // convert clock module temperature to string
  tempToStr(clockTemp);

  // Display the temperature
  drawText(0, 45, (char*)"Local", 1);
  drawText(45, dispStr, 1);
  
  // convert to temperature to string
  tempToStr(probeTemp);

  // Display the temperature
  drawText(0, 57, (char*)"Probe", 1);
  drawText(57, dispStr, 1);

  // display temperature units
  display.drawCircle(110, 51, 3, WHITE);     // Put degree symbol ( ° )
  drawText(116, 48, tempUnit == TEMP_FORMAT_C ? (char*)"C" : (char*)"F", 2);

  // display normal clock screen  
  display.display();

  // test alarm
  if(alarmSet != 0)
    {
    switch(alarmState)
      {
      // wait for alarm hour and minute
      case ALARM_NOT_ACTIVE:
        if(hour == alarmHour && minute == alarmMinute)
          {
          digitalWrite(ALARM_BUZZER, LOW);
          alarmTimer = millis();
          alarmState = ALARM_ACTIVE;
          }
        break;

      // alarm is active
      // wait for alarm length in seconds
      case ALARM_ACTIVE:
        if((long) (millis() - alarmTimer) > 1000 * (long) alarmLength)
          {
          digitalWrite(ALARM_BUZZER, HIGH);
          alarmState = ALARM_RESET;
          }
        break;
      
      // wait until alarm start minute is over
      case ALARM_RESET:
        if(hour != alarmHour || minute != alarmMinute) alarmState = ALARM_NOT_ACTIVE;
        break;
      }
    }
  return;
  }

/////////////////////////////////////////////////////////////////////////
// set clock module parameters after user setup
/////////////////////////////////////////////////////////////////////////
void setClockModule
    (
    bool setDaylight  
    )
  {
  // Write data to DS3231 RTC
  Wire.beginTransmission(0x68); // Start I2C protocol with DS3231 address
  if(setDaylight)
    {
    // last day of the month
    byte lastDay = lastDayOfMonth[month];
    if(month == 2 && (year % 4) == 0) lastDay++;

    // daylight +1 hour adjustment
    if(daylight == DAYLIGHT_SPRING)
      {
      if(hour == 23)
        {
        hour = 0;
        if(day == lastDay)
          {
          day = 1;
          if(month == 12)
            {
            month = 1;
            if(year < 99) year++;
            }
          else
            {
            month++;
            }
          }
        else
          {
          day++;
          }
        }
      else
        {
        hour++;
        }
      }

    // daylight +1 hour adjustment
    else
      {
      if(hour == 0)
        {
        hour = 23;
        if(day == 1)
          {
          if(month == 1)
            {
            month = 12;
            if(year > 0) year--;
            }
          else
            {
            month--;
            }
          day = lastDay;
          }
        else
          {
          day--;
          }
        }
      else
        {
        hour--;
        }
      }
    Wire.write(2);              // Send register address (hour)
    }
  else
    {
    Wire.write(0);              // Send register address (seconds)
    Wire.write(0);              // Reset seconds
    writeRS3231(minute);        // Write minute
    }
  writeRS3231(hour);            // Write hour
  dayOfWeek = dayOfTheWeek();   // calculate day of the week from date
  writeRS3231(dayOfWeek);       // Write day of the week
  writeRS3231(day);             // Write day of the month
  writeRS3231(month);           // Write month
  writeRS3231(year);            // Write year
  Wire.endTransmission();       // Stop transmission and release the I2C bus
  delay(200);                   // Wait 200ms
  return;
  }

/////////////////////////////////////////////////////////////////////////
// save in eeprom display style flags
// year, month and day of the month
// time in 24 hours clock and 12 hours clock
// temperature units degree C or F
// alarm set
/////////////////////////////////////////////////////////////////////////
void saveDisplayFormat()
  {
  // test for parameters change
  byte newParameters = dateStyle | (timeStyle << 2) | (tempUnit << 3) | (alarmSet << 4);
  if(newParameters != eepromFlags)
    {
    // write setup parameters to eeprom
    EEPROM.write(EEPROM_SETUP_INDEX, newParameters);
    eepromFlags = newParameters;
    }
  return;
  }

/////////////////////////////////////////////////////////////////////////
// save alarm hour, minute and length
/////////////////////////////////////////////////////////////////////////
void saveAlarmParameters()
  {
  // alarm hour
  if(alarmHour != eepromAlarmHour)
    {
    eepromAlarmHour = alarmHour;
    EEPROM.write(EEPROM_ALARM_HOUR, eepromAlarmHour);
    }

  // alarm minute
  if(alarmMinute != eepromAlarmMinute)
    {
    eepromAlarmMinute = alarmMinute;
    EEPROM.write(EEPROM_ALARM_MINUTE, eepromAlarmMinute);
    }
    
  // alarm length
  if(alarmLength != eepromAlarmLength)
    {
    eepromAlarmLength = alarmLength;
    EEPROM.write(EEPROM_ALARM_LENGTH, eepromAlarmLength);
    }
  return;
  }

/////////////////////////////////////////////////////////////////////////
// display setup menu
/////////////////////////////////////////////////////////////////////////
void displaySetupMenu()
  {
  // clear display
  display.clearDisplay();

  // parameter name
  const char* name;

  // switch based on parameter
  switch(setupIndex)
    {
    case SETUP_SUB_MENU:
      drawText(0, selectMenuHeading, 1, false);
      submenu = MAIN_MENU_ALARM;
      drawMainMenuParam();
      paramPtr = &submenu;
      return;
      
    case SETUP_ALARM_SET:
      drawText(0, alarmOnOffMenuHeading, 1, false);
      drawAlarmOnOffMenu();
      paramPtr = &alarmSet;
      return;

    case SETUP_DAYLIGHT_START:
      drawText(0, DaylightMenuHeading, 1, false);
      drawDaylightMenu();
      paramPtr = &daylight;
      return;
      
    case SETUP_DATE_STYLE:
      if(dateStyle > 2) dateStyle = 0;
      drawText(0, dateStyleMenuHeading, 1, false);
      drawDateStyleMenu();
      paramPtr = &dateStyle;
      return;
      
    case SETUP_TIME_STYLE:
      if(timeStyle > 1) timeStyle = 0;
      drawText(0, timeStyleMenuHeading, 1, false);
      drawTimeStyleMenu();
      paramPtr = &timeStyle;
      return;
      
    case SETUP_TEMP_UNIT:
      if(tempUnit > 1) tempUnit = 0;
      drawText(0, tempUnitMenuHeading, 1, false);
      drawTempUnitMenu();
      paramPtr = &tempUnit;
      return;

    case SETUP_ALARM_HOUR:
      name = hourStr;
      paramPtr = &alarmHour;
      break;
      
    case SETUP_ALARM_MINUTE:
      name = minuteStr;
      paramPtr = &alarmMinute;
      break;
      
    case SETUP_ALARM_LENGTH:
      name = lengthStr;
      paramPtr = &alarmLength;
      break;
      
    case SETUP_YEAR:
      name = yearStr;
      paramPtr = &year;
      break;
      
    case SETUP_MONTH:
      name = monthStr;
      if(month == 0) month++;
      paramPtr = &month;
      break;
      
    case SETUP_DAY:
      name = dayStr;
      if(day == 0) day++;
      paramMax[SETUP_DAY] = lastDayOfMonth[month - 1];
      if(month == 2 && (year % 4) == 0) paramMax[SETUP_DAY]++;
      paramPtr = &day;
      break;
      
    case SETUP_HOUR:
      name = hourStr;
      paramPtr = &hour;
      break;
      
    case SETUP_MINUTE:
      name = minuteStr;
      paramPtr = &minute;
      break;

    }

  // make sure parameter is within limits
  if(paramPtr[0] > paramMax[setupIndex]) paramPtr[0] = paramMax[setupIndex];

  drawText(0, setupStr, 1, false);
  drawText(22, name, 2, false);
  displaySetupMenuParameters();
  return;
  }

/////////////////////////////////////////////////////////////////////////
// display one of the setup menus
/////////////////////////////////////////////////////////////////////////
void displaySetupMenuParameters()
  {
  switch(setupIndex)
    {
    case SETUP_SUB_MENU:
      drawMainMenuParam();
      return;

    case SETUP_ALARM_SET:
      drawAlarmOnOffMenu();
      return;

    case SETUP_DAYLIGHT_START:
      drawDaylightMenu();
      return;

    case SETUP_DATE_STYLE:
      drawDateStyleMenu();
      return;

    case SETUP_TIME_STYLE:
      drawTimeStyleMenu();
      return;

    case SETUP_TEMP_UNIT:
      drawTempUnitMenu();
      return;

    case SETUP_YEAR:
      dispStr[0] = '2';
      dispStr[1] = '0';
      dispStr[2] = byteToChar1(paramPtr[0]);
      dispStr[3] = byteToChar2(paramPtr[0]);
      dispStr[4] = 0;
      drawText(44, dispStr, 2);
      break;

    case SETUP_HOUR:
      if(timeStyle == TIME_FORMAT_12)
        {
        char ampm;
        byte hour1 = hourToAMPM(&ampm);
        dispStr[0] = byteToChar1(hour1);
        dispStr[1] = byteToChar2(hour1);
        dispStr[2] = ampm;
        dispStr[3] = 'M';
        dispStr[4] = 0;
        drawText(44, dispStr, 2);
        break;
        }

    default:
      dispStr[0] = byteToChar1(paramPtr[0]);
      dispStr[1] = byteToChar2(paramPtr[0]);
      dispStr[2] = 0;
      drawText(44, dispStr, 2);
      break;
    }
  display.display();
  return;
  }

/////////////////////////////////////////////////////////////////////////
// draw select menu choices
/////////////////////////////////////////////////////////////////////////
void drawMainMenuParam()
  {
  drawText(13, 16, selectMenuAlarm, 1, submenu == MAIN_MENU_ALARM);
  drawText(13, 28, selectMenuDateTime, 1, submenu == MAIN_MENU_DATE_TIME);
  drawText(13, 40, selectMenuDaylight, 1, submenu == MAIN_MENU_DAYLIGHT);
  drawText(13, 52, selectMenuDispStyle, 1, submenu == MAIN_MENU_DISP_STYLE);
  display.display();
  return;
  }

/////////////////////////////////////////////////////////////////////////
// draw alarm clock on or off
/////////////////////////////////////////////////////////////////////////
void drawAlarmOnOffMenu()
  {
  drawText(19, 28, alarmOnOffMenuOff, 1, alarmSet == 0);
  drawText(19, 40, alarmOnOffMenuOn, 1, alarmSet == 1);
  display.display();
  return;
  }

/////////////////////////////////////////////////////////////////////////
// draw daylight saving time
/////////////////////////////////////////////////////////////////////////
void drawDaylightMenu()
  {
  drawText(19, 28, DaylightMenuCancel, 1, daylight == DAYLIGHT_CANCEL);
  drawText(19, 40, DaylightMenuSpring, 1, daylight == DAYLIGHT_SPRING);
  drawText(19, 52, DaylightMenuFall, 1, daylight == DAYLIGHT_FALL);
  display.display();
  return;
  }

/////////////////////////////////////////////////////////////////////////
// draw date style menu choices
/////////////////////////////////////////////////////////////////////////
void drawDateStyleMenu()
  {
  drawText(16, 28, dateStyleMenuYMD, 1, dateStyle == 0);
  drawText(16, 40, dateStyleMenuDMY, 1, dateStyle == 1);
  drawText(16, 52, dateStyleMenuMDY, 1, dateStyle == 2);
  display.display();
  return;
  } 

/////////////////////////////////////////////////////////////////////////
// draw time style menu choices
/////////////////////////////////////////////////////////////////////////
void drawTimeStyleMenu()
  {
  drawText(16, 28, timeStyleMenu24, 1, timeStyle == 0);
  drawText(16, 40, timeStyleMenu12, 1, timeStyle == 1);
  display.display();
  return;
  } 

/////////////////////////////////////////////////////////////////////////
// draw temperature units choices 
/////////////////////////////////////////////////////////////////////////
void drawTempUnitMenu()
  {
  drawText(16, 28, tempUnitMenuC, 1, tempUnit == 0);
  drawText(16, 40, tempUnitMenuF, 1, tempUnit == 1);
  display.display();
  return;
  }

/////////////////////////////////////////////////////////////////////////
// convert raw temperatur to string
/////////////////////////////////////////////////////////////////////////
void tempToStr
    (
    int temp
    )
  {
  // convert temperature to Fahrenheit
  if(tempUnit != 0)
    {
    temp = (int) (((long) temp * 9) / 5) + 3200;
    }

  // string length
  byte strLen = 0;

  // negative temperature
  if(temp < 0)
    {
    temp = -temp;
    dispStr[0] = '-';
    strLen++;
    }
  
  // remove leading zeros
  bool noZero = false;

  // hundreds
  byte digit = (byte) (temp / 10000);
  if(digit > 0)
    {
    dispStr[strLen++] = digit + '0';
    temp -= 10000 * (int) digit;
    noZero = true;
    }

  // tens
  digit = (byte) (temp / 1000);
  if(digit > 0 || noZero)
    {
    dispStr[strLen++] = digit + '0';
    temp -= 1000 * (int) digit;
    noZero = true;
    }

  // ones
  digit = (byte) (temp / 100);
  dispStr[strLen++] = digit + '0';
  temp -= 100 * digit;

  // decimal point
  dispStr[strLen++] = '.';

  // one tenth
  digit = (byte) (temp / 10);
  dispStr[strLen++] = digit + '0';
  temp -= 10 * digit;

  // last digit 1/100
  dispStr[strLen++] = (byte) temp + '0';
  
  // terminating null
  dispStr[strLen++] = 0;

  // for debugging only
  //getFreeMemory();

  // return
  return;
  }

/////////////////////////////////////////////////////////////////////////
// read one register from real time clock
/////////////////////////////////////////////////////////////////////////
byte readRS3231()
  {
  // Read seconds from RS3231 register
  byte value = Wire.read();

  // convert bcd to integer
  value = (value >> 4) * 10 + (value & 0x0F);
  return value;
  }

/////////////////////////////////////////////////////////////////////////
// write one register to real time clock
/////////////////////////////////////////////////////////////////////////
void writeRS3231
    (
    byte value
    )
  {
  // Convert decimal to BCD and write to RS3231
  Wire.write(((value / 10) << 4) + (value % 10));
  return;
  }

/////////////////////////////////////////////////////////////////////////
// draw text to screen
/////////////////////////////////////////////////////////////////////////
void drawText(byte x_pos, byte y_pos, char *text, byte text_size)
  {
  display.setCursor(x_pos, y_pos);
  display.setTextSize(text_size);
  display.print(text);
  return;
  }

/////////////////////////////////////////////////////////////////////////
// draw text to screen. Text will be centered
/////////////////////////////////////////////////////////////////////////
void drawText(byte y_pos, char *text, byte text_size)
  {
  byte len;
  for(len = 0; text[len] != 0l; len++);
  drawText((byte) (128 - 6 * len * text_size) / 2, y_pos, text, text_size);
  return;
  }

/////////////////////////////////////////////////////////////////////////
// draw text stored in PROGMEM to the screen
/////////////////////////////////////////////////////////////////////////
void drawText(byte x_pos, byte y_pos, const char* text, byte text_size, bool highlight)
  {
  strcpy_P(dispStr, (PGM_P) text);
  if(highlight) display.setTextColor(BLACK,WHITE);
  drawText(x_pos, y_pos, (char*) dispStr, text_size);
  if(highlight) display.setTextColor(WHITE,BLACK);
  return;
  }

/////////////////////////////////////////////////////////////////////////
// draw text stored in PROGMEM to the screen
// text will be centered
/////////////////////////////////////////////////////////////////////////
void drawText(byte y_pos, const char* text, byte text_size, bool highlight)
  {
  strcpy_P(dispStr, (PGM_P) text);
  if(highlight) display.setTextColor(BLACK,WHITE);
  drawText(y_pos, (char*) dispStr, text_size);
  if(highlight) display.setTextColor(WHITE,BLACK);
  return;
  }

/////////////////////////////////////////////////////////////////////////
// calculate day of the week (1-Sunday to 7-Saturady)
// from year, month and day of the month
// will work for 2000 to end of 2099
/////////////////////////////////////////////////////////////////////////
byte dayOfTheWeek()
  {
	// leap year stuff
	byte year2 = year / 4;
	byte year3 = year - 4 * year2;

 	// day of the year
  int dayno = 1461 * year2 + 365 * year3 + day + 5;
  int end = month - 1;
  for(int m = 0; m < end; m++) dayno += lastDayOfMonth[m];
  
  // add one for any date not in leap year or
  // any date on or after march 1 of leap year 
  if(year3 > 0 || month > 2) dayno++;

	// final calculation
  return (byte) ((dayno % 7) + 1);
  }

/////////////////////////////////////////////////////////////////////////
// convert hour in 24 hour format to 12 hour format
/////////////////////////////////////////////////////////////////////////
byte hourToAMPM
    (
    char* ampm  
    )
  {
  // midnight
  if(hour == 0)
    {
    ampm[0] = 'A';  
    return 12;
    }
  // before noon
  if(hour < 12)
    {
    ampm[0] = 'A';  
    return hour;
    }
  // noon time
  if(hour == 12)
    {
    ampm[0] = 'P';  
    return 12;
    }
  // afternoon
  ampm[0] = 'P';
  return hour - 12;
  }

/////////////////////////////////////////////////////////////////////////
// find out how much free memory is available
// for debugging only
/////////////////////////////////////////////////////////////////////////
/*
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

extern char *__brkval;
int freeMemory()
  {
    char top;

  #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
  #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
    return &top - __brkval;
  #else  // __arm__
    return __brkval ? &top - __brkval : &top - __malloc_heap_start;
  #endif  // __arm__
  }

void getFreeMemory()
  {
  char memStr[5];
  int memLen = freeMemory();
  byte digit = (byte) (memLen / 1000);
  memStr[0] = digit + '0';
  memLen -= 1000 * (int) digit;

  digit = (byte) (memLen / 100);
  memStr[1] = digit + '0';
  memLen -= 100 * digit;

  digit = (byte) (memLen / 10);
  memStr[2] = digit + '0';
  memLen -= 10 * digit;

  // last digit 1/100
  memStr[3] = (byte) memLen + '0';

  // terminating null
  memStr[4] = 0;
  drawText(0, 33, memStr, 1);
  return;
  }
*/
