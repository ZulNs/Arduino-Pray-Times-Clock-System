/*
 Dot Matrix Display for Pray Times Clock System
 
 IR Remote Control for Setting
 
 Designed for Yusuf Kanoi's Final Project
 
 Created 6 April 2018
 @Gorontalo, Indonesia
 by ZulNs
 */


#include <Wire.h>
#include <RTClib.h>
#include <DMD2.h>
#include <fonts/SystemFont5x7.h>
#include <IRLremote.h>
#include <EEPROM.h>

#define IR_RX_PIN  2
#define BUZZER_PIN 12

#define DEFAULT_LATITUDE  0.6269914
#define DEFAULT_LONGITUDE 122.9798945
#define DEFAULT_ELEVATION 50
#define DEFAULT_TIMEZONE  8.0
#define MARQUEE_TIMEOUT   100
#define SETTING_TIMEOUT   30000
#define IR_ADDRESS        0xFF00
#define STR_FRIDAY        WEEKDAY[5]

#define IMSAK_FACTOR      10.0
#define FAJR_FACTOR       20.0
#define DHUHR_FACTOR      0.0
#define ASR_FACTOR        1.0
#define MAGHRIB_FACTOR    0.0
#define ISHA_FACTOR       18.0
#define MIDNIGHT_PORTION  0.5

#define CW                true
#define CCW               false

#define IMSAK_TIME          0
#define AFTER_IMSAK_TIME    1
#define FAJR_TIME           2
#define AFTER_FAJR_TIME     3
#define SUNRISE_TIME        4
#define AFTER_SUNRISE_TIME  5
#define DHUHR_TIME          6
#define AFTER_DHUHR_TIME    7
#define ASR_TIME            8
#define AFTER_ASR_TIME      9
#define MAGHRIB_TIME        10
#define AFTER_MAGHRIB_TIME  11
#define ISHA_TIME           12
#define AFTER_ISHA_TIME     13
#define MIDNIGHT_TIME       14
#define AFTER_MIDNIGHT_TIME 15

#define NUMBER_OF_TIMES 16
#define TIME_PERIODS    5
#define FRIDAY          5
#define ALARM_CYCLE     30

#define KEY_0           0
#define KEY_1           1
#define KEY_2           2
#define KEY_3           3
#define KEY_4           4
#define KEY_5           5
#define KEY_6           6
#define KEY_7           7
#define KEY_8           8
#define KEY_9           9
#define KEY_ASTERISK    10
#define KEY_NUMBER_SIGN 11
#define KEY_LEFT        12
#define KEY_RIGHT       13
#define KEY_UP          14
#define KEY_DOWN        15
#define KEY_OK          16

#define MODE_CLOCK            0x00
#define MODE_SELECT_TIME      0x01
#define MODE_SELECT_DATE      0x02
#define MODE_SELECT_YEAR      0x03
#define MODE_SELECT_TIMEZONE  0x04
#define MODE_SELECT_LATITUDE  0x05
#define MODE_SELECT_LONGITUDE 0x06
#define MODE_SELECT_ELEVATION 0x07
#define MODE_DISP_ABOUT       0x08
#define MODE_SELECT_OPTION    0x09
#define MODE_SET_TIME         0x11
#define MODE_SET_DATE         0x12
#define MODE_SET_YEAR         0x13
#define MODE_SET_TIMEZONE     0x14
#define MODE_SET_LATITUDE     0x15
#define MODE_SET_LONGITUDE    0x16
#define MODE_SET_ELEVATION    0x17
#define MODE_SET_MINUTE       0x31
#define MODE_SET_MONTH        0x32


struct Times
{
  float imsak    =  5.0;
  float fajr     =  5.0;
  float sunrise  =  6.0;
  float dhuhr    = 12.0;
  float asr      = 13.0;
  float sunset   = 18.0;
  float maghrib  = 18.0;
  float isha     = 18.0;
  float midnight =  0.0;
};

struct SunPosition
{
  float declination;
  float equation;
};

const char *WEEKDAY[] = {
  "Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jum'at", "Sabtu"
};

const char *MONTHNAME[] =
{
  "Januari",   "Februari", "Maret",    "April",
  "Mei",       "Juni",     "Juli",     "Agustus",
  "September", "Oktober",  "Nopember", "Desember"
};

const char STR_IMSAK[]    = "Imsak";
const char STR_FAJR[]     = "Shubuh";
const char STR_SUNRISE[]  = "Terbit";
const char STR_DHUHR[]    = "Dhuhr";
const char STR_ASR[]      = "Asr";
const char STR_MAGHRIB[]  = "Maghrib";
const char STR_ISHA[]     = "Isya";
const char STR_MIDNIGHT[] = "T.malam";
const char STR_ONTIME[]   = "Waktunya";
const char STR_PRAY[]     = "sholat";

const char STR_MENU_OPTIONS[] = "1:Jam 2:Tgl 3:Thn 4:Tmz 5:Ltg 6:Bjr 7:Elv 8:???";

const byte KEY_REMOTE[] =
{
  0x52, 0x16, 0x19, 0x0D, // 0     1     2     3
  0x0C, 0x18, 0x5E, 0x08, // 4     5     6     7
  0x1C, 0x5A, 0x42, 0x4A, // 8     9     *     #
  0x44, 0x43, 0x46, 0x15, // LEFT  RIGHT UP    DOWN
  0x40                    // OK
};

const int EEPROM_LATITUDE  = EEPROM.length() - 4;   // to store 4 bytes float
const int EEPROM_LONGITUDE = EEPROM.length() - 8;   // to store 4 bytes float
const int EEPROM_ELEVATION = EEPROM.length() - 10;  // to store 2 bytes int16_t
const int EEPROM_TIMEZONE  = EEPROM.length() - 11;  // to store 1 byte  int8_t

DS3231 rtc;

SoftDMD dmd(1, 1);  // DMD controls the entire display

CNec IRLremote;

DateTime now;

float lat, lng, jDate, setFloat;
unsigned long secondMarkTimeout, marqueeTimeout, settingTimeout;
boolean isSecondMarkDisplayed, isMarqueeActive, isOnPrayTime,
        isAlarmSet, isDecPointFloat;
int16_t elv, setElv; 
int8_t timeZone, setTimeZone,
       curTemp, marqueePosCtr;
