#ifndef SQ_DIAG_H
#define SQ_DIAG_H

#include <stdint.h>

/*
 * \brief Define to switch DIAG on of off
 *
 * Comment or make it a #undef
 */
#define ENABLE_DIAG     1
/*
 * \brief Define send diag messages to "Serial"
 *
 * Comment or make it a #undef to send diag messages to a Software Serial
 */
#define USE_DIAG_SERIAL 1

#if ENABLE_DIAG
#if USE_DIAG_SERIAL
#include <Arduino.h>
#define diagport        Serial
#else
#include <Sodaq_SoftSerial.h>
extern SoftwareSerial diagport;
#endif

#define DIAGPRINT(...)          diagport.print(__VA_ARGS__)
#define DIAGPRINTLN(...)        diagport.println(__VA_ARGS__)
void dumpBuffer(const uint8_t * buf, size_t size);
void memoryDump();
void showAddress(const char *txt, void * addr);
void showFreeRAM();
void showBattVolt(float value);

#else
#define DIAGPRINT(...)
#define DIAGPRINTLN(...)
#define dumpBuffer(buf, size)
#define memoryDump()
#define showAddress(txt, addr)
#define showFreeRAM()
#define showBattVolt(value)
#endif

#endif //  SQ_DIAG_H
