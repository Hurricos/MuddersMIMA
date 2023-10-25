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

  //choose how many cells in series
		//#define STACK_IS_48S
		#define STACK_IS_60S
  
  //choose ONE of the following:
		//#define SET_CURRENT_HACK_00 //OEM configuration (no current hack installed inside MCM)
		//#define SET_CURRENT_HACK_20 //+20%
		#define SET_CURRENT_HACK_40 //+40%
		//#define SET_CURRENT_HACK_60 //+60% //Note: LiBCM can only measure between 71 A regen & 147 A assist //higher current values will (safely) rail the ADC

  //Clutch delay logic. All values in ms.
  #define CLUTCH_DELAY   250 // Delay before commanding assist after clutch depressed. 
  #define CLUTCH_RAMP    250 // Duration of ramp-up after CLUTCH_DELAY. Linear ramp from 0% to current joystick value. 

  //Low power mode to avoid false IGBT short-circuit events, jerking, and P1440 codes. Only active if STACK_IS_60S is defined above.
  const unsigned long DERATE_UNDER_RPM = 2000; //Max assist/regen limited under this RPM.
  const unsigned long DERATE_PERCENT = 80; //Percentage of max power.

  //Minimum threshold before regen cuts out on deceleration.
  #define MIN_REGEN_RPM 1300


#endif