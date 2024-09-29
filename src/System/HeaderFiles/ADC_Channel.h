//==============================================================================
//
//  ADC_Channel.h
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
//! This module measures the voltage of the battery pack
//
//==============================================================================
//  REVISION HISTORY
//==============================================================================
//  Revision: 1.0  
//      
//
//==============================================================================

#ifndef __ADC_CHANNEL_H
#define __ADC_CHANNEL_H

#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "SysDef.h"

//==============================================================================
//  GLOBAL CONSTANTS, TYPEDEFS AND MACROS
//==============================================================================

#define ADC_DebugPrint(format,...)      SYSTEM_PRINT(ENABLE_ADC_LOG, format, ##__VA_ARGS__)
#define ADC_DebugFlush()                SYSTEM_FLUSH(ENABLE_ADC_LOG)

//==============================================================================
//  GLOBAL DATA STRUCTURES DEFINITION
//==============================================================================

//==============================================================================
//
//  int8_t InitializeADC(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/08/18
//
//!  This function initializes the ADC Channel
//
//==============================================================================
void InitializeADC(void);

//==============================================================================
//
//  int8_t ReadBatteryLevel(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/08/20
//
//!  This function Reads battery level from the ADC Channel
//
//==============================================================================
int8_t UpdateBatteryVoltageInfo(void);
#endif/*__ADC_CHANNEL_H*/