byte modeOperation, curTimeId, curWeekDay, curDay, curMonth,
     marqueeChrCtr, marqueeMsgCtr, marqueeLimit,
     floatPosCtr, floatStrCtr,
     setHour, setMinute, setDay, setMonth, alarmCycle;
uint16_t curTime, nextTime, curYear, setYear,
         imsak, fajr, sunrise, dhuhr, asr, maghrib, isha, midnight;
char * ptrMarquee;
char strMarquee[24];
char strMarqueeBuffer[8];
char strBuffer[13];

void setup()
{
  Wire.begin();
  rtc.begin();
  dmd.begin();
  dmd.selectFont(SystemFont5x7);
  IRLremote.begin(IR_RX_PIN);
  //bitSet(PORTD, BUZZER_PIN); // Buzzer active low
  //bitSet(DDRD, BUZZER_PIN);
  
  Serial.begin(115200);
  Serial.println(F("*** Pray Times Calculation ***"));
  Serial.println();
  
  EEPROM.get(EEPROM_LATITUDE, lat);
  if (isnan(lat))
  {
    lat = DEFAULT_LATITUDE;
    EEPROM.put(EEPROM_LATITUDE, lat);
  }
  
  EEPROM.get(EEPROM_LONGITUDE, lng);
  if (isnan(lng))
  {
    lng = DEFAULT_LONGITUDE;
    EEPROM.put(EEPROM_LONGITUDE, lng);
  }
  
  EEPROM.get(EEPROM_ELEVATION, elv);
  if (isnan(elv))
  {
    elv = DEFAULT_ELEVATION;
    EEPROM.put(EEPROM_ELEVATION, elv);
  }
  
  EEPROM.get(EEPROM_TIMEZONE, timeZone);
  if (isnan(timeZone))
  {
    timeZone = DEFAULT_TIMEZONE;
    EEPROM.put(EEPROM_TIMEZONE, timeZone);
  }
  
  secondMarkTimeout = millis() + 500;
  modeOperation = MODE_CLOCK;
  marqueeMsgCtr = 0;
  now = rtc.now();
  initAll();
  
  /*Serial.print(WEEKDAY[curWeekDay]);
  Serial.print(F(", "));
  Serial.print(curDay);
  Serial.print(F(" "));
  Serial.print(MONTHNAME[curMonth - 1]);
  Serial.print(F(" "));
  Serial.print(curYear);
  Serial.println(F(" :"));
  
  Times times = getTimes(curYear, curMonth, curDay);
  
  Serial.print(F("Imsak\t: "));
  printTime(times.imsak);
  Serial.println();
  
  Serial.print(F("Fajr\t: "));
  printTime(times.fajr);
  Serial.println();
  
  Serial.print(F("Sunrise\t: "));
  printTime(times.sunrise);
  Serial.println();
  
  Serial.print(F("Dhuhr\t: "));
  printTime(times.dhuhr);
  Serial.println();
  
  Serial.print(F("Asr\t: "));
  printTime(times.asr);
  Serial.println();
  
  Serial.print(F("Sunset\t: "));
  printTime(times.sunset);
  Serial.println();
  
  Serial.print(F("Maghrib\t: "));
  printTime(times.maghrib);
  Serial.println();
  
  Serial.print(F("Isha\t: "));
  printTime(times.isha);
  Serial.println();
  
  Serial.print(F("Midnight: "));
  printTime(times.midnight);
  Serial.println();
  Serial.println();*/
  
  //rtc.adjust(DateTime(2018, 3, 26, 19, 5));
}

