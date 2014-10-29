/*
 * Config.h
 *
 *  Created on: Mar 27, 2014
 *      Author: Kees Bakker
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdint.h>
#include <Arduino.h>
#include "SQ_Command.h"
#include "SQ_Diag.h"


class ConfigParms
{
public:
  uint16_t      _as;
  uint16_t      _us;
  uint16_t      _al;
  uint16_t      _ul;
  uint16_t      _l;
  uint32_t      _s;
  char          _stationName[20];
  char          _apn[25];               // Is this enough for the APN?
  char          _ftpsrv[25];            // Is this enough for the server name?
  char          _ftpusr[16];            // Is this enough for the server user?
  char          _ftppw[16];             // Is this enough for the server password?
  uint16_t      _ftpport;

public:
  void read();
  void commit(bool forced=false);
  void reset();

  bool execCommand(const char * line);

  uint16_t getAs() const { return _as; }
  uint16_t getUs() const { return _us; }
  uint16_t getAl() const { return _al; }
  uint16_t getUl() const { return _ul; }
  uint16_t getL() const { return _l; }
  uint32_t getS() const { return _s; }
  const char *getStationName() const { return _stationName; }
  const char *getAPN() const { return _apn; }
  const char *getFTPserver() const { return _ftpsrv; }
  const char *getFTPuser() const { return _ftpusr; }
  const char *getFTPpassword() const { return _ftppw; }
  uint16_t getFTPport() const { return _ftpport; }

  static void showSettings(Stream & stream);
  bool checkConfig();

#if ENABLE_DIAG
  void dump();
#else
  void dump() {}
#endif
};

extern ConfigParms      parms;

#endif /* CONFIG_H_ */
