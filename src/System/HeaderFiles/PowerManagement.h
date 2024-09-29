//==============================================================================
//
//  PowerManagement.h
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
//  Project:       
//
//  Author:        
//
//  Date:          
//
//  Revision:      1.0
//
//==============================================================================
//  FILE DESCRIPTION
//==============================================================================
//
//! \file
//! This module takes care of all the communication/messages between smart battery
//! board and VPRO
//
//==============================================================================
//  REVISION HISTORY
//==============================================================================
//  Revision: 1.0  
//      
//
//==============================================================================

#ifndef __POWER_MANAGEMENT_H
#define __POWER_MANAGEMENT_H

#include "SysDef.h"

//==============================================================================
//  GLOBAL CONSTANTS, TYPEDEFS AND MACROS
//==============================================================================

#define REBOOT_TIME_OUT                            14

#define PowerManagementDebugPrint(format,...)      SYSTEM_PRINT(ENABLE_POWER_MANAGEMENT_LOG, format, ##__VA_ARGS__)
#define PowerManagementDebugFlush()                SYSTEM_FLUSH(ENABLE_POWER_MANAGEMENT_LOG)
//==============================================================================
//  GLOBAL DATA STRUCTURES DEFINITION
//==============================================================================

//==============================================================================
//  GLOBAL DATA
//==============================================================================

//==============================================================================
//  EXTERNAL OR GLOBAL FUNCTIONS
//==============================================================================

//==============================================================================
//
//  void PowerManagementStateMachine(uint16_t EventType)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/30
//
//!  This is the state machine handling the power management activity
//
//==============================================================================
void PowerManagementStateMachine(uint16_t EventType);

//==============================================================================
//
//  uint8_t InitializePowerSavingSettings(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/30
//
//!  This function handels the SWI posted in CS GPIO iterrupt 
//
//==============================================================================
uint8_t InitializePowerSavingSettings(void);

//==============================================================================
//
//  void PowerManagementFunction(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/30
//
//!  This functions 
//
//==============================================================================
void PowerManagementFunction(void);

void ShutDownGNSSystem(void);

void SystemReboot(void);
#endif/*__POWER_MANAGEMENT_H*/