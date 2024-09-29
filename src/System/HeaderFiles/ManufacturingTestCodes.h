#ifdef MANFACTURING_TEST_CODE

//==============================================================================
//
//  ManufacturingTestCodes.h
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

#ifndef __MANUFACTURINGTESTCODES_H
#define __MANUFACTURINGTESTCODES_H

//#include "Event.h"
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "SysDef.h"



#define MfgTestLogDebugPrint(format,...)                          SYSTEM_PRINT(ENABLE_MFG_TEST_DEBUG_LOG, format, ##__VA_ARGS__)
#define MfgTestLogDebugFlush()                                    SYSTEM_FLUSH(ENABLE_MFG_TEST_DEBUG_LOG)

//==============================================================================
//	CONSTANTS, TYPEDEFS AND MACROS 
//==============================================================================

#define PWM_PERIOD               200000u//20000000u
#define RTC_TEST_TIME_IN_SEC     10u


//==============================================================================
//	LOCAL DATA STRUCTURE DEFINITION
//==============================================================================

//==============================================================================
//	GLOBAL DATA DECLARATIONS
//==============================================================================

typedef union ManufacturingTestBitField
{
  uint16_t ManufacturingTestFlagsVector;
}ManufacturingTestBitField_t;

extern ManufacturingTestBitField_t ManufacturingTestsState;

//==============================================================================
//	LOCAL FUNCTION PROTOTYPES
//==============================================================================

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
void MfgTestRun (uint16_t EventType);

#endif/*__MANUFACTURINGTESTCODES_H*/

#endif /*MANFACTURING_TEST_CODE*/ 