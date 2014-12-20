/*
 * Copyright (c) 2014 Kees Bakker.  All rights reserved.
 *
 * This file is part of TPH Ddemo.
 *
 * TPH Demo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or(at your option) any later version.
 *
 * TPH Demo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with TPH Demo.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

//################ defines ################

#define ADC_AREF        3.3     // DEFAULT see wiring_analog.c
#define ADC_AREF_MV     3300    // In milliVolt
#define MIN_BATTERY_LEVEL1_GPRSBEE      3.5    // Below this level do not use GPRSbee
#define MIN_BATTERY_LEVEL2_GPRSBEE      3.9    // Below this level try GPRSbee on and check

//############ time service ################
#define TIMEURL "http://time.sodaq.net/?"

//################ includes ################
#include <avr/sleep.h>
#include <avr/wdt.h>

// Sketchbook libraries
#include <Arduino.h>
#include <Sodaq.h>
#include <Sodaq_BMP085.h>
#include <Sodaq_SHT2x.h>
#include <Sodaq_DS3231.h>
#include <GPRSbee.h>
#include <Wire.h>
#include <Sodaq_dataflash.h>
#include <Sodaq_SoftSerial.h>
#include <RTCTimer.h>
#include <Sodaq_PcInt.h>

#include "SQ_Diag.h"
#include "SQ_Utils.h"
#include "SQ_StartupCommands.h"
#include "SQ_UploadPages.h"
#include "SQ_DataflashUtils.h"

// Our own libraries
#include "version.h"
#include "pindefs.h"
#include "MyWatchdog.h"
#include "DataRecord.h"
#include "Config.h"

//################ variables ################

Sodaq_BMP085 bmp;

RTCTimer timer;

bool doneRetryUpload;

uint8_t oldMCUSR;

//######### forward declare #############

void systemSleep();

void createRecord(uint32_t now);
void doUploadData(uint32_t now);
void doSystemCheck(uint32_t now);
void showDateTime(uint32_t now);
void startLongTerm(uint32_t now);
void flashLed(uint32_t now);
void doCheckGPRSoff(uint32_t now);

uint32_t getNow();
void syncRTCwithServer(uint32_t now);

float getRealBatteryVoltage();
uint16_t getBatteryMilliVolt();
bool checkBatteryOnGPRSbee();

void addDeviceId(String & str);

void showStartupBanner(Stream & stream, uint8_t mcusr);
void showDeviceId(Stream & stream);
bool checkConfig();

//######### correct the things we hate from Arduino #############

#undef abs

//################ get the initial value of MCUSR ################
#if 1
/*
 * Save R2 which contains the MCUSR value from directly after system reset.
 *
 * Optiboot has a feature to pass the old MCUSR to the application
 * program via R2. The big question is: how do you get to the value in R2.
 *
 * The following code will located in .init5 (between .init4 and .init6).
 * .init2 resets the Status register (disables interrupt), and initializes the stack pointer
 * .init4 does "copy data" and "clear bss"
 * .init6 does __do_global_ctors
 *
 * Notice that it makes no sense to place the code in .init3 because the variable
 * initialMCUSR will be cleared in .init4
 *
 */
extern "C" {
void saveR2() __attribute__((section(".init5"), __noreturn__, __naked__));

uint8_t optibootMCUSR;
void saveR2(void)
{
  __asm__ __volatile__ ("sts  %[var1],r2" : [var1] "=m" (optibootMCUSR) :);
  // "__naked__" suppresses the "ret"?
}

}
#endif