void loop()
{
  now = rtc.now();
  
  if (lowByte(curTime) != now.minute())
  {
    curTime = word(highByte(curTime), now.minute());
    if (modeOperation == MODE_CLOCK)
    {
      dmd.drawString(19, 0, getTimeFormatStr(now.minute()));
    }
  }

  if (highByte(curTime) != now.hour())
  {
    curTime = word(now.hour(), lowByte(curTime));
    if (modeOperation == MODE_CLOCK)
    {
      dmd.drawString(1, 0, getTimeFormatStr(now.hour()));
    }
  }
  
  if (curWeekDay != now.dayOfWeek() ||
      curDay != now.day() ||
      curMonth != now.month() ||
      curYear != now.year())
  {
    initTimes();
    initPrayTimes();
    if (modeOperation == MODE_CLOCK)
    {
      setStrCurDate();
      dispMarquee(strMarquee);
      marqueeMsgCtr = 1;
    }
  }
  
  if (curTime == nextTime)
  {
    if (curTime == maghrib)
    {
      updatePrayTimes();
    }
    curTimeId = (curTimeId + 1) % NUMBER_OF_TIMES;
    nextTime = getTimeById(curTimeId + 1);
    if (modeOperation == MODE_CLOCK)
    {
      setStrOnTime();
      dispMarquee(strMarquee);
      if (curTimeId == FAJR_TIME ||
          curTimeId == DHUHR_TIME ||
          curTimeId == ASR_TIME ||
          curTimeId == MAGHRIB_TIME ||
          curTimeId == ISHA_TIME)
      {
        marqueeMsgCtr = 255; // end of message
        isAlarmSet = true;
        alarmCycle = 0;
      }
      else
      {
        marqueeMsgCtr = 2;
      }
    }
  }
  
  if (isMarqueeActive && millis() > marqueeTimeout)
  {
    if (advanceMarquee())
    {
      if (modeOperation == MODE_CLOCK)
      {
        marqueeMsgCtr++;
        switch (marqueeMsgCtr)
        {
          case 0:
            setStrCurTemp();
            break;
          
          case 1:
            setStrCurDate();
            break;
          
          case 2:
            if (bitRead(curTimeId, 0)) // is curTimeId odd ?
            {
              setStrNextTime(curTimeId + 1);
            }
            else
            {
              setStrOnTime();
              if (curTimeId == FAJR_TIME ||
                  curTimeId == DHUHR_TIME ||
                  curTimeId == ASR_TIME ||
                  curTimeId == MAGHRIB_TIME ||
                  curTimeId == ISHA_TIME)
              {
                marqueeMsgCtr = 255; // end of message
              }
            }
            break;
          
          case 3:
            setStrNextTime(bitRead(curTimeId, 0) ? curTimeId + 3 : curTimeId + 2);
            break;
          
          case 4:
            setStrNextTime(bitRead(curTimeId, 0) ? curTimeId + 5 : curTimeId + 4);
            marqueeMsgCtr = 255; // end of message
        }
        dispMarquee(strMarquee);
      }
      else if (modeOperation == MODE_SELECT_OPTION)
      {
        dispMarquee(STR_MENU_OPTIONS);
      }
      else if (modeOperation == MODE_DISP_ABOUT)
      {
        dispMarquee(strMarquee);
      }
    }
  }
  
  if (millis() > secondMarkTimeout)
  {
    secondMarkTimeout = millis() + 500;
    isSecondMarkDisplayed = !isSecondMarkDisplayed;
    switch (modeOperation)
    {
      case MODE_CLOCK:
        if (isSecondMarkDisplayed)
        {
          dmd.drawString(13, 0, ":");
        }
        else
        {
          dmd.drawString(13, 0, " ");
        }
        break;
      
      case MODE_DISP_ABOUT:
        if (isSecondMarkDisplayed)
        {
          dmd.drawString(1, 0, strBuffer);
        }
        else
        {
          dmd.drawFilledBox(0, 0, 31, 7, GRAPHICS_OFF);
        }
        break;
      
      case MODE_SET_TIME:
        if (isSecondMarkDisplayed)
        {
          dmd.drawString(0, 8, getTimeFormatStr(setHour));
        }
        else
        {
          dmd.drawFilledBox(0, 8, 11, 15, GRAPHICS_OFF);
        }
        break;
      
      case MODE_SET_MINUTE:
        if (isSecondMarkDisplayed)
        {
          dmd.drawString(18, 8, getTimeFormatStr(setMinute));
        }
        else
        {
          dmd.drawFilledBox(18, 8, 29, 15, GRAPHICS_OFF);
        }
        break;
      
      case MODE_SET_DATE:
        if (isSecondMarkDisplayed)
        {
          dmd.drawString(0, 8, getTimeFormatStr(setDay));
        }
        else
        {
          dmd.drawFilledBox(0, 8, 11, 15, GRAPHICS_OFF);
        }
        break;
      
      case MODE_SET_MONTH:
        if (isSecondMarkDisplayed)
        {
          dmd.drawString(18, 8, getTimeFormatStr(setMonth));
        }
        else
        {
          dmd.drawFilledBox(18, 8, 29, 15, GRAPHICS_OFF);
        }
        break;
      
      case MODE_SET_YEAR:
        if (isSecondMarkDisplayed)
        {
          dmd.drawString(0, 8, getDecStr(setYear));
        }
        else
        {
          dmd.drawFilledBox(0, 8, 23, 15, GRAPHICS_OFF);
        }
        break;
      
      case MODE_SET_TIMEZONE:
        if (isSecondMarkDisplayed)
        {
          if (setTimeZone > 0)
          {
            dmd.drawString(0, 8, "+");
          }
          dmd.drawString(((setTimeZone >= 0) ? 6 : 0), 8, getDecStr(setTimeZone));
        }
        else
        {
          dmd.drawFilledBox(0, 8, 23, 15, GRAPHICS_OFF);
        }
        break;
      
      case MODE_SET_LATITUDE:
      case MODE_SET_LONGITUDE:
        if (isSecondMarkDisplayed)
        {
          dmd.drawString(0, 8, strBuffer + floatPosCtr);
        }
        else
        {
          dmd.drawFilledBox(0, 8, 31, 15, GRAPHICS_OFF);
        }
        break;
      
      case MODE_SET_ELEVATION:
        if (isSecondMarkDisplayed)
        {
          dmd.drawString(0, 8, getDecStr(setElv));
        }
        else
        {
          dmd.drawFilledBox(0, 8, 31, 15, GRAPHICS_OFF);
        }
    }
    
    if (isAlarmSet)
    {
      if (isSecondMarkDisplayed)
      {
        //bitClear(PORTD, BUZZER_PIN);
        tone(BUZZER_PIN, 5000, 500);
      }
      else
      {
        //bitSet(PORTD, BUZZER_PIN);
        alarmCycle++;
        if (alarmCycle == ALARM_CYCLE)
        {
          isAlarmSet = false;
        }
      }
    }
  }

  if (modeOperation != MODE_CLOCK && millis() > settingTimeout)
  {
    modeOperation = MODE_CLOCK;
    initAll();
  }
  
  if (IRLremote.available())
  {
    auto irData = IRLremote.read();
    if (irData.address == IR_ADDRESS)
    {
      byte idx = getIrKeyIndex(irData.command);
      if (modeOperation != MODE_CLOCK && idx != KEY_OK && idx != 255)
      {
        settingTimeout = millis() + SETTING_TIMEOUT;
      }
      switch (idx)
      {
        case KEY_ASTERISK:
          if (!isDecPointFloat &&
              (modeOperation == MODE_SET_LATITUDE  && -90.0  < setFloat && setFloat < 90.0 ||
               modeOperation == MODE_SET_LONGITUDE && -180.0 < setFloat && setFloat < 180.0))
          {
            if (floatStrCtr == 0 || floatStrCtr == 1 && strBuffer[0] == '-')
            {
              strBuffer[floatStrCtr++] = '0';
            }
            strBuffer[floatStrCtr++] = '.';
            strBuffer[floatStrCtr] = 0;
            floatPosCtr = 0;
            isDecPointFloat = true;
          }
          Serial.println(F("*"));
          break;
        
        case KEY_NUMBER_SIGN:
          if (modeOperation == MODE_SET_TIMEZONE)
          {
            setTimeZone = -setTimeZone;
          }
          else if (modeOperation == MODE_SET_ELEVATION)
          {
            setElv = -setElv;
          }
          else if ((modeOperation == MODE_SET_LATITUDE || modeOperation == MODE_SET_LONGITUDE)
                   && floatStrCtr == 0)
          {
            strBuffer[floatStrCtr++] = '-';
            strBuffer[floatStrCtr] = 0;
            floatPosCtr = 0;
          }
          Serial.println(F("#"));
          break;
        
        case KEY_LEFT:
          if (MODE_SELECT_TIME <= modeOperation && modeOperation <= MODE_DISP_ABOUT)
          {
            setModeSelect(incVal(modeOperation, MODE_SELECT_TIME, MODE_DISP_ABOUT, false));
          }
          else if (modeOperation == MODE_SET_MINUTE)
          {
            dmd.drawString(18, 8, getTimeFormatStr(setMinute));
            modeOperation = MODE_SET_TIME;
          }
          else if (modeOperation == MODE_SET_MONTH)
          {
            dmd.drawString(18, 8, getTimeFormatStr(setMonth));
            modeOperation = MODE_SET_DATE;
          }
          else if ((modeOperation == MODE_SET_LATITUDE || modeOperation == MODE_SET_LONGITUDE)
                   && floatPosCtr > 0)
          {
            floatPosCtr--;
          }
          Serial.println(F("Left"));
          break;
        
        case KEY_RIGHT:
          if (MODE_SELECT_TIME <= modeOperation && modeOperation <= MODE_DISP_ABOUT)
          {
            setModeSelect(incVal(modeOperation, MODE_SELECT_TIME, MODE_DISP_ABOUT, true));
          }
          else if (modeOperation == MODE_SET_TIME)
          {
            dmd.drawString(0, 8, getTimeFormatStr(setHour));
            modeOperation = MODE_SET_MINUTE;
          }
          else if (modeOperation == MODE_SET_DATE)
          {
            dmd.drawString(0, 8, getTimeFormatStr(setDay));
            modeOperation = MODE_SET_MONTH;
          }
          else if ((modeOperation == MODE_SET_LATITUDE || modeOperation == MODE_SET_LONGITUDE)
                   && floatPosCtr < (strlen(strBuffer) - 5))
          {
            floatPosCtr++;
          }
          Serial.println(F("Right"));
          break;
        
        case KEY_UP:
          if (MODE_SELECT_TIME <= modeOperation && modeOperation <= MODE_DISP_ABOUT)
          {
            setModeSelect(incVal(modeOperation, MODE_SELECT_TIME, MODE_DISP_ABOUT, false));
          }
          else if (modeOperation == MODE_SET_TIME)
          {
            setHour = incVal(setHour, 0, 23, true);
          }
          else if (modeOperation == MODE_SET_MINUTE)
          {
            setMinute = incVal(setMinute, 0, 59, true);
          }
          else if (modeOperation == MODE_SET_DATE)
          {
            setDay = incVal(setDay, 1, getDaysInMonth(setMonth, curYear), true);
          }
          else if (modeOperation == MODE_SET_MONTH)
          {
            setMonth = incVal(setMonth, 1, 12, true);
            if (setDay > getDaysInMonth(setMonth, curYear))
            {
              setDay = getDaysInMonth(setMonth, curYear);
              dmd.drawString(0, 8, getTimeFormatStr(setDay));
            }
          }
          else if (modeOperation == MODE_SET_YEAR)
          {
            setYear = incVal(setYear, 2000, 2099, true);
          }
          else if (modeOperation == MODE_SET_TIMEZONE)
          {
            setTimeZone = incVal(setTimeZone, -12, 12, true);
          }
          else if (modeOperation == MODE_SET_ELEVATION)
          {
            setElv = incVal(setElv, -9999, 9999, true);
          }
          else if ((modeOperation == MODE_SET_LATITUDE || modeOperation == MODE_SET_LONGITUDE)
                   && floatStrCtr > 0)
          {
            if (strBuffer[--floatStrCtr] == '.')
            {
              isDecPointFloat = false;
            }
            strBuffer[floatStrCtr] = 0;
            setFloat = atof(strBuffer);
            floatPosCtr = (floatStrCtr > 5) ? floatStrCtr - 5 : 0;
          }
          Serial.println(F("Up"));
          break;
        
        case KEY_DOWN:
          if (MODE_SELECT_TIME <= modeOperation && modeOperation <= MODE_DISP_ABOUT)
          {
            setModeSelect(incVal(modeOperation, MODE_SELECT_TIME, MODE_DISP_ABOUT, true));
          }
          else if (modeOperation == MODE_SET_TIME)
          {
            setHour = incVal(setHour, 0, 23, false);
          }
          else if (modeOperation == MODE_SET_MINUTE)
          {
            setMinute = incVal(setMinute, 0, 59, false);
          }
          else if (modeOperation == MODE_SET_DATE)
          {
            setDay = incVal(setDay, 1, getDaysInMonth(setMonth, curYear), false);
          }
          else if (modeOperation == MODE_SET_MONTH)
          {
            setMonth = incVal(setMonth, 1, 12, false);
            if (setDay > getDaysInMonth(setMonth, curYear))
            {
              setDay = getDaysInMonth(setMonth, curYear);
              dmd.drawString(0, 8, getTimeFormatStr(setDay));
            }
          }
          else if (modeOperation == MODE_SET_YEAR)
          {
            setYear = incVal(setYear, 2000, 2099, false);
          }
          else if (modeOperation == MODE_SET_TIMEZONE)
          {
            setTimeZone = incVal(setTimeZone, -12, 12, false);
          }
          else if (modeOperation == MODE_SET_ELEVATION)
          {
            setElv = incVal(setElv, -9999, 9999, false);
          }
          Serial.println(F("Down"));
          break;
        
        case KEY_OK:
          if (modeOperation == MODE_CLOCK)
          {
            modeOperation = MODE_SELECT_OPTION;
            dmd.clearScreen();
            dmd.drawString(0, 0, "SET?");
            dispMarquee(STR_MENU_OPTIONS);
            settingTimeout = millis() + SETTING_TIMEOUT;
          }
          else if (modeOperation == MODE_DISP_ABOUT || modeOperation == MODE_SELECT_OPTION)
          {
            modeOperation = MODE_CLOCK;
            setStrCurTemp();
            marqueeMsgCtr = 0;
            dispClock();
            dispMarquee(strMarquee);
          }
          else if (MODE_SELECT_TIME <= modeOperation && modeOperation <= MODE_SELECT_ELEVATION)
          {
            modeOperation |= 0x10;  // MODE_SELECT_XXX to MODE_SET_XXX
            switch (modeOperation)
            {
              case MODE_SET_TIME:
                setHour = highByte(curTime);
                setMinute = lowByte(curTime);
                break;
              case MODE_SET_DATE:
                setDay = curDay;
                setMonth = curMonth;
                break;
              case MODE_SET_YEAR:
                setYear = curYear;
                break;
              case MODE_SET_TIMEZONE:
                setTimeZone = timeZone;
                break;
              case MODE_SET_LATITUDE:
              case MODE_SET_LONGITUDE:
                setFloat = (modeOperation == MODE_SET_LATITUDE) ? lat : lng;
                floatPosCtr = 0;
                floatStrCtr = strlen(strBuffer);
                isDecPointFloat = floor(setFloat) != setFloat;
                break;
              case MODE_SET_ELEVATION:
                setElv = elv;
            }
          }
          else if (bitRead(modeOperation, 4))  // is MODE_SET_XXX
          {
            now = rtc.now();
            switch (modeOperation)
            {
              case MODE_SET_TIME:
              case MODE_SET_MINUTE:
                now.sethour(setHour);
                now.setminute(setMinute);
                now.setsecond(0);
                break;
              case MODE_SET_DATE:
              case MODE_SET_MONTH:
                now.setday(setDay);
                now.setmonth(setMonth);
                break;
              case MODE_SET_YEAR:
                if (now.day() > getDaysInMonth(now.month(), setYear))
                {
                  now.setday(getDaysInMonth(now.month(), setYear));
                }
                now.setyear(setYear);
                break;
              case MODE_SET_TIMEZONE:
                timeZone = setTimeZone;
                EEPROM.put(EEPROM_TIMEZONE, timeZone);
                break;
              case MODE_SET_LATITUDE:
                lat = atof(strBuffer);
                EEPROM.put(EEPROM_LATITUDE, lat);
                break;
              case MODE_SET_LONGITUDE:
                lng = atof(strBuffer);
                EEPROM.put(EEPROM_LONGITUDE, lng);
                break;
              case MODE_SET_ELEVATION:
                elv = setElv;
                EEPROM.put(EEPROM_ELEVATION, elv);
            }
            if (modeOperation == MODE_SET_TIME ||
                modeOperation == MODE_SET_MINUTE ||
                modeOperation == MODE_SET_DATE ||
                modeOperation == MODE_SET_MONTH ||
                modeOperation == MODE_SET_YEAR)
            {
              rtc.adjust(now);
            }
            modeOperation = MODE_CLOCK;
            initAll();
          }
          Serial.println(F("OK"));
          break;
        
        case 255:
          Serial.println(F("Unknown key"));
          break;
        
        default:  // KEY_0 to KEY_9
          if (MODE_SELECT_TIME <= modeOperation && modeOperation <= MODE_SELECT_OPTION &&
              KEY_1 <= idx && idx <= KEY_8 &&
              modeOperation != idx)
          {
            setModeSelect(idx);
          }
          switch (modeOperation)
          {
            case MODE_SET_TIME:
              setHour = setProperValue(setHour, idx, 0, 23);
              break;
            case MODE_SET_MINUTE:
              setMinute = setProperValue(setMinute, idx, 0, 59);
              break;
            case MODE_SET_DATE:
              setDay = setProperValue(setDay, idx, 1, getDaysInMonth(setMonth, curYear));
              break;
            case MODE_SET_MONTH:
              setMonth = setProperValue(setMonth, idx, 1, 12);
              if (setDay > getDaysInMonth(setMonth, curYear))
              {
                setDay = getDaysInMonth(setMonth, curYear);
                dmd.drawString(0, 8, getTimeFormatStr(setDay));
              }
              break;
            case MODE_SET_YEAR:
              setYear = setProperValue(setYear, idx, 0, 2099);
              if (setYear < 100)
              {
                setYear += 2000;
              }
              else if (setYear < 1000)
              {
                setYear = setYear % 100 + 2000;
              }
              else if (setYear < 2000)
              {
                setYear = setYear & 1000;
                setYear = setYear % 100 + 2000;
              }
              break;
            case MODE_SET_TIMEZONE:
              setTimeZone = setProperValue(setTimeZone, idx, 0, 12);
              break;
            case MODE_SET_ELEVATION:
              setElv = setProperValue(setElv, idx, 0, 10000);
              break;
            case MODE_SET_LATITUDE:
            case MODE_SET_LONGITUDE:
              if (floatStrCtr < 12)
              {
                if (!isDecPointFloat && floatStrCtr > 0 && strBuffer[floatStrCtr - 1] == '0' && setFloat == 0)
                {
                  if (idx == KEY_0)
                  {
                    break;
                  }
                  else
                  {
                    floatStrCtr--;
                  }
                }
                strBuffer[floatStrCtr++] = idx | 0x30;
                strBuffer[floatStrCtr] = 0;
                setFloat = atof(strBuffer);
                if ((modeOperation == MODE_SET_LATITUDE  && (setFloat < -90.0  || setFloat > 90.0)) ||
                    (modeOperation == MODE_SET_LONGITUDE && (setFloat < -180.0 || setFloat > 180.0)))
                {
                  strBuffer[--floatStrCtr] = 0;
                }
                setFloat = atof(strBuffer);
                floatPosCtr = (floatStrCtr > 5) ? floatStrCtr - 5 : 0;
              }
          }
          Serial.println(idx, DEC);
          break;
      }
    }
  }
}


