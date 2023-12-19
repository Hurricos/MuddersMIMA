//Copyright 2022-2023(c) John Sullivan


//engine related signals

#include "muddersMIMA.h"

volatile uint16_t latestEngineRPM = 0;
uint16_t latestVehicleMPH = 0; // 4550 VSS pulses per mile

/////////////////////////////////////////////////////////////////////////////////////////////

//PCMSK0 is configured so that only D8 causes interrupt (supports D8:D13)
ISR(PCINT0_vect)
{
	static uint32_t tachometerTick_previous_us = 0;

	if(gpio_engineRPM_getPinState() == HIGH)
	{
		uint32_t tachometerTick_now_us = micros();

		uint32_t periodBetweenTicks_us = tachometerTick_now_us - tachometerTick_previous_us;

		latestEngineRPM = ONE_MINUTE_IN_MICROSECONDS / periodBetweenTicks_us;

		tachometerTick_previous_us = tachometerTick_now_us;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////

void engineSignals_handle_vss(void)
{
	static uint32_t vssTick_previous_us = 0;
    static boolean vss_previous_state = LOW;

	if(gpio_VSS_getPinState() == HIGH)
	{
		if (vss_previous_state == LOW)
		{
			vss_previous_state = HIGH;

			uint32_t vssTick_now_us = micros();

			uint32_t periodBetweenTicks_us = vssTick_now_us - vssTick_previous_us;

			latestVehicleMPH = ONE_HOUR_IN_MICROSECONDS / periodBetweenTicks_us / 4550; // 4550 VSS pulses per mile

			vssTick_previous_us = vssTick_now_us;
		}
	} else {
        vss_previous_state = LOW;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////

uint16_t engineSignals_getLatestRPM(void) { return latestEngineRPM; }

/////////////////////////////////////////////////////////////////////////////////////////////

uint16_t engineSignals_getLatestVehicleMPH(void) { return latestVehicleMPH; }

/////////////////////////////////////////////////////////////////////////////////////////////

void engineSignals_begin(void)
{
	PCMSK0 = (1<<PCINT0); //only pin D8 will generate a pin change interrupt on ISR PCINT0_vect (which supports D8:D13)
}

/////////////////////////////////////////////////////////////////////////////////////////////

void engineSignals_handler(void)
{
	engineSignals_handle_vss();
}
