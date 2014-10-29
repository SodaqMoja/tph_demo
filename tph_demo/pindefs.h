#ifndef PINDEFS_H
#define PINDEFS_H

#define SODAQ_VARIANT_V2        2
#define SODAQ_VARIANT_MOJA      3
#define SODAQ_VARIANT_MBILI     4

#if defined(__AVR_ATmega32U4__)
#define SODAQ_VARIANT   SODAQ_VARIANT_V2
#elif defined(__AVR_ATmega328P__)
#define SODAQ_VARIANT   SODAQ_VARIANT_MOJA
#elif defined(__AVR_ATmega1284P__)
#define SODAQ_VARIANT   SODAQ_VARIANT_MBILI
#else
#error "Unknown SODAQ_VARIANT"
#endif

#if SODAQ_VARIANT == SODAQ_VARIANT_V2
//#########   pin definitions SODAQ V2   ########

#error "SODAQ V2 not supported anymore"

#elif SODAQ_VARIANT == SODAQ_VARIANT_MOJA
//#########   pin definitions SODAQ Moja   ########

#define BATVOLTPIN      A7
#define BATVOLT_R1      10              // in fact 10M
#define BATVOLT_R2      2               // in fact 2M

// Only needed if DIAG is enabled
#define DIAGPORT_RX     4       // PD4 Note. No interrupt. Cannot be used for input
#define DIAGPORT_TX     5       // PD5

#define BEEPORT         Serial
// Skip .begin and assume Serial will be initialized anyway
#define BEEPORT_BEGIN(baud)

#elif SODAQ_VARIANT == SODAQ_VARIANT_MBILI
//#########   pin definitions SODAQ Mbili   ########

#define BATVOLTPIN      A6
#define BATVOLT_R1      47              // in fact 4.7M
#define BATVOLT_R2      100             // in fact 10M

// Only needed if DIAG is enabled
#define DIAGPORT_RX     4       // PD4 Note. No interrupt. Cannot be used for input
#define DIAGPORT_TX     5       // PD5

#define BEEPORT         Serial1
#define BEEPORT_BEGIN(baud)     BEEPORT.begin(baud)

#endif

#define FATAL_LED       GROVEPWR
#define FATAL_LED_OFF   GROVEPWR_OFF
#define FATAL_LED_ON    GROVEPWR_OFF

#endif // PINDEFS_H
