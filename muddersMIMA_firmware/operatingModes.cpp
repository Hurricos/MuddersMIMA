//Copyright 2022-2023(c) John Sullivan

//Send MCM control data, depending on IMA mode

#include "muddersMIMA.h"

uint8_t joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
bool useStoredJoystickValue = NO; //JTS2doLater: I'm not convinced this is required
bool useSuggestedPedalAssist = YES;
uint32_t time_latestBrakeLightsOn_ms = 0;

const uint8_t map_TPS[10] = {6, 7, 8, 10, 12, 16, 20, 30, 40, 50};
const uint8_t map_VSS[9] = {6, 10, 20, 30, 40, 50, 60, 70, 80};
const uint8_t map_suggestedCMDPWR_TPS_VSS[90] =
    {
/*     TPS
       BRK, 7%, 8%,10%,13%,16%,20%,30%,40%,50%, */
        50, 50, 50, 55, 60, 63, 65, 69, 73, 80, //VSS < 6
        40, 50, 50, 58, 60, 63, 65, 69, 73, 80, //VSS < 10
        35, 43, 43, 60, 62, 64, 69, 71, 75, 85, //VSS < 20
        27, 40, 40, 63, 64, 66, 71, 75, 85, 88, //VSS < 30
        20, 35, 35, 64, 67, 70, 75, 80, 88, 90, //VSS < 40
        15, 28, 28, 66, 70, 73, 78, 85, 90, 90, //VSS < 50
        10, 25, 25, 67, 73, 77, 85, 88, 90, 90, //VSS < 60
        10, 23, 23, 68, 75, 81, 88, 90, 90, 90, //VSS < 70
        10, 22, 22, 72, 79, 85, 90, 90, 90, 90  //VSS < 80
};

/////////////////////////////////////////////////////////////////////////////////////////////

void mode_OEM(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_OEM); 
	mcm_passUnmodifiedSignals_fromECM();
}

/////////////////////////////////////////////////////////////////////////////////////////////

//PHEV mode
//JTS2doNow: implement manual regen
void mode_manualRegen_autoAssist(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_AUTOMATIC);

	if(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE, JOYSTICK_NEUTRAL_NOM_PERCENT); } //ignore regen request
	else /* (ECM not requesting regen) */                { mcm_passUnmodifiedSignals_fromECM(); } //pass all other signals through
}

/////////////////////////////////////////////////////////////////////////////////////////////

//LiControl completely ignores ECM signals (including autostop, autostart, prestart, etc)
void mode_manualControl_old(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_AUTOMATIC);

	uint16_t joystick_percent = adc_readJoystick_percent();

	if     (joystick_percent < JOYSTICK_MIN_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT); } //signal too low
	else if(joystick_percent < JOYSTICK_NEUTRAL_MIN_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_REGEN,  joystick_percent);             } //manual regen
	else if(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT) {
		if(gpio_getButton_momentary() == BUTTON_PRESSED && engineSignals_getLatestRPM() < FAS_MAX_STOPPED_RPM)
		{
			mcm_setMAMODE1_state(MAMODE1_STATE_IS_START);
			mcm_setCMDPWR_percent(50);
		}
		else { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   joystick_percent); }
	} //standby
	else if(joystick_percent < JOYSTICK_MAX_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_ASSIST, joystick_percent);             } //manual assist
	else                                                     { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT); } //signal too high
}

/////////////////////////////////////////////////////////////////////////////////////////////

