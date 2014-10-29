/*
 * This module is responsible for the configuration of the SODAQ
 * device.
 */

// These are default values that can be changed and stored in EEPROM
#define PARM_As         (5L)                 //   5 secs
#define PARM_Al         (5L * 60)            //   5 mins
#define PARM_Us         (5L * 60)            //   5 mins
#define PARM_Ul         (60L * 60)           //   1 hour
#define PARM_L          (120L * 60)          // 120 mins
#define PARM_S          (24L * 60 * 60)      // 24 hours

#include <stdint.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "SQ_Command.h"
#include "SQ_Diag.h"
#include "SQ_Utils.h"

#include "Config.h"

static void * eepromAddr() { return (void *)0x40; }
const char magic[] PROGMEM = "SODAQ";
const char stationName_Default[] PROGMEM = "tph_demo";

ConfigParms      parms;
static bool needCommit;

/*
 * Read all of the config parameters from the EEPROM
 *
 * There are some sanity checks. If they fail then
 * it will call reset() instead.
 */
void ConfigParms::read()
{
  const size_t crc_size = sizeof(uint16_t);
  const size_t magic_len = sizeof(magic);
  size_t size = magic_len + sizeof(*this) + crc_size;
  uint8_t buffer[size];

  //DIAGPRINT(F("magic_len=")); DIAGPRINTLN(magic_len);
  //DIAGPRINT(F("size=")); DIAGPRINTLN(size);
  //DIAGPRINT(F("crc_size=")); DIAGPRINTLN(crc_size);
  eeprom_read_block((void *)buffer, eepromAddr(), size);
  //dumpBuffer(buffer, size);

  uint16_t crc = *(uint16_t *)(buffer + size - crc_size);
  uint16_t crc1 = crc16_ccitt(buffer, size - crc_size);

  if (strncmp_P((const char *)buffer, magic, magic_len) != 0) {
    //DIAGPRINTLN(F("ConfigParms::read - magic wrong"));
    goto do_reset;
  }
  //DIAGPRINTLN(F("ConfigParms::read - magic OK"));
  //DIAGPRINT(F("crc=")); DIAGPRINTLN(crc);
  //DIAGPRINT(F("crc1=")); DIAGPRINTLN(crc1);
  if (crc != crc1) {
    goto do_reset;
  }
  memcpy((uint8_t *)this, buffer + magic_len, sizeof(*this));
  return;

do_reset:
  reset();
}

void ConfigParms::reset()
{
  //DIAGPRINTLN(F("ConfigParms::reset"));
  memset(this, 0, sizeof(*this));
  _as = PARM_As;
  _us = PARM_Us;
  _al = PARM_Al;
  _ul = PARM_Ul;
  _l = PARM_L;
  _s = PARM_S;

  strncpy_P(_stationName, stationName_Default, sizeof(_stationName) - 1);

  needCommit = true;
}

/*
 * Write the configuration parameters to EEPROM
 */
void ConfigParms::commit(bool forced)
{
  if (_stationName[0] == '\0') {
    // It was probably not initialized.
    return;
  }
  if (!forced && !needCommit) {
    return;
  }

  //DIAGPRINTLN(F("ConfigParms::commit"));
  // Fill in the magic and CRC, and write to EEPROM
  const size_t crc_size = sizeof(uint16_t);
  const size_t magic_len = sizeof(magic);
  size_t size = magic_len + sizeof(*this) + crc_size;
  uint8_t buffer[size];

  strncpy_P((char *)buffer, magic, magic_len);
  memcpy(buffer + magic_len, (uint8_t *)this, sizeof(*this));
  uint16_t crc = crc16_ccitt(buffer, size - crc_size);
  *(uint16_t *)(buffer + size - crc_size) = crc;
  //dumpBuffer(buffer, size);

  eeprom_write_block((const void *)buffer, eepromAddr(), size);
  needCommit = false;
}

static const Command args[] = {
    {"short term sample", "as=",   Command::set_uint16, Command::show_uint16,  &parms._as},
    {"short term upload", "us=",   Command::set_uint16, Command::show_uint16,  &parms._us},
    {"long term sample",  "al=",   Command::set_uint16, Command::show_uint16,  &parms._al},
    {"long term upload",  "ul=",   Command::set_uint16, Command::show_uint16,  &parms._ul},
    {"long term begin",   "l=",    Command::set_uint16, Command::show_uint16,  &parms._l},
    {"sync RTC",          "rtc=",  Command::set_uint32, Command::show_uint32,  &parms._s},
    {"station name",      "nm=",   Command::set_string, Command::show_string,  parms._stationName, sizeof(parms._stationName)},
    {"APN",               "apn=",  Command::set_string, Command::show_string,  parms._apn, sizeof(parms._apn)},
    {"FTP server",        "srv=",  Command::set_string, Command::show_string,  parms._ftpsrv, sizeof(parms._ftpsrv)},
    {"FTP user",          "user=", Command::set_string, Command::show_string,  parms._ftpusr, sizeof(parms._ftpusr)},
    {"FTP password",      "pw=",   Command::set_string, Command::show_string,  parms._ftppw, sizeof(parms._ftppw)},
};

void ConfigParms::showSettings(Stream & stream)
{
  stream.println();
  stream.println(F("Settings:"));
  for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); ++i) {
    const Command *a = &args[i];
    if (a->show_func) {
      a->show_func(a, stream);
    }
  }
}

/*
 * Execute a command from the commandline
 *
 * Return true if it was a valid command
 */
bool ConfigParms::execCommand(const char * line)
{
  bool done = Command::execCommand(args, sizeof(args) / sizeof(args[0]), line);
  if (done) {
    needCommit = true;
  }
  return done;
}

/*
 * Check if all required config parameters are filled in
 */
bool ConfigParms::checkConfig()
{
  // The APN cannot be empty
  if (_apn[0] == 0xFF || _apn[0] == '\0') {
    return false;
  }
  // The APN cannot be empty
  if (_ftpsrv[0] == 0xFF || _ftpsrv[0] == '\0') {
    return false;
  }
  return true;
}

#if ENABLE_DIAG
void ConfigParms::dump()
{
  ConfigParms::showSettings(diagport);
}
#endif
