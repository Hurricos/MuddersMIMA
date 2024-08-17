//Copyright 2022-2023(c) John Sullivan

//Send MCM control data, depending on IMA mode

#include "muddersMIMA.h"

uint8_t joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
uint16_t previousOutputCMDPWR_permille = 500;
int16_t previousOverRegenAdjustment_permille = 0;
bool useStoredJoystickValue = NO; //JTS2doLater: I'm not convinced this is required
bool useSuggestedPedalAssist = YES;
uint32_t time_latestBrakeLightsOn_ms = 0;
uint32_t time_latestBrakeLightsOff_ms = 0;
bool holdingAutostopRegen = NO;

const uint16_t map_TPS[11] = {40, 60, 70, 80, 100, 120, 160, 200, 300, 400, 500};
const uint16_t map_VSS[10] = {1, 10, 15, 20, 30, 40, 50, 60};
const uint16_t map_suggestedCMDPWR_TPS_VSS[88] =
    {
/*     TPS
       BRL,BRI, 7%, 8%,10%,13%,16%,20%,30%,40%,50%, */
        50, 50, 50, 50, 55, 58, 62, 64, 66, 74, 77, //VSS < 0
        42, 45, 50, 50, 59, 63, 65, 67, 71, 77, 83, //VSS < 10
        40, 40, 47, 47, 59, 63, 65, 67, 71, 77, 83, //VSS < 15
        37, 38, 45, 45, 63, 66, 69, 73, 76, 82, 87, //VSS < 20
        25, 32, 40, 40, 64, 68, 70, 75, 79, 87, 90, //VSS < 30
        10, 23, 35, 35, 64, 71, 72, 77, 82, 90, 90, //VSS < 40
        10, 15, 28, 28, 67, 74, 75, 80, 87, 90, 90, //VSS < 50
        10, 10, 25, 25, 68, 75, 79, 87, 90, 90, 90 //VSS < 60
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
void mode_INWORK_manualRegen_autoAssist(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_OEM);

	if(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE, JOYSTICK_NEUTRAL_NOM_PERCENT); } //ignore regen request
	else /* (ECM not requesting regen) */                { mcm_passUnmodifiedSignals_fromECM(); } //pass all other signals through
}

/////////////////////////////////////////////////////////////////////////////////////////////

//LiControl completely ignores ECM signals (including autostop, autostart, prestart, etc)
void mode_manualAssistRegen_ignoreECM(void)
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

void mode_manualAssistRegen_withAutoStartStop(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_AUTOMATIC);

	if( ((ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN ) && !(holdingAutostopRegen)) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_IDLE  ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_ASSIST)  )
	{
        holdingAutostopRegen = NO;
		//ECM is sending assist, idle, or regen signal...
		//but we're in blended manual mode, so combine its signal cleverly with the joystick (either previously stored or value right now)

		uint16_t joystick_percent = adc_readJoystick_percent();
        uint16_t throttle_permille = adc_getECM_TPS_permille(); throttle_permille = max(throttle_permille, 70);

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
		} else if(millis() - time_latestBrakeLightsOn_ms > 200)
			time_latestBrakeLightsOff_ms = millis();

        //MIK2doNow: Why does brake detection not work consistently? Is it because we're pulsing the brake data somehow?
        //A: Yes, *maybe* intentionally; see https://github.com/doppelhub/MuddersMIMA/issues/10
        if(millis() - time_latestBrakeLightsOn_ms < 200) {
            throttle_permille = max(throttle_permille - 20, 60);
            if (millis() - time_latestBrakeLightsOff_ms > 500) throttle_permille = max(throttle_permille - 5, 20);
            if (millis() - time_latestBrakeLightsOff_ms > 1000) throttle_permille = max(throttle_permille - 5, 20);
            if (millis() - time_latestBrakeLightsOff_ms > 1500) throttle_permille = max(throttle_permille - 5, 20);
            if (millis() - time_latestBrakeLightsOff_ms > 2000) throttle_permille = max(throttle_permille - 5, 20);
            if (millis() - time_latestBrakeLightsOff_ms > 2500) throttle_permille = max(throttle_permille - 5, 20);
        }

        if (throttle_permille < 20) throttle_permille = 20;

        uint8_t pedalSuggestedCMDPWR = \
          evaluate_2d_map(map_suggestedCMDPWR_TPS_VSS,
                          map_TPS, 11, throttle_permille,
                          map_VSS, 8, engineSignals_getLatestVehicleMPH());

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

		// MIK2doNow: Track control "correctly"
		// - Track which system is controlling CMDPWR
		// - Consider what "neutral position" is based on this
		// - Expose which subsystem is controlling CMDPWR so other methods can use it to change their behavior

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

        //MIK2reviewNow: Handle over-regen throttling dynamically
        // Note: I suspect this clause not to work as it will hold the RPM on regen at above 1000 *specifically*
        // there needs to be a "detente" where if more regen is actually needed then the RPMs are allowed to go lower?
        if ( previousOutputCMDPWR_permille < 450 && engineSignals_getLatestVehicleMPH() > 5 && engineSignals_getLatestRPM() > FAS_MAX_STOPPED_RPM) {
          if ( engineSignals_getLatestRPM() < 1000 ) previousOverRegenAdjustment_permille += 1;
          if ( engineSignals_getLatestRPM() < 1100 ) previousOverRegenAdjustment_permille += 1;
          if ( engineSignals_getLatestRPM() < 1200 ) previousOverRegenAdjustment_permille += 1;
        }
        previousOverRegenAdjustment_permille -= 1;

        if ( previousOverRegenAdjustment_permille < 0 ) previousOverRegenAdjustment_permille = 0;

        if (joystick_percent + previousOverRegenAdjustment_permille / 10 < 50) { joystick_percent += previousOverRegenAdjustment_permille / 10; }
        else if (joystick_percent + previousOverRegenAdjustment_permille / 10 >= 50 && joystick_percent < 50) { joystick_percent = 50; }

        //MIK2reviewNow: While brake and throttle are released, slow down change in IMA power even more
        uint8_t changeby_increment = (gpio_getBrakePosition_bool() == BRAKE_LIGHTS_ARE_OFF && adc_getECM_TPS_permille() < 90) ? 3 : 6;
        //This is simulating something nicer -- tracking the rationale for the current change in control.
        if		(joystick_percent * 10 > previousOutputCMDPWR_permille) { previousOutputCMDPWR_permille = previousOutputCMDPWR_permille + changeby_increment * (((joystick_percent * 10 - previousOutputCMDPWR_permille)/100)+1); joystick_percent = previousOutputCMDPWR_permille / 10; }
        else if (joystick_percent * 10 < previousOutputCMDPWR_permille) { previousOutputCMDPWR_permille = previousOutputCMDPWR_permille - changeby_increment * (((previousOutputCMDPWR_permille - joystick_percent * 10)/100)+1); joystick_percent = previousOutputCMDPWR_permille / 10; }

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
	else if ((ecm_getMAMODE1_state() == MAMODE1_STATE_IS_AUTOSTOP) || (ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN && holdingAutostopRegen))
	{
        holdingAutostopRegen = YES;
		//pass these signals through unmodified (so autostop works properly)
		mcm_passUnmodifiedSignals_fromECM();

		//clear stored assist/idle/regen setpoint
		joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		useStoredJoystickValue = NO;
	} else //ECM is sending start or undefined signal
	{
        holdingAutostopRegen = NO;
		//pass these signals through unmodified (so autostop works properly)
		mcm_passUnmodifiedSignals_fromECM();

		//clear stored assist/idle/regen setpoint
		joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		useStoredJoystickValue = NO;
	}

	//JTS2doLater: New feature: When the key is on and the engine is off, pushing momentary button starts engine.
}

