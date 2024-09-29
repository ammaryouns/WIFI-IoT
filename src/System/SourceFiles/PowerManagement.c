//==============================================================================
//
//  SPI_Communication.c
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
#include "Board.h"
#include <ti/sysbios/knl/Swi.h>
#include "SysDef.h"
#include "GPS.h"
#include "SysTask.h"
#include "TimerTask.h"
#include "PowerManagement.h"
#include "EventQ.h"
#include <ti/drivers/GPIO.h>
#include <ti/devices/cc32xx/inc/hw_types.h>
#include <ti/devices/cc32xx/driverlib/rom_map.h>
#include <ti/devices/cc32xx/driverlib/cpu.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC32XX.h>
#include "WifiTask.h"
#include <ti/devices/cc32xx/driverlib/prcm.h>
//==============================================================================
//	CONSTANTS, TYPEDEFS AND MACROS 
//==============================================================================
#define SLOW_CLK_FREQ           (32*1024)
//==============================================================================
//	LOCAL DATA STRUCTURE DEFINITION
//==============================================================================

//==============================================================================
//	LOCAL DATA DECLARATIONS
//==============================================================================

//==============================================================================
//	LOCAL AND GLOBAL FUNCTIONS IMPLEMENTATION
//==============================================================================

void startStartupSeq(void);
void startShutdownSeq(void);
void ShutDownGNSSystem(void);
void RestartGNSSystem(void);

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
void PowerManagementStateMachine(uint16_t EventType)
{
    
    switch(EventType)
    {
    case POWER_WAKE_UP_EVENT:
        PowerManagementDebugPrint("***************************************************\r\n");
        PowerManagementDebugPrint("*************INSTRUMENT WAKE-UP EVENT**************\r\n"); 
        PowerManagementDebugPrint("***************************************************\r\n");
        PowerManagementDebugFlush();
        startStartupSeq();
        break;
        
    case POWER_DEEP_SLEEP_EVENT:
        PowerManagementDebugPrint("***************************************************\r\n");
        PowerManagementDebugPrint("************INSTRUMENT DEEP-SLEEP EVENT************\r\n"); 
        PowerManagementDebugPrint("***************************************************\r\n");
        PowerManagementDebugFlush();
        startShutdownSeq();
        break;
        
    case POWER_REBOOT_EVENT:
      PowerManagementDebugPrint("*************************************************\r\n");
      PowerManagementDebugPrint("************INSTRUMENT REBOOT EVENT**************\r\n"); 
      PowerManagementDebugPrint("*************************************************\r\n");
      PowerManagementDebugFlush();
      //Wait for 1 seconds before going to reboot the device
      Task_sleep(1000);
      
      //Control should never leave,if it doesnt reboot trap it and let WDT reset the system.
      while(true)
      {
        SystemReboot();
      }
        break;
        
    default:
        break;
  }
}

void startStartupSeq(void)
{
    //WakeUp Communication Interface
    PostAutoReconnectMsgToWifiTask();
    //Start GPS Interface
    RestartGNSSystem();
    //Start PERIODIC Event Disptcher
    startPeriodicEventDIspatcher();
    
}
void startShutdownSeq(void)
{
    //Stop Periodic Event Dispatcher
    stopPeriodicEventDIspatcher();
    //Commit Setting To File System
    CommitParametersToFlash();
    CommitGPSSettingFileToWIFIFlash();
    //ShutDown Communication Interface
    PostDisableMsgToWifiTask();
    //Shudown GPS Module
    ShutDownGNSSystem();
    RemoteUnit.InstrumentState = 0x01<<SHUTDOWN;
    RemoteUnit.InternalInstrumentState = 0x01<<SHUTDOWN;
}

void ShutDownGNSSystem(void)
{
    SendEventToGPSStateMachine(GPS_EVENT_SLEEP_EVENT);

}
void RestartGNSSystem(void)
{
    SendEventToGPSStateMachine(GPS_EVENT_REINIT_EVENT);
}

void SystemReboot(void)
{ 
  PRCMCC3200MCUInit();
  CPUcpsid();
  
  Swi_disable();
  Task_disable();
  
  PRCMHibernateIntervalSet( 1 );
  
  PRCMHibernateWakeUpGPIOSelect( PRCM_HIB_GPIO17,PRCM_HIB_RISE_EDGE|PRCM_HIB_HIGH_LEVEL );
  PRCMHibernateWakeupSourceEnable( PRCM_HIB_SLOW_CLK_CTR|PRCM_HIB_GPIO17 );
  
  PRCMHibernateEnter();
}