//--------- Convertion Functions ---------

void initAll()
{
  initTimes();
  initPrayTimes();
  setStrCurTemp();
  marqueeMsgCtr = 0;
  dispClock();
  dispMarquee(strMarquee);
}

void initTimes()
{
  curTime = word(now.hour(), now.minute());
  curWeekDay = now.dayOfWeek();
  curDay = now.day();
  curMonth = now.month();
  curYear = now.year();
  curTemp = floatToByte(rtc.getTemp());
}

void initPrayTimes()
{
  updatePrayTimes();
  curTimeId = getTimeId();
  nextTime = getTimeById(curTimeId + 1);
}

void dispClock()
{
  dmd.drawFilledBox(0, 0, 31, 7, GRAPHICS_OFF); // clears top screen
  dmd.drawString(1, 0, getTimeFormatStr(highByte(curTime)));
  if (isSecondMarkDisplayed)
  {
    dmd.drawString(13, 0, ":");
  }
  dmd.drawString(19, 0, getTimeFormatStr(lowByte(curTime)));
}

boolean dispMarquee(const char * str)
{
  marqueeChrCtr = 0;
  marqueePosCtr = 16;
  ptrMarquee = str;
  marqueeLimit = strlen(ptrMarquee);
  strncpy(strMarqueeBuffer, ptrMarquee, 7);
  strMarqueeBuffer[7] = 0;
  dmd.drawFilledBox(0, 8, 31, 15, GRAPHICS_OFF); // clears bottom screen
  dmd.drawString(marqueePosCtr, 8, strMarqueeBuffer);
  isMarqueeActive = true;
  marqueeTimeout = millis() + MARQUEE_TIMEOUT;
}

