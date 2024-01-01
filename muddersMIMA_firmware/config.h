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

	//'choose' your transmission type. This helps LiControl interpret different signals.
		//#define TRANSMISSION_IS_MT
		//#define TRANSMISSION_IS_CVT
	// If you do not choose, certain features will be inaccessible.

#endif