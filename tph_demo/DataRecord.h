#ifndef DATARECORD_H
#define DATARECORD_H

#include <WString.h>

#define HEADER_MAGIC            "SODAQ"
#define DATA_VERSION            7               // Please register at http://sodaq.net/

struct DataRecord_t
{
  uint32_t      ts;             // seconds since epoch (01-jan-1970)

  //SHT21
  int16_t       temp_sht21;
  uint16_t      hum_sht21;

  //BMP085
  int16_t       temp_bmp85;
  uint16_t      pres_bmp85;

  //battery
  uint16_t      batteryVoltage;
  bool isValidRecord() const;
  void addToString(String & str) const;
  static void addHeaderToString(String & str);
#ifdef ENABLE_DIAG
  void printRecordHeader() const;
  void printRecord() const;
#else
  void printRecordHeader() {}
  void printRecord() {}
#endif
};
typedef struct DataRecord_t DataRecord_t;

static inline void clearRecord(DataRecord_t *rec) { memset(rec, 0, sizeof((*rec))); }


#endif // DATARECORD_H