boolean advanceMarquee()
{
  marqueePosCtr--;
  if (marqueePosCtr == -6)
  {
    marqueePosCtr = 0;
    marqueeChrCtr++;
    if (marqueeChrCtr == marqueeLimit)
    {
      return true;
    }
    else
    {
      strncpy(strMarqueeBuffer, ptrMarquee + marqueeChrCtr, 7);
      strBuffer[7] = 0;
    }
  }
  dmd.drawString(marqueePosCtr, 8, strMarqueeBuffer);
  marqueeTimeout = millis() + MARQUEE_TIMEOUT;
  return false;
}

byte getIrKeyIndex(byte irCommand)
{
  for (byte i = 0; i < 17; i++)
  {
    if (irCommand == KEY_REMOTE[i])
    {
      return i;
    }
  }
  return 255;
}

void updatePrayTimes()
{
  Times times = getTimes(curYear, curMonth, curDay);
  
  dhuhr    = floatTimeToWord(times.dhuhr);
  asr      = floatTimeToWord(times.asr);
  maghrib  = floatTimeToWord(times.maghrib);
  isha     = floatTimeToWord(times.isha);
  midnight = floatTimeToWord(times.midnight);

  if (maghrib <= curTime && curTime <= 0x173B) // current time between maghrib and 23:59
  {
    times   = getTimes(curYear, curMonth, curDay + 1);
  }
  imsak    = floatTimeToWord(times.imsak);
  fajr     = floatTimeToWord(times.fajr);
  sunrise  = floatTimeToWord(times.sunrise);
}

byte getTimeId()
{
  if (isTimeInRange(curTime, addMinutes(isha, TIME_PERIODS), midnight))
  {
    return AFTER_ISHA_TIME;
  }
  if (isTimeInRange(curTime, midnight, addMinutes(midnight, TIME_PERIODS)))
  {
    return MIDNIGHT_TIME;
  }
  if (isTimeInRange(curTime, addMinutes(midnight, TIME_PERIODS), imsak))
  {
    return AFTER_MIDNIGHT_TIME;
  }
  if (curTime < addMinutes(imsak, TIME_PERIODS))
  {
    return IMSAK_TIME;
  }
  if (curTime < fajr)
  {
    return AFTER_IMSAK_TIME;
  }
  if (curTime < addMinutes(fajr, TIME_PERIODS))
  {
    return FAJR_TIME;
  }
  if (curTime < sunrise)
  {
    return AFTER_FAJR_TIME;
  }
  if (curTime < addMinutes(sunrise, TIME_PERIODS))
  {
    return SUNRISE_TIME;
  }
  if (curTime < dhuhr)
  {
    return AFTER_SUNRISE_TIME;
  }
  if (curTime < addMinutes(dhuhr, TIME_PERIODS))
  {
    return DHUHR_TIME;
  }
  if (curTime < asr)
  {
    return AFTER_DHUHR_TIME;
  }
  if (curTime < addMinutes(asr, TIME_PERIODS))
  {
    return ASR_TIME;
  }
  if (curTime < maghrib)
  {
    return AFTER_ASR_TIME;
  }
  if (curTime < addMinutes(maghrib, TIME_PERIODS))
  {
    return MAGHRIB_TIME;
  }
  if (curTime < isha)
  {
    return AFTER_MAGHRIB_TIME;
  }
  return ISHA_TIME;
}

uint16_t getTimeById(const byte id)
{
  switch (id % NUMBER_OF_TIMES)
  {
    case IMSAK_TIME:
      return imsak;
    case AFTER_IMSAK_TIME:
      return addMinutes(imsak, TIME_PERIODS);
    case FAJR_TIME:
      return fajr;
    case AFTER_FAJR_TIME:
      return addMinutes(fajr, TIME_PERIODS);
    case SUNRISE_TIME:
      return sunrise;
    case AFTER_SUNRISE_TIME:
      return addMinutes(sunrise, TIME_PERIODS);
    case DHUHR_TIME:
      return dhuhr;
    case AFTER_DHUHR_TIME:
      return addMinutes(dhuhr, TIME_PERIODS);
    case ASR_TIME:
      return asr;
    case AFTER_ASR_TIME:
      return addMinutes(asr, TIME_PERIODS);
    case MAGHRIB_TIME:
      return maghrib;
    case AFTER_MAGHRIB_TIME:
      return addMinutes(maghrib, TIME_PERIODS);
    case ISHA_TIME:
      return isha;
    case AFTER_ISHA_TIME:
      return addMinutes(isha, TIME_PERIODS);
    case MIDNIGHT_TIME:
      return midnight;
  }
  return addMinutes(midnight, TIME_PERIODS);
}

char * getTimeNameById(const byte id)
{
  switch ((id % NUMBER_OF_TIMES) & 0x0E)
  {
    case IMSAK_TIME:
      return STR_IMSAK;
    case FAJR_TIME:
      return STR_FAJR;
    case SUNRISE_TIME:
      return STR_SUNRISE;
    case DHUHR_TIME:
      return ((curWeekDay == FRIDAY) ? STR_FRIDAY : STR_DHUHR);
    case ASR_TIME:
      return STR_ASR;
    case MAGHRIB_TIME:
      return STR_MAGHRIB;
    case ISHA_TIME:
      return STR_ISHA;
  }
  return STR_MIDNIGHT;
}

byte getDaysInMonth(const byte month, const uint16_t year)
{
  switch (month)
  {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      return 31;
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
    case 2:
      if ((year % 4) == 0 && year != 2100)
      {
        return 29;
      }
      else
      {
        return 28;
      }
  }
  return 0;
}

int16_t setProperValue(const int16_t val, const byte adder, const int16_t minVal, const int16_t maxVal)
{
  int16_t val0 = (val > 0) ? val : -val;
  boolean sign = val < 0;
  int16_t divider = 1000;
  while ((maxVal % divider) == maxVal && divider > 10)
  {
    divider /= 10;
  }
  val0 = (val0 % divider) * 10 + adder;
  if (val0 > maxVal)
  {
    val0 %= divider;
  }
  if (val0 < minVal)
  {
    val0 = sign ? -val : val;
  }
  if (sign)
  {
    val0 = -val0;
  }
  return val0;
}

