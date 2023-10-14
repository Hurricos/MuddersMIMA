//Copyright 2022-2023(c) John Sullivan

//Send MCM control data, depending on IMA mode

#include "muddersMIMA.h"

uint8_t joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
bool useStoredJoystickValue = NO; //JTS2doLater: I'm not convinced this is required
static bool clutchWasPressed = false;
static uint16_t lowPowerLevel = 0;
uint8_t requestType = REQUEST_OEM;  // Default to OEM request type

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

	if(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE, JOYSTICK_NEUTRAL_NOM_PERCENT, REQUEST_OEM); } //ignore regen request
	else /* (ECM not requesting regen) */                { mcm_passUnmodifiedSignals_fromECM(); } //pass all other signals through
}

/////////////////////////////////////////////////////////////////////////////////////////////

//LiControl completely ignores ECM signals (including autostop, autostart, prestart, etc)
void mode_manualControl_old(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_AUTOMATIC);

	uint16_t joystick_percent = adc_readJoystick_percent();

	if     (joystick_percent < JOYSTICK_MIN_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT, REQUEST_OEM); } //signal too low
	else if(joystick_percent < JOYSTICK_NEUTRAL_MIN_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_REGEN,  joystick_percent, REQUEST_OEM);             } //manual regen
	else if(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   joystick_percent, REQUEST_OEM);             } //standby
	else if(joystick_percent < JOYSTICK_MAX_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_ASSIST, joystick_percent, REQUEST_OEM);             } //manual assist
	else                                                     { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT, REQUEST_OEM); } //signal too high
}

/////////////////////////////////////////////////////////////////////////////////////////////

void mode_manualControl_new(void)
{
	brakeLights_setControlMode(BRAKE_LIGHT_AUTOMATIC);

	if( (ecm_getMAMODE1_state() == MAMODE1_STATE_IS_REGEN ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_IDLE  ) ||
		(ecm_getMAMODE1_state() == MAMODE1_STATE_IS_ASSIST)  )
    //ECM is sending assist, idle, or regen signal...
		//but we're in manual mode, so use joystick value instead (either previously stored or value right now)
	  {
      if (gpio_getClutchPosition() == CLUTCH_PEDAL_PRESSED) // Check if the clutch is pressed
      {
          // Clutch is pressed, set the state to MAMODE1_STATE_IS_IDLE
          mcm_setAllSignals(MAMODE1_STATE_IS_IDLE, JOYSTICK_NEUTRAL_NOM_PERCENT, REQUEST_OEM);
          // Set clutchWasPressed to true when the clutch is pressed
          clutchWasPressed = true;
          return;
      }
      else if(gpio_getClutchPosition() == CLUTCH_PEDAL_RELEASED && clutchWasPressed) 
      {
        clutchStartTime = millis();
        requestType = REQUEST_CLUTCH;
        clutchWasPressed = false; // Reset the state so it doesn't keep triggering
        clutchStartTime = millis();
      }

      {
	      uint16_t joystick_percent = adc_readJoystick_percent();
        uint16_t engineRPM = engineSignals_getLatestRPM();

		    if(gpio_getButton_momentary() == BUTTON_PRESSED)
		    {
			    //store joystick value when button is pressed
			    joystick_percent_stored = joystick_percent;
			    useStoredJoystickValue = YES;
		    }

        #ifdef STACK_IS_60S
          #ifdef SET_CURRENT_HACK_40
          // Check if the engine RPM is below threshold
            if (engineRPM < (DERATE_UNDER_RPM - 100)) {
              // Remap joystick_percent to DERATE_PERCENT
              lowPowerLevel = map(lowPowerLevel, 0, joystick_percent, 0, DERATE_PERCENT);
            } else if (engineRPM < DERATE_UNDER_RPM) {
              // Scale DERATE_PERCENT from its value to 100% as engineRPM approaches DERATE_UNDER_RPM
              int scaledPercent = map(engineRPM, DERATE_UNDER_RPM - 100, DERATE_UNDER_RPM, DERATE_PERCENT, 100);
              lowPowerLevel = map(lowPowerLevel, 0, joystick_percent, 0, scaledPercent);
            }
          #endif
        #endif

        #ifdef STACK_IS_48S
        // If STACK_IS_48S is defined, no derating logic is applied.
        #endif

        if(clutchStartTime == 0 || millis() - clutchStartTime > CLUTCH_DELAY + CLUTCH_RAMP) // Reset clutchStartTime once clutch logic is complete
        {
          clutchStartTime = 0;
          requestType = REQUEST_OEM;
        }

		    //disable stored joystick value if user is braking
		    //JTS2doLater: Add clutch disable
		    if(gpio_getBrakePosition_bool() == BRAKE_LIGHTS_ARE_ON)
		    {
			    useStoredJoystickValue = NO;
		  	  joystick_percent_stored = JOYSTICK_NEUTRAL_NOM_PERCENT;
		    } 

		    //use stored joystick value if conditions are right
		    if( (useStoredJoystickValue == YES                ) && //user previously pushed button
		  	  (joystick_percent > JOYSTICK_NEUTRAL_MIN_PERCENT) && //joystick is neutral
		  	  (joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT)  ) //joystick is neutral
		    {
			    //replace actual joystick position with previously stored value
		  	  joystick_percent = joystick_percent_stored;
		    }
		    
        if     (joystick_percent < JOYSTICK_MIN_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT, requestType); } //signal too low
        else if(joystick_percent < JOYSTICK_NEUTRAL_MIN_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_REGEN,  joystick_percent, requestType);             } //manual regen
        else if(joystick_percent < JOYSTICK_NEUTRAL_MAX_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   joystick_percent, requestType);             } //standby
        else if(joystick_percent < JOYSTICK_MAX_ALLOWED_PERCENT) { mcm_setAllSignals(MAMODE1_STATE_IS_ASSIST, joystick_percent, requestType);             } //manual assist
        else                                                     { mcm_setAllSignals(MAMODE1_STATE_IS_IDLE,   JOYSTICK_NEUTRAL_NOM_PERCENT, requestType); } //signal too high
  	  }
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
		  else { mcm_setAllSignals(MAMODE1_STATE_IS_AUTOSTOP, JOYSTICK_NEUTRAL_NOM_PERCENT, REQUEST_OEM); } //JTS2doLater: This prevents user from manually assist-starting IMA

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