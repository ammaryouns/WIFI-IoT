//==============================================================================
//
//  ManufacturingTestCodes.c
//
//  Copyright (C) 2018 by Industrial Scientific.
//
//  This document and all information contained within are confidential and
//  proprietary property of Industrial Scientific Corporation. All rights
//  reserved. It is not to be reproduced or reused without the prior approval
//  of Industrial Scientific Corporation.
//
//==============================================================================
//  FILE INFORMATION
//==============================================================================
//
//  Source:        
//
//  Project:       Cornell
//
//  Author:        Ammar Younis & Tayyab Tahir
//
//  Date:          02-01-2019
//
//  Revision:      1.0
//
//==============================================================================
//  FILE DESCRIPTION
//==============================================================================
//
//! \file
//! This module takes care of manufacturing test codes for Cornell
//
//==============================================================================
//  REVISION HISTORY
//==============================================================================
//  Revision: 1.0  
//      
//
//==============================================================================
//  INCLUDES
//==============================================================================


#ifdef MANFACTURING_TEST_CODE
 


#include "SPI_Comm.h"
#include "SysDef.h"
#include "SysTask.h"
#include "ManufacturingTestCodes.h"
#include <xdc/std.h>
#include <ti/drivers/PWM.h>
//==============================================================================
//	CONSTANTS, TYPEDEFS AND MACROS 
//==============================================================================


//==============================================================================
//	LOCAL DATA STRUCTURE DEFINITION
//==============================================================================
#define NETWORK_ID_BIT_FIELD_INDEX                              0
//==============================================================================
//	GLOBAL DATA DECLARATIONS
//==============================================================================
ManufacturingTestBitField_t ManufacturingTestsState;
//==============================================================================
//	LOCAL FUNCTION PROTOTYPES
//==============================================================================
static void PWMTest( BOOLEAN startTest );
static void RTCTest(void);


//==============================================================================
//	LOCAL AND GLOBAL FUNCTIONS IMPLEMENTATION
//==============================================================================

//==============================================================================
//
//   void MfgTestRun (uint16_t EventType)
//
//   Author:   Tayyab Tahir
//   Date:     2019/01/02      
//
//!  This function handels the messages posted by systask to manufacturing test machine 
//
//==============================================================================
void MfgTestRun (uint16_t EventType)
{
  switch(EventType)
  {
  case STOP_TEST:
    MfgTestLogDebugPrint("MFG test Event : STOP_EVENT\n");
    MfgTestLogDebugFlush();
    PWMTest(false);
    //Reset the state of test to deafult.
    InstrumentInfo.RTCTest = TEST_IN_PROGRESS;
    break;
        
  case START_GPS_TEST:
    MfgTestLogDebugPrint("MFG test Event : START_GPS_TEST\n");
    MfgTestLogDebugFlush();
    break;
  
  case START_40MHZ_CRYSTAL_TEST:
    MfgTestLogDebugPrint("MFG test Event : START_40MHZ_CRYSTAL_TEST\n");
    MfgTestLogDebugFlush();
    PWMTest(true);
    break;
    
  case START_RTC_TEST:
    MfgTestLogDebugPrint("MFG test Event : START_RTC_TEST\n");
    MfgTestLogDebugFlush();
    RTCTest();
    break;
    
  case WATCH_DOG_TEST:
    MfgTestLogDebugPrint("MFG test Event : WATCH_DOG_TEST\n");
    MfgTestLogDebugFlush();
    InstrumentInfo.isWDTTestRunnig = TRUE;
    break;
    
  default:
    MfgTestLogDebugPrint("MFG test Event : DEFAULT\n");
    MfgTestLogDebugFlush();
    break;
  }
}


//==============================================================================
//
//   void PWMTest (BOOLEAN startTest)
//
//   Author:   Ammar Younis
//   Date:     2019/01/02      
//
//!  This function handels PWM test
//
//==============================================================================
static void PWMTest(BOOLEAN startTest)
{
    uint32_t   pwmPeriod = PWM_PERIOD;
    uint32_t   duty = (PWM_DUTY_FRACTION_MAX >> 1);
    int32_t isPWMEnable = -1;
    
    static PWM_Handle pwm = NULL;
    
    if(startTest)
    {
        
        PWM_Params params;
        
        /* Call driver init functions. */
        PWM_init();
        
        PWM_Params_init(&params);
        params.dutyUnits = PWM_DUTY_FRACTION;
        params.dutyValue = 0;
        params.periodUnits = PWM_PERIOD_HZ;
        params.periodValue = pwmPeriod;
        
        if(NULL == pwm)
        {
            pwm = PWM_open(Board_PWM0, &params);
            if(NULL != pwm)
            {
                PWM_start(pwm);
                PWM_setDuty(pwm, duty);
            }
            else
            {
                MfgTestLogDebugPrint("PWM Handle Failed");
                MfgTestLogDebugFlush();
            }

        }
    }
    else
    {
        if(NULL != pwm)
        {
            PWM_stop(pwm);
            PWM_close(pwm);
            pwm = NULL;
        }
    }
}


//==============================================================================
//
//   void RTCTest(void)
//
//   Author:   Ammar Younis
//   Date:     2019/01/02      
//
//!  This function handels RTC test
//
//==============================================================================
static void RTCTest(void)
{
    uint32_t RTCStampBeforeTest = 0u;
    uint32_t RTCStampAfterTest = 0u;
    
    uint16_t msOffset = 0u;
    
    PRCMRTCGet(&RTCStampBeforeTest, &msOffset);
    
    Task_sleep(RTC_TEST_TIME_IN_SEC * OS_TICKS_PER_SECOND);
    
    PRCMRTCGet(&RTCStampAfterTest, &msOffset);
    
    if(RTC_TEST_TIME_IN_SEC  <= (RTCStampAfterTest - RTCStampBeforeTest) )
    {
        InstrumentInfo.RTCTest = TEST_PASSED;
    }
    else
    {
        InstrumentInfo.RTCTest = TEST_FAILED;
    }
}

#endif /*MANFACTURING_TEST_CODE*/ 