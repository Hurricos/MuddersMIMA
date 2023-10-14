//Copyright 2022-2023(c) John Sullivan


//debug stuff

#include "muddersMIMA.h"

/////////////////////////////////////////////////////////////////////////////////////////////

void mcm_setMAMODE1_state (uint8_t newState  ) { gpio_setMCM_MAMODE1_percent(newState);   } //JTS2doNow: Redundant... remove
void mcm_setMAMODE2_state (uint8_t newState  ) { gpio_setMCM_MAMODE2_bool   (newState);   }
void mcm_setCMDPWR_percent(uint8_t newPercent) { gpio_setMCM_CMDPWR_percent (newPercent); }

/////////////////////////////////////////////////////////////////////////////////////////////

uint8_t getMAMODE2State(uint8_t mamode1_state) {
    switch(mamode1_state) {
        case MAMODE1_STATE_IS_ASSIST:
            return MAMODE2_STATE_IS_ASSIST;
        case MAMODE1_STATE_IS_REGEN:
            return MAMODE2_STATE_IS_REGEN_STANDBY;
        case MAMODE1_STATE_IS_IDLE:
        default:
            return MAMODE2_STATE_IS_REGEN_STANDBY;
    }
}

void applySignal(uint8_t state, uint16_t powerPercent)
{
    switch(state)
    {
        case MAMODE1_STATE_IS_ASSIST:
            mcm_setMAMODE2_state(MAMODE2_STATE_IS_ASSIST);
            gpio_setMCM_CMDPWR_percent(powerPercent);
            break;
        case MAMODE1_STATE_IS_REGEN:
            mcm_setMAMODE2_state(MAMODE2_STATE_IS_REGEN_STANDBY);
            mcm_setCMDPWR_percent(powerPercent);
            break;
        case MAMODE1_STATE_IS_IDLE:
        default:
            mcm_setMAMODE2_state(MAMODE2_STATE_IS_REGEN_STANDBY);
            mcm_setCMDPWR_percent(50);
            break;
    }
}

void mcm_setAllSignals(uint8_t newState, uint16_t CMDPWR_percent, uint8_t requestType)
{
    mcm_setMAMODE1_state(newState);
    mcm_setMAMODE2_state(getMAMODE2State(newState));

    if(CMDPWR_percent > 90) { CMDPWR_percent = 90; }
    if(CMDPWR_percent < 10) { CMDPWR_percent = 10; }

    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - clutchStartTime;
    
if(requestType == REQUEST_CLUTCH)
    {
        if(elapsedTime < CLUTCH_DELAY)
        {
            // Wait for the clutch delay before starting the ramp.
            return;
        }
        else if(elapsedTime < (CLUTCH_DELAY + CLUTCH_RAMP))
        {
            uint16_t scaledPercent = CMDPWR_percent * (elapsedTime - CLUTCH_DELAY) / CLUTCH_RAMP;
            applySignal(newState, scaledPercent);
        }
        else
        {
            applySignal(newState, CMDPWR_percent);
            clutchStartTime = currentTime; // Reset the start time after ramp is over
        }
    }
    else if(requestType == REQUEST_OEM)
    {
        applySignal(newState, CMDPWR_percent);
    }
}

/*
void mcm_setAllSignals(uint8_t newState, uint16_t CMDPWR_percent, uint8_t requestType)
{
    mcm_setMAMODE1_state(newState);

    if(CMDPWR_percent > 90) { CMDPWR_percent = 90; }
	  if(CMDPWR_percent < 10) { CMDPWR_percent = 10; }
    
    if(requestType == REQUEST_OEM)
    {
        if(newState == MAMODE1_STATE_IS_ASSIST)
        {
            mcm_setMAMODE2_state(MAMODE2_STATE_IS_ASSIST);
            gpio_setMCM_CMDPWR_percent(CMDPWR_percent);
        }
        else if(newState == MAMODE1_STATE_IS_REGEN)
        {
            mcm_setMAMODE2_state(MAMODE2_STATE_IS_REGEN_STANDBY);
            mcm_setCMDPWR_percent(CMDPWR_percent);
        }
        else if(newState == MAMODE1_STATE_IS_IDLE)
        {
            mcm_setMAMODE2_state(MAMODE2_STATE_IS_REGEN_STANDBY);
            mcm_setCMDPWR_percent(50);           
        }
    }
    else if(requestType == REQUEST_CLUTCH)
    {
        static unsigned long startTime = 0;
        unsigned long currentTime = millis();

        if(currentTime - startTime < CLUTCH_DELAY)
        {
            return;  
        }
        else if(currentTime - startTime < CLUTCH_DELAY + CLUTCH_RAMP)
        {
            uint16_t scaledPercent = CMDPWR_percent * (currentTime - startTime - CLUTCH_DELAY) / CLUTCH_RAMP;
            
            if(newState == MAMODE1_STATE_IS_ASSIST)
            {
                mcm_setMAMODE2_state(MAMODE2_STATE_IS_ASSIST);
                gpio_setMCM_CMDPWR_percent(scaledPercent);
            }
            else if(newState == MAMODE1_STATE_IS_REGEN)
            {
                mcm_setMAMODE2_state(MAMODE2_STATE_IS_REGEN_STANDBY);
                mcm_setCMDPWR_percent(CMDPWR_percent);
            }
            else if(newState == MAMODE1_STATE_IS_IDLE)
            {
                mcm_setMAMODE2_state(MAMODE2_STATE_IS_REGEN_STANDBY);
                mcm_setCMDPWR_percent(50);           
            }
        }
        else
        {
            if(newState == MAMODE1_STATE_IS_ASSIST)
            {
                mcm_setMAMODE2_state(MAMODE2_STATE_IS_ASSIST);
                gpio_setMCM_CMDPWR_percent(CMDPWR_percent);
            }
            // ... [rest of the conditions for other states]
            startTime = currentTime;  
        }
    }
}
/*

/*
void mcm_setAllSignals(uint8_t newState, uint16_t CMDPWR_percent, uint8_t requestType)
{
	mcm_setMAMODE1_state(newState);

	if(CMDPWR_percent > 90) { CMDPWR_percent = 90; }
	if(CMDPWR_percent < 10) { CMDPWR_percent = 10; }

	if     (newState == MAMODE1_STATE_IS_ASSIST) { mcm_setMAMODE2_state(MAMODE2_STATE_IS_ASSIST);        mcm_setCMDPWR_percent(CMDPWR_percent); }
	else if(newState == MAMODE1_STATE_IS_REGEN)  { mcm_setMAMODE2_state(MAMODE2_STATE_IS_REGEN_STANDBY); mcm_setCMDPWR_percent(CMDPWR_percent); }
	else if(newState == MAMODE1_STATE_IS_IDLE)   { mcm_setMAMODE2_state(MAMODE2_STATE_IS_REGEN_STANDBY); mcm_setCMDPWR_percent(50);             }
	else
	{
		; //add additional states if needed
	}

}
*/

/////////////////////////////////////////////////////////////////////////////////////////////

void mcm_passUnmodifiedSignals_fromECM(void)
{
	mcm_setMAMODE1_state  (ecm_getMAMODE1_state()  );
	mcm_setMAMODE2_state  (ecm_getMAMODE2_state()  );
	mcm_setCMDPWR_percent (ecm_getCMDPWR_percent() );
}