//################ setup ################
void setup(void)
{
  oldMCUSR = MCUSR | optibootMCUSR;
  // Clear all reset causes, just in case we have no boot loader
  MCUSR = 0;
  wdt_disable();

  pinMode(GROVEPWR, OUTPUT);
  digitalWrite(GROVEPWR, GROVEPWR_OFF);

  Serial.begin(9600);
  BEEPORT_BEGIN(9600);
  gprsbee.init(BEEPORT, BEECTS, BEEDTR);
#if SODAQ_VARIANT == SODAQ_VARIANT_MBILI
  gprsbee.setPowerSwitchedOnOff(true);          // Use the D23 switched power available on Mbili
#endif

#if ENABLE_DIAG
  diagport.begin(9600);
#if ENABLE_GPRSBEE_DIAG
  gprsbee.setDiag(diagport);
#endif
#endif
  // Make sure the GPRSbee is switched off
  doCheckGPRSoff(0);

  showStartupBanner(Serial, oldMCUSR);
#if ENABLE_DIAG
  if (static_cast<Stream*>(&Serial) != static_cast<Stream*>(&diagport)) {
    showStartupBanner(diagport, oldMCUSR);
  }
#endif
  showFreeRAM();

  Wire.begin();
  bmp.begin();
  rtc.begin();

  // Initialize the Data Flash chip. Only then we can display the
  // device ID.
  dflash.init(MISO, MOSI, SCK, SS);
  showDeviceId(Serial);
#if ENABLE_DIAG
  if (static_cast<Stream*>(&Serial) != static_cast<Stream*>(&diagport)) {
    showDeviceId(diagport);
  }
#endif

  parms.read();

  do {
    startupCommands(Serial);
  } while (!checkConfig());
  parms.dump();
  parms.commit();

  // We need the current timestamp as a pseudo random value
  uint32_t ts = getNow();
  initializeDataflash(ts);

  // Instruct the RTCTimer how to get the "now" timestamp.
  timer.setNowCallback(getNow);

  // parms A - sampling rate of "short term"
  // parms B - sampling rate of "long term"
  // parms C - after how long starts the "long term"
  // parms D - syncRTC interval
  uint16_t untilLongTerm = parms.getL();
  timer.every(parms.getAs(), createRecord, untilLongTerm / parms.getAs());
  timer.every(parms.getUs(), doUploadData, untilLongTerm / parms.getUs());

  // Execute a function that will switch intervals for sampling
  // and uploading.
  // After the short term, create a new event to sample at 5 minute intervals
  timer.every(untilLongTerm + 1, startLongTerm, 1);

  if (parms.getS() > 10 * 60) {
    // Do an early RTC sync, so that we don't have to wait 24 hours
    timer.every(2L * 60, syncRTCwithServer, 1);
  }
  timer.every(parms.getS(), syncRTCwithServer);

  // Flash a LED to has a visual indication that the system is still alive
  timer.every(3, flashLed);

  // Do an extra check if GPRS is switched off after 5 seconds.
  timer.every(5, doCheckGPRSoff, 2);

  setupWatchdog();

  interrupts();

  doSystemCheck(getNow());
}

//################ loop ################
void loop(void)
{
  if (hz_flag) {
    wdt_reset();
    WDTCSR |= _BV(WDIE);

    hz_flag = false;

    timer.update();
  }

  diagport.flush();
  systemSleep();
}

/*
 * Return the current timestamp
 *
 * This is a wrapper function to be used for the "now" callback
 * of the RTCTimer.
 */
uint32_t getNow()
{
  return rtc.now().getEpoch();
}

void startLongTerm(uint32_t now)
{
  //DIAGPRINTLN(F("startLongTerm"));
  // The previous schedule of creating records and doing uploads has stopped.

  // Start a new sequences with much longer interval.
  timer.every(parms.getAl(), createRecord);
  timer.every(parms.getUl(), doUploadData);
}

/*
 * Create a record and write it to dataflash
 *
 * This function reads all sensors and counters and battery voltage
 */
void createRecord(uint32_t now)
{
  DataRecord_t rec;

  /*
   * The battery is connected to the middle of a 10M and 2M resistor
   * that are between Vcc and GND.
   * So actual battery voltage is:
   *    <adc value> * 1023. / AREF * (10+2) / 2
   */

  rec.ts = now;
  rec.temp_sht21 = SHT2x.GetTemperature() * 10;
  rec.hum_sht21 = SHT2x.GetHumidity() * 10;
  rec.temp_bmp85 = bmp.readTemperature() * 10;
  rec.pres_bmp85 = bmp.readPressure() / 10;         // divide by 100 and multiply by 10
  rec.batteryVoltage = getBatteryMilliVolt();

  {
    static int counter;
    if (counter == 0) {
      rec.printRecordHeader();
    }
    if (++counter >= 10) {
      counter = 0;
    }
  }
  rec.printRecord();

  addCurPageRecord(&rec, now);
}