/////////////////////////////////////////////////////////////////////////////////////////////

//GOAL: All OEM signals are passed through unmodified, except:
//CMDPWR assist
	//LiControl uses strongest assist request (user or ECM), except that;
	//pressing the momentary button stores the joystick position (technically the value is stored on button release)
	//after pressing the momentary button, all ECM assist requests are ignored until the user either brakes or (temporarily) changes modes   
	//manual joystick assist requests are allowed even after pushing momentary button; stored value resumes once joystick is neutral again
//CMDPWR regen
	//LiControl ignores ECM regen requests, unless user is braking
	//when braking and joystick is neutral, LiControl uses ECM regen request
	//manual joystick regen request always overrides ECM regen request
//MAMODE1 prestart
	//modified to always enable DCDC when key is on
void mode_INWORK_PHEV_mudder(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_MONITOR_ONLY); //JTS2doLater: if possible, add strong regen brake lights

	if( (ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_IDLE  ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_ASSIST)  )
	{
		//ECM is sending assist, idle, or regen signal

		uint8_t joystick_percent = adc_readJoystick_percent();
		uint8_t ECM_CMDPWR_percent = ecm_getCMDPWR_percent();

		if (ECM_CMDPWR_percent > joystick_percent) { joystick_percent = ECM_CMDPWR_percent; } //choose strongest assist request (user or ECM)

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
			useStoredJoystickValue = NO;
			joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;	
		} 

		//Use ECM regen request when user is braking AND joystick is neutral
		if ((joystick_percent > JOYSTICK_NEUTRAL_MIN_PERCENT)     && //joystick is neutral
			(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT)     && //joystick is neutral
			(gpio_getBrakePosition_bool() == BRAKE_LIGHTS_ARE_ON)  ) //user is braking
		{
			//while braking, replace neutral joystick position with ECM regen request
			joystick_percent = ecm_getCMDPWR_percent();
		}

		//use stored joystick value if conditions are right
		if( (useStoredJoystickValue == YES                  ) && //user previously pushed button
			(joystick_percent > JOYSTICK_NEUTRAL_MIN_PERCENT) && //joystick is neutral
			(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT)  ) //joystick is neutral
		{
			//replace actual joystick position with previously stored value
			joystick_percent = joystick_percent_stored;
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
		previousOutputCMDPWR_permille = 500;
		useStoredJoystickValue = NO;
	}

	if     (toggleState == TOGGLE_POSITION0) { MODE0_BEHAVIOR(); } //see #define substitutions in config.h
	else if(toggleState == TOGGLE_POSITION1) { MODE1_BEHAVIOR(); }
	else if(toggleState == TOGGLE_POSITION2) { MODE2_BEHAVIOR(); }
	else /* hidden 'mode3' (unsupported) */  { MODE0_BEHAVIOR(); }

	toggleState_previous = toggleState;
}