int16_t incVal(const int16_t val, const int16_t minVal, const int16_t maxVal, boolean inc)
{
  int16_t res = val;
  res = inc ? ++res : --res;
  if (res > maxVal)
  {
    res = minVal;
  }
  else if (res < minVal)
  {
    res = maxVal;
  }
  return res;
}

void setModeSelect(const byte mode)
{
  modeOperation = mode;
  dmd.clearScreen();
  if (MODE_SELECT_TIME <= mode && mode <= MODE_SELECT_ELEVATION)
  {
    isMarqueeActive = false;
    strncpy(strBuffer, STR_MENU_OPTIONS + ((mode - 1) * 6 + 2), 3);
    strBuffer[3] = '?';
    strBuffer[4] = 0;
    dmd.drawString(0, 0, strBuffer);
  }
  switch (mode)
  {
    case MODE_SELECT_TIME:
      dmd.drawString(0, 8, getTimeStr(curTime));
      break;
    
    case MODE_SELECT_DATE:
      strcpy(strMarqueeBuffer, getTimeFormatStr(curDay));
      strcat(strMarqueeBuffer, "/");
      strcat(strMarqueeBuffer, getTimeFormatStr(curMonth));
      dmd.drawString(0, 8, strMarqueeBuffer);
      break;
    
    case MODE_SELECT_YEAR:
      dmd.drawString(0, 8, getDecStr(curYear));
      break;
    
    case MODE_SELECT_TIMEZONE:
      if (timeZone > 0)
      {
        dmd.drawString(0, 8, "+");
      }
      dmd.drawString(((timeZone >= 0) ? 6 : 0), 8, getDecStr(timeZone));
      break;
    
    case MODE_SELECT_LATITUDE:
      dmd.drawString(0, 8, floatToStr(lat));
      break;
    
    case MODE_SELECT_LONGITUDE:
      dmd.drawString(0, 8, floatToStr(lng));
      break;
    
    case MODE_SELECT_ELEVATION:
      dmd.drawString(0, 8, getDecStr(elv));
      break;
    
    case MODE_DISP_ABOUT:
      strBuffer[0] = 0x5A;
      strBuffer[1] = 0x75;
      strBuffer[2] = 0x6C;
      strBuffer[3] = 0x4E;
      strBuffer[4] = 0x73;
      strBuffer[5] = 0;
      strMarquee[0] = 0x4A;
      strMarquee[1] = 0x54;
      strMarquee[2] = 0x45;
      strMarquee[3] = 0x2D;
      strMarquee[4] = 0x46;
      strMarquee[5] = 0x54;
      strMarquee[6] = 0x2D;
      strMarquee[7] = 0x55;
      strMarquee[8] = 0x4E;
      strMarquee[9] = 0x47;
      strMarquee[10] = 0;
      dispMarquee(strMarquee);
  }
}

void setStrCurTemp()
{
  strMarquee[0] = 0;
  strcat(strMarquee, getDecStr(curTemp));
  strcat(strMarquee, "`C");
}

void setStrCurDate()
{
  strcpy(strMarquee, WEEKDAY[curWeekDay]);
  strcat(strMarquee, ", ");
  strcat(strMarquee, getDecStr(curDay));
  strcat(strMarquee, " ");
  strcat(strMarquee, MONTHNAME[curMonth - 1]);
  strcat(strMarquee, " ");
  strcat(strMarquee, getDecStr(curYear));
}

void setStrOnTime()
{
  strcpy(strMarquee, STR_ONTIME);
  strcat(strMarquee, " ");
  if (curTimeId != IMSAK_TIME && curTimeId != SUNRISE_TIME && curTimeId != MIDNIGHT_TIME)
  {
    strcat(strMarquee, STR_PRAY);
    strcat(strMarquee, " ");
  }
  strcat(strMarquee, getTimeNameById(curTimeId));
}

void setStrNextTime(const byte timeId)
{
  strcpy(strMarquee, getTimeNameById(timeId));
  strcat(strMarquee, " ");
  strcat(strMarquee, getTimeStr(getTimeById(timeId)));
}

char * floatToStr(const float num)
{
  byte len;
  dtostrf(num, 12, 7, strBuffer);
  len = strlen(strBuffer);
  while (strBuffer[--len] == '0')
  {
    strBuffer[len] = 0;
  }
  if (strBuffer[len] == '.')
  {
    strBuffer[len] = 0;
  }
  len = 0;
  while (strBuffer[len] == ' ')
  {
    len++;
  }
  if (len > 0)
  {
    strncpy(strBuffer, strBuffer + len, 13 - len);
  }
  return strBuffer;
}

char * getTimeStr(const uint16_t time)
{
  char str[6];
  strcpy(str, getTimeFormatStr(highByte(time)));
  strcat(str, ":");
  strcat(str, getTimeFormatStr(lowByte(time)));
  strcpy(strBuffer, str);
  return strBuffer;
}

char * getTimeFormatStr(const byte time)
{
  char str[3];
  if (time < 10)
  {
    strcat(str, "0");
  }
  strcat(str, getDecStr(time));
  strcpy(strBuffer, str);
  return strBuffer;
}

char * getDecStr(const int16_t dec)
{
  itoa(dec, strBuffer, 10);
  return strBuffer;
}

int8_t floatToByte(float floatNum)
{
  return (int8_t) floatNum; 
}

uint16_t floatTimeToWord(float time)
{
  byte hours, minutes;
  time = fixHour(time + 0.5 / 60.0);
  hours   = (byte) floor(time);
  minutes = (byte) floor((time - (float) hours) * 60.0);
  return word(hours, minutes);
}

uint16_t addMinutes(const uint16_t time, const byte minutes)
{
  byte low = lowByte(time);
  byte high = highByte(time);
  low += minutes;
  high += low / 60;
  low %= 60;
  high %= 24;
  return word(high, low);
}

boolean isTimeInRange(const uint16_t time, const uint16_t timeBefore, const uint16_t timeAfter)
{
  if (timeBefore == time)
  {
    return true;
  }
  if (timeBefore < time && time < timeAfter)
  {
    return true;
  }
  if (time < timeBefore && time < timeAfter && timeAfter < timeBefore)
  {
    return true;
  }
  if (timeBefore < time && timeAfter < time && timeAfter < timeBefore)
  {
    return true;
  }
  return false;
}

void printTime(float time)
{
  uint16_t wordTime = floatTimeToWord(time);
  strcpy(strBuffer, getTimeStr(floatTimeToWord(wordTime)));
  Serial.print(strBuffer);
}


