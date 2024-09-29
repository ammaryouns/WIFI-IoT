//==============================================================================
//
//  ADC_Channel.c
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
//  INCLUDES
//==============================================================================
#include <stdint.h>
#include <stddef.h>
#include <ti/drivers/ADC.h>
#include <ti/drivers/adc/ADCCC32XX.h>
#include "Board.h"
#include "SysDef.h"
#include "SysTask.h"
#include "ADC_Channel.h"
#include <stdio.h>
#include <ti/sysbios/knl/Task.h>
#include "SPI_Comm.h"

//==============================================================================
//	CONSTANTS, TYPEDEFS AND MACROS 
//==============================================================================
#define ADC_SAMPLES_PER_CHANNEL                 (21)
#define DELAY_BETWEEN_SAMPLES                   (10)

#define VOLTAGE_DEVIDER_SCALE_FACTOR            (4)
#define SCALE_FACTOR_FOR_MILI_VOLT              (1000)
#define ADC_VREF_VOLTAGE_IN_MICRO_VOLTS         (670000)

//==============================================================================
//	LOCAL DATA STRUCTURE DEFINITION
//==============================================================================

//==============================================================================
//	GLOBAL DATA DECLARATIONS
//==============================================================================

ADC_Params   adcParams;

//==============================================================================
//	LOCAL DATA DECLARATIONS
//==============================================================================

//==============================================================================
//	LOCAL FUNCTION PROTOTYPES
//==============================================================================
int16_t ReadBatteryVoltage(uint32_t *ptrToAvgBatteryVoltage,uint8_t Channel,uint16_t numOfReadings,uint16_t msDelay);
//==============================================================================
//	LOCAL AND GLOBAL FUNCTIONS IMPLEMENTATION
//==============================================================================

//==============================================================================
//	LOCAL FUNCTIONS IMPLEMENTATION
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

void InitializeADC(void)
{
  ADC_init();  
  ADC_Params_init(&adcParams);
}

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


int8_t UpdateBatteryVoltageInfo(void)
{
    uint8_t retVal = 0;
    
    uint32_t avgHostMonVoltageIn_uV = 0;
    uint32_t avgBatteryMonVoltageIn_uV = 0;
    
    if(0 == ReadBatteryVoltage(&avgHostMonVoltageIn_uV,Board_ADC0,ADC_SAMPLES_PER_CHANNEL,DELAY_BETWEEN_SAMPLES) )
    {
        avgHostMonVoltageIn_uV *= VOLTAGE_DEVIDER_SCALE_FACTOR;
        avgHostMonVoltageIn_uV += ADC_VREF_VOLTAGE_IN_MICRO_VOLTS;
        
        InstrumentInfo.vHostMon = avgHostMonVoltageIn_uV / SCALE_FACTOR_FOR_MILI_VOLT;

    }
    if(0 == ReadBatteryVoltage(&avgBatteryMonVoltageIn_uV,Board_ADC1,ADC_SAMPLES_PER_CHANNEL,DELAY_BETWEEN_SAMPLES))
    {
        avgBatteryMonVoltageIn_uV *= VOLTAGE_DEVIDER_SCALE_FACTOR;
        avgBatteryMonVoltageIn_uV += ADC_VREF_VOLTAGE_IN_MICRO_VOLTS;
        
        InstrumentInfo.vBatMon = avgBatteryMonVoltageIn_uV / SCALE_FACTOR_FOR_MILI_VOLT;
    }
    
    return retVal;
}

int16_t ReadBatteryVoltage(uint32_t *ptrToAvgBatteryVoltage,uint8_t Channel,uint16_t numOfReadings,uint16_t msDelay)
{
    ADC_Handle   adcHandle = NULL;
    uint16_t ADCReadingAvg = 0;
    uint16_t ADCReading = 0;
    int16_t retVal = -1;
    
    adcHandle = ADC_open(Channel, &adcParams);
    if(adcHandle)
    {
        for(uint8_t index = 0;index< numOfReadings; ++index)
        {
            retVal = ADC_convert(adcHandle,&ADCReading);
            if(ADC_STATUS_SUCCESS == retVal)
            {
                ADCReadingAvg += (ADCReading/numOfReadings);
            }
            else
            {
                ADC_DebugPrint("ADC Conversion Failure\r\n");
                ADC_DebugFlush();
                break;
            }
            Task_sleep(msDelay);
        }
        if(ADC_STATUS_SUCCESS == retVal)
        {
            *ptrToAvgBatteryVoltage = ADC_convertRawToMicroVolts(adcHandle,ADCReadingAvg);
        }
    }
    else
    {
        ADC_DebugPrint("ADC handler Failure\r\n");
        ADC_DebugFlush();
    }
    
    ADC_close(adcHandle);
    return retVal;
}