/*
 * Make the FTP file name
 *
 * The file name has the following syntax:
 *   <station name> '.' <timestamp> ".csv"
 */
void makeUploadFilename(String & filename, uint32_t start)
{
  if (filename.reserve(40)) {
    filename += parms.getStationName();
    filename += '.';
    addDeviceId(filename);
    filename += '.';
    filename += start;
    filename += ".csv";
  } else {
    // Not enough space
  }
}

/*
 * Upload all available data from dataflash to the server
 */
void doUploadData(uint32_t now)
{
  // Prepare the filename prefix
  String filename;
  uint32_t start;
  bool status;

  start = getNow();
  makeUploadFilename(filename, start);
  showFreeRAM();
  if (filename.length() == 0) {
    // Fatal error
    return;
  }

  // TODO Verify that this is a good thing.
  // Should we close the current page?
  // If we do it can be send to the server in a moment.
  newCurPage(start);

  status = uploadPages(filename.c_str(), parms);
  DIAGPRINT(F("time upload: ")); DIAGPRINTLN((int)(getNow() - start));
  if (!status) {
    if (!doneRetryUpload) {
      // Repeat again in a few minutes, but only once.
      timer.every(120, doUploadData, 1);
      doneRetryUpload = true;
    }
  } else {
    doneRetryUpload = false;
  }

  // Do an extra check if GPRS is switched off after 5 seconds.
  timer.every(5, doCheckGPRSoff, 2);
}

/*
 * Check the status of the system
 *
 * Display (via diagnostic) all sorts of status information
 */
void doSystemCheck(uint32_t now)
{
  showDateTime(now);

  showBattVolt(getBatteryMilliVolt());
  parms.dump();
  //showFreeRAM();
  //memoryDump();
}

/*
 * Show date and time
 */
void showDateTime(uint32_t now)
{
  String strDt;
  strDt.reserve(30);
  DateTime dt(rtc.makeDateTime(now));
  dt.addToString(strDt);
  DIAGPRINTLN(strDt);
}

//######### watchdog and system sleep #############
void systemSleep()
{
  ADCSRA &= ~_BV(ADEN);         // ADC disabled

  /*
  * Possible sleep modes are (see sleep.h):
  #define SLEEP_MODE_IDLE         (0)
  #define SLEEP_MODE_ADC          _BV(SM0)
  #define SLEEP_MODE_PWR_DOWN     _BV(SM1)
  #define SLEEP_MODE_PWR_SAVE     (_BV(SM0) | _BV(SM1))
  #define SLEEP_MODE_STANDBY      (_BV(SM1) | _BV(SM2))
  #define SLEEP_MODE_EXT_STANDBY  (_BV(SM0) | _BV(SM1) | _BV(SM2))
  */
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  /*
   * This code is from the documentation in avr/sleep.h
   */
  cli();
  // Only go to sleep if there was no watchdog interrupt.
  if (!hz_flag)
  {
    sleep_enable();
    sei();
    sleep_cpu();
    sleep_disable();
  }
  sei();

  ADCSRA |= _BV(ADEN);          // ADC enabled
}

//################ RTC ################
/*
 * Synchronize RTC with a time server
 */