//--------- Get Prayer Times Functions ---------

Times getTimes(int year, byte month, byte day)
{
  Times times;
  
  jDate = julian(year, month, day) - lng / 360.0;
  
  times = dayPortion(times);
  
  times.imsak   = sunAngleTime(IMSAK_FACTOR,   times.imsak,   CCW);
  times.fajr    = sunAngleTime(FAJR_FACTOR,    times.fajr,    CCW);
  times.sunrise = sunAngleTime(riseSetAngle(), times.sunrise, CCW);
  times.dhuhr   = midDay(times.dhuhr);
  times.asr     = asrTime(ASR_FACTOR, times.asr);
  times.sunset  = sunAngleTime(riseSetAngle(), times.sunset,  CW);
  times.maghrib = sunAngleTime(MAGHRIB_FACTOR, times.maghrib, CW);
  times.isha    = sunAngleTime(ISHA_FACTOR,    times.isha,    CW);
  
  times.imsak   += timeZone - lng / 15.0;
  times.fajr    += timeZone - lng / 15.0;
  times.sunrise += timeZone - lng / 15.0;
  times.dhuhr   += timeZone - lng / 15.0;
  times.asr     += timeZone - lng / 15.0;
  times.sunset  += timeZone - lng / 15.0;
  times.maghrib += timeZone - lng / 15.0;
  times.isha    += timeZone - lng / 15.0;
  
  times = adjustHighLats(times);
  
  times.imsak = times.fajr - IMSAK_FACTOR / 60.0;
  times.maghrib = times.sunset + MAGHRIB_FACTOR / 60.0;
  times.dhuhr += DHUHR_FACTOR / 60.0;
  
  times.midnight = times.sunset + timeDiff(times.sunset, times.sunrise) / 2.0;
  
  return times;
}


//--------- Calculation Functions ---------

float midDay(float time)
{
  float eqt = sunPosition(time).equation;
  float noon = fixHour(12.0 - eqt);
  return noon;
}

float sunAngleTime(float angle, float time, boolean direction)
{
  float decl = sunPosition(time).declination;
  float noon = midDay(time);
  float t = 1.0 / 15.0 * darccos((-dsin(angle) - dsin(decl) * dsin(lat)) / (dcos(decl) * dcos(lat)));
  return noon + (direction ? t : -t);
}

float asrTime(float factor, float time)
{
  float decl = sunPosition(time).declination;
  float angle = -darccot(factor + dtan(abs(lat - decl)));
  return sunAngleTime(angle, time, CW);
}

SunPosition sunPosition(float time)
{
  SunPosition sp;
  float D = jDate + time;
  float g = fixAngle(357.529 + 0.98560028 * D);
  float q = fixAngle(280.459 + 0.98564736 * D);
  float L = fixAngle(q + 1.915 * dsin(g) + 0.02 * dsin(2.0 * g));

  float R = 1.00014 - 0.01671 * dcos(g) - 0.00014 * dcos(2.0 * g);
  float e = 23.439 - 0.00000036 * D;

  float RA = darctan2(dcos(e) * dsin(L), dcos(L)) / 15.0;
  sp.equation = q / 15.0 - fixHour(RA);
  sp.declination = darcsin(dsin(e) * dsin(L));
  return sp;
}

// convert Gregorian date to Julian day
// Ref: Astronomical Algorithms by Jean Meeus
float julian(int year, byte month, float day)
{
  if (month <= 2)
  {
    year--;
    month += 12;
  }
  float A = floor(year / 100.0);
  float B = 2.0 - A + floor(A / 4.0);
  
  float JD = floor(365.25 * (year + 4716.0)) - 2451545.0;
  JD += floor(30.6001 * (month + 1.0)) + day + B - 1524.5;
  return JD;
}


//--------- Compute Prayer Times ---------

float riseSetAngle()
{
  //long earthRad = 6371009; // in meters
  //float angle = darccos(earthRad / (earthRad + elv));
  float angle = 0.0347 * sqrt((float) elv); // an approxiamation
  return 0.833 + angle;
}

Times adjustHighLats(Times times)
{
  float nightTime = timeDiff(times.sunset, times.sunrise);
  
  times.imsak   = adjustHLTime(times.imsak,   times.sunrise, nightTime, CCW);
  times.fajr    = adjustHLTime(times.fajr,    times.sunrise, nightTime, CCW);
  times.isha    = adjustHLTime(times.isha,    times.sunset,  nightTime, CW);
  times.maghrib = adjustHLTime(times.maghrib, times.sunset,  nightTime, CW);
  
  return times;
}

float adjustHLTime(float time, float base, float night, byte direction)
{
  float portion = night * MIDNIGHT_PORTION;
  float tmDiff = (direction) ? timeDiff(base, time) : timeDiff(time, base);
  if (tmDiff > portion)
  {
    time = base + (direction ? portion : -portion);
  }
  return time;
}

Times dayPortion(Times times)
{
  times.imsak    /= 24.0;
  times.fajr     /= 24.0;
  times.sunrise  /= 24.0;
  times.dhuhr    /= 24.0;
  times.asr      /= 24.0;
  times.sunset   /= 24.0;
  times.maghrib  /= 24.0;
  times.isha     /= 24.0;
  times.midnight /= 24.0;
  return times;
}


//--------- Misc Methods ---------

float timeDiff(float time1, float time2)
{
  return fixHour(time2 - time1);
}


//--------- Degree-Based Math Methods ---------

float dtr(float d)
{
  return d * PI / 180.0;
}

float rtd(float r)
{
  return r * 180.0 / PI;
}

float dsin(float d)
{
  return sin(dtr(d));
}

float dcos(float d)
{
  return cos(dtr(d));
}

float dtan(float d)
{
  return tan(dtr(d));
}

float darcsin(float x)
{
  return rtd(asin(x));
}

float darccos(float x)
{
  return rtd(acos(x));
}

float darctan(float x)
{
  return rtd(atan(x));
}

float darccot(float x)
{
  return rtd(atan(1.0 / x));
}

float darctan2(float y, float x)
{
  return rtd(atan2(y, x));
}

float fixAngle(float a)
{
  return fix(a, 360);
}

float fixHour(float a)
{
  return fix(a, 24);
}

float fix(float a, int b)
{
  a = a - b * floor(a / b);
  return (a < 0.0) ? a + b : a;
}

