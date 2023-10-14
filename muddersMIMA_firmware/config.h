//Copyright 2022-2023(c) John Sullivan


//config.h - compile time configuration parameters

#ifndef config_h
	#define config_h
	#include "muddersMIMA.h"  //For Arduino IDE compatibility

	#define FW_VERSION "0.1.8"
    #define BUILD_DATE "2023SEP28"

	#define CPU_MAP_ATMEGA328p
    
	//#define HW_PROTO
    #define HW_REVA_REVB

	#define INVERT_JOYSTICK_DIRECTION //comment to mirror joystick assist and regen directions

  #define CLUTCH_DELAY   250 // Delay before commanding assist after clutch depressed. 
  #define CLUTCH_RAMP    250 // Duration of ramp-up after CLUTCH_DELAY.

  const unsigned long DERATE_UNDER_RPM = 1900; //Max assist/regen limited under this RPM. Adjust as needed. 
  const unsigned long DERATE_PERCENT = 90; //Percentage of max power. Default 80%, or derated by 20%.


#endif