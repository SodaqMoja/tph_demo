/*
 * \brief Data record and data storage (flash)
 */

#include <Arduino.h>

#include "SQ_Diag.h"
#include "DataRecord.h"

// TODO Do we need this one?
//DataRecord_t rec;

/*
 * \brief Is the record a valid record
 */
bool DataRecord_t::isValidRecord() const
{
  if ((int32_t)ts == -1) {
    return false;
  }
  return true;
}

void DataRecord_t::addToString(String & str) const
{
  str.reserve(80);
  str += ts;
  str += ',';
  str += temp_sht21;
  str += ',';
  str += hum_sht21;
  str += ',';
  str += temp_bmp85;
  str += ',';
  str += pres_bmp85;
  str += ',';
  str += batteryVoltage;
}

void DataRecord_t::addHeaderToString(String & str)
{
  str.reserve(100);
  PGM_P ptr = PSTR("ts"
      ",temp_sht21,hum_sht21,temp_bmp85,pres_bmp85"
      ",batteryVoltage"
      );
  char c;
  do {
    c = pgm_read_byte_near(ptr++);
    if (c) {
      str += c;
    }
  } while (c != '\0');
}

#ifdef ENABLE_DIAG
//################ print all values of the record ################
void DataRecord_t::printRecordHeader() const
{
  String str;
  addHeaderToString(str);
  DIAGPRINTLN(str.c_str());
}

void DataRecord_t::printRecord() const
{
  String str;
  addToString(str);
  DIAGPRINTLN(str.c_str());
}

#endif