void syncRTCwithServer(uint32_t now)
{
  //DIAGPRINT(F("syncRTCwithServer ")); DIAGPRINTLN(now);

  String url;
  url.reserve(22 + 1 + 8);      // http://time.sodaq.net/?efee734f
  addPstrToString(url, PSTR(TIMEURL));
  addDeviceId(url);
  if (oldMCUSR) {
    // Only send MCUSR the first time.
    url += '&';
    url += oldMCUSR;
    oldMCUSR = 0;
  }
  char buffer[20];
  if (gprsbee.doHTTPGET(parms.getAPN(), url, buffer, sizeof(buffer))) {
    //DIAGPRINT(F("HTTP GET: ")); DIAGPRINTLN(buffer);
    uint32_t newTs;
    if (getUValue(buffer, &newTs)) {
      // Tweak the timestamp a little because doHTTPGET took a few seconds
      // to close the connection after getting the time from the server
      newTs += 3;
      uint32_t oldTs = getNow();
      int32_t diffTs = labs(newTs - oldTs);
      if (diffTs > 30) {
        DIAGPRINT(F("Updating RTC, old=")); DIAGPRINT(oldTs);
        DIAGPRINT(F(" new=")); DIAGPRINTLN(newTs);
        timer.adjust(oldTs, newTs);
        rtc.setEpoch(newTs);
      }
    }
  }
  //doSystemCheck();
  //DIAGPRINTLN(F("syncRTCwithServer - end"));

  // Do an extra check if GPRS is switched off after 5 seconds.
  timer.every(5, doCheckGPRSoff, 2);
}

/*
 * \brief Read the battery voltage and compute actual voltage
 */
float getRealBatteryVoltage()
{
  /*
   * This pin is connected to the middle of a 4M7 and 10M resistor
   * that are between Vcc (battery) and GND.
   * So actual battery voltage is:
   *    <adc value> * 1023. / AREF * (47+100) / 100
   */
  uint16_t batteryVoltage = analogRead(BATVOLTPIN);
  return (ADC_AREF / 1023.) * (BATVOLT_R1 + BATVOLT_R2) / BATVOLT_R2 * batteryVoltage;
}

/*
 * \brief Read the battery voltage and compute actual voltage in milliVolt
 *
 * See getRealBatteryVoltage for more details
 */
uint16_t getBatteryMilliVolt()
{
  uint32_t batteryVoltage = analogRead(BATVOLTPIN);
  return batteryVoltage * (BATVOLT_R1 + BATVOLT_R2) * ADC_AREF_MV / 1023 / BATVOLT_R2;
}

/*
 * Check if the GPRSbee is really off
 */
void doCheckGPRSoff(uint32_t now)
{
  // Maybe we shouldn't care about WDT. If it does not switch
  // off properly it could be a good thing to reset the system.
  // TODO We need to verify this somehow.
  gprsbee.off();
  for (uint8_t i = 0; i < 10; ++i) {
    delay(50);
    if (gprsbee.off()) {
      break;
    }
  }
}

void showDeviceId(Stream & stream)
{
  String devId;
  addDeviceId(devId);
  stream.print(F("device ID ")); stream.println(devId);
}

/*
 * Show the startup banner
 *
 * First of all it prints the name of the application and its
 * version number.
 * Next it prints the reset value of MCUSR.  It is the ORed
 * version from optiboot and the plain MCUSR in case there is no boot
 * loader.
 */
void showStartupBanner(Stream & stream, uint8_t mcusr)
{
  stream.println();
  stream.println(F("TPH demo " VERSION));
  stream.print(F(" MCUSR=")); stream.println(mcusr, HEX);
}

/*
 * Add the unique device id, taken from dataflash security register
 */
void addDeviceId(String &str)
{
  uint8_t buffer[128];
  dflash.readSecurityReg(buffer, 128);
  /* An example of the second 64 bytes of the security register
0D0414071A2D1F2600011204FFFFE8FF
303032533636313216140AFFFFFFFFFF
3E3E3E3E3E3C3E3E3E3C3E3C3C3E3E3C
FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
   */
  //dumpBuffer(buffer + 64, 64);
  uint16_t crc1 = crc16_ccitt(buffer + 64, 16);
  uint16_t crc2 = crc16_ccitt(buffer + 64 + 16, 16);

  add04x(str, crc1);
  add04x(str, crc2);
}

/*
 * Check if all required config parameters are filled in
 */
bool checkConfig()
{
  if (!parms.checkConfig()) {
    return false;
  }
  return true;
}

void flashLed(uint32_t now)
{
  digitalWrite(GROVEPWR, GROVEPWR_ON);
  delay(10);
  digitalWrite(GROVEPWR, GROVEPWR_OFF);
}
