// Platform setup ------------------------------------------------------------------------------------
#pragma once

// This is for fast processors with hardware FP
#define HAL_FAST_PROCESSOR

// Lower limit (fastest) step rate in uS for this platform (in SQW mode)
#define HAL_MAXRATE_LOWER_LIMIT 16

// Width of step pulse
#define HAL_PULSE_WIDTH 2500

// New symbols for the Serial ports so they can be remapped if necessary -----------------------------

// SerialA is manidatory
#define SERIAL_A Serial
// SerialB is optional
#if SERIAL_B_BAUD_DEFAULT != OFF
  #define SERIAL_B Serial2
#endif
// SerialC is optional
#if PINMAP == InsteinESP1
  #ifndef SERIAL_C_BAUD_DEFAULT
    #define SERIAL_C_BAUD_DEFAULT 9600
  #endif
  #define SERIAL_C Serial1
  #define SERIAL_C_RX 21
  #define SERIAL_C_TX 22
  #define HAL_SERIAL_C_ENABLED
#elif defined(SERIAL_C_BAUD_DEFAULT)
  #if SERIAL_C_BAUD_DEFAULT != OFF
    #define SERIAL_C Serial1
    #define HAL_SERIAL_C_ENABLED
  #endif
#endif
#include "../lib/serial/IP_ESP32.h"

// New symbol for the default I2C port ---------------------------------------------------------------
#include <Wire.h>
#define HAL_Wire Wire
#define HAL_WIRE_CLOCK 100000

// Non-volatile storage ----------------------------------------------------------------------------
#ifdef NV_DEFAULT
  #define E2END 4095
  #undef  NV_ENDURANCE
  #define NV_ENDURANCE NVE_LOW
  #include "../lib/nv/NV_ESP.h"
#endif

//--------------------------------------------------------------------------------------------------
// General purpose initialize for HAL
#define HAL_INIT() { nv.init(E2END + 1, false, 5000, false); }

//--------------------------------------------------------------------------------------------------
// Internal MCU temperature (in degrees C)
// Correction for ESP32's internal temperture sensor
#define INTERNAL_TEMP_CORRECTION 0
#define HAL_TEMP() ( temperatureRead() + INTERNAL_TEMP_CORRECTION )

//---------------------------------------------------------------------------------------------------
// Misc. includes to support this processor's operation
//#include "../lib/analog/AN_ESP32.h"