void mode_manualControl_new(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_AUTOMATIC);

	if( (ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_IDLE  ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_ASSIST)  )
	{
		//ECM is sending assist, idle, or regen signal...
		//but we're in blended manual mode, so combine its signal cleverly with the joystick (either previously stored or value right now)

		uint16_t joystick_percent = adc_readJoystick_percent();
        uint16_t throttle_percent = adc_getECM_TPS_percent();

		if(gpio_getButton_momentary() == BUTTON_PRESSED)
		{
			//store joystick value when button is pressed
			joystick_percent_stored = joystick_percent;
			useStoredJoystickValue = YES;
		}

		//disable stored joystick value if user is braking
		//JTS2doLater: Add clutch disable
		if(gpio_getBrakePosition_bool() == BRAKE_LIGHTS_ARE_ON)
		{
            time_latestBrakeLightsOn_ms = millis();
			useStoredJoystickValue = NO;
			joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		} 

        //MIK2doNow: Why does brake detection not work consistently? Is it because we're pulsing the brake data somehow?
        //A: Yes, *maybe* intentionally; see https://github.com/doppelhub/MuddersMIMA/issues/10
        if(millis() - time_latestBrakeLightsOn_ms < 500)
            throttle_percent = throttle_percent - 2;
        if (throttle_percent < 6) throttle_percent = 6;

        uint8_t pedalSuggestedCMDPWR = \
          evaluate_2d_map(map_suggestedCMDPWR_TPS_VSS,
                          map_TPS, 10, throttle_percent,
                          map_VSS, 9, engineSignals_getLatestVehicleMPH());

		//use stored joystick value if conditions are right
		if( (useStoredJoystickValue == YES                ) && //user previously pushed button
			( ( (joystick_percent > JOYSTICK_NEUTRAL_MIN_PERCENT) && //joystick is neutral, or
			    (joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT)
              ) ||
              ( (joystick_percent < joystick_percent_stored) && //joystick is less than stored value if assist, or
                (joystick_percent > JOYSTICK_NEUTRAL_MAX_PERCENT)
              ) ||
              ( (joystick_percent > joystick_percent_stored) && //joystick is more than stored value if regen
                (joystick_percent < JOYSTICK_NEUTRAL_MIN_PERCENT) ) ) )
		{
			//replace actual joystick position with previously stored value
			joystick_percent = joystick_percent_stored;
		}

		//use the suggested assist if conditions are right
		if( (useSuggestedPedalAssist == YES                ) &&
			( ( (joystick_percent > JOYSTICK_NEUTRAL_MIN_PERCENT) && //joystick is neutral, or
			    (joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT)
              ) ||
              ( (joystick_percent < pedalSuggestedCMDPWR) && //joystick is less than stored value if assist, or
                (joystick_percent > JOYSTICK_NEUTRAL_MAX_PERCENT)
              ) ||
              ( (joystick_percent > pedalSuggestedCMDPWR) && //joystick is more than stored value if regen
                (joystick_percent < JOYSTICK_NEUTRAL_MIN_PERCENT) ) ) )
		{
			//replace actual joystick position with previously stored value
			joystick_percent = pedalSuggestedCMDPWR;
		}
		
		//send assist/idle/regen value to MCM
		if     (joystick_percent < JOYSTICK_MIN_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT); } //signal too low
		else if(joystick_percent < JOYSTICK_NEUTRAL_MIN_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_REGEN,  joystick_percent);             } //manual regen
		else if(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   joystick_percent);             } //standby
		else if(joystick_percent < JOYSTICK_MAX_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_ASSIST, joystick_percent);             } //manual assist
		else                                                     { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT); } //signal too high

	}
	else if(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_PRESTART)
	{
		//prevent DCDC disable when user regen-stalls car

		//DCDC converter must be disabled when the key first turns on.
		//Otherwise, the DCDC input current prevents the HVDC capacitors from pre-charging through the precharge resistor.
		//This can cause intermittent P1445, particularly after rapidly turning key off and on.

		//Therefore, we need to honor the ECM's PRESTART request for the first few seconds after keyON (so the HVDC bus voltage can charge to the pack voltage).

		//JTS2doNow: if SoC too low (get from LiBCM), pass through unmodified signal (which will disable DCDC) 
		if(millis() < (time_latestKeyOn_ms() + PERIOD_AFTER_KEYON_WHERE_PRESTART_ALLOWED_ms)) { mcm_passUnmodifiedSignals_fromECM(); } //key hasn't been on long enough
		else { mcm_setAllSignals(MAMODE1_STATE_IS_AUTOSTOP, JOYSTICK_NEUTRAL_NOM_PERCENT); } //JTS2doLater: This prevents user from manually assist-starting IMA

		//clear stored assist/idle/regen setpoint
		joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		useStoredJoystickValue = NO;
	}
	else //ECM is sending autostop, start, or undefined signal
	{
		//pass these signals through unmodified (so autostop works properly)
		mcm_passUnmodifiedSignals_fromECM();

		//clear stored assist/idle/regen setpoint
		joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		useStoredJoystickValue = NO;
	}

	//JTS2doLater: New feature: When the key is on and the engine is off, pushing momentary button starts engine.
}

/////////////////////////////////////////////////////////////////////////////////////////////

void operatingModes_handler(void)
{
	uint8_t toggleState = gpio_getButton_toggle();
	static uint8_t toggleState_previous = TOGGLE_UNDEFINED;

	if(toggleState != toggleState_previous)
	{
		//clear previously stored joystick value (from the last time we were in manual mode)
		joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		useStoredJoystickValue = NO;
	}

	if     (toggleState == TOGGLE_POSITION0) { mode_OEM();               }
	else if(toggleState == TOGGLE_POSITION1) { mode_manualControl_new(); }
	else if(toggleState == TOGGLE_POSITION2) { mode_manualControl_old(); }
	else /* this should never happen */      { mode_OEM();               }

	toggleState_previous = toggleState;
}