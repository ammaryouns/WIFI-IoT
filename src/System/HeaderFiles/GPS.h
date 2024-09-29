//==============================================================================
//
//  GPS.h
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
//  Author:     Ammar Younas
//
//  Date:       2018/05/21   
//
//  Revision:   1.0
//
//==============================================================================
//  FILE DESCRIPTION
//==============================================================================
//
//! \file
//! This module takes care 
//
//==============================================================================
//  REVISION HISTORY
//==============================================================================
//  Revision: 1.0  
//      
//
//==============================================================================
#ifndef __GPS_H
#define __GPS_H

//==============================================================================
//  INCLUDES
//==============================================================================
#include <Board.h>
#include <SysDef.h>
#include "SysTask.h"

//==============================================================================
//  GLOBAL CONSTANTS, TYPEDEFS AND MACROS
//==============================================================================
#define GPS_UART_BAUD_RATE                      9600u

typedef void (*PTR_TO_FUNC)(void);

#define GPS_UART_READ_WRITE_TIME_OUT                   5000
#define GPS_CLOCK_DRIFT_TIME_OUT                       240//330
#define GPS_TIME_OUT_MAX                               (~(0))
#define GPS_WAKE_UP_STATE_TIME_OUT                      15
#define GPS_INITIALIZATION_STATE_TIME_OUT               0
#define GPS_DATA_RECV_STATE_TIME_OUT                    15

#ifdef MANFACTURING_TEST_CODE

#define GPS_IDLE_STATE_TIME_OUT                         45                         
#define GPS_IDLE_STATE_TIME_OUT_ON_LOCK                 45

#else /*MANFACTURING_TEST_CODE*/

#define GPS_IDLE_STATE_TIME_OUT                         45//75                         
#define GPS_IDLE_STATE_TIME_OUT_ON_LOCK                 45

#endif /*MANFACTURING_TEST_CODE*/

#define GPSDebugPrint(format,...)      SYSTEM_PRINT(ENABLE_GPS_DEBUG_LOG, format, ##__VA_ARGS__)
#define GPSDebugFlush()                SYSTEM_FLUSH(ENABLE_GPS_DEBUG_LOG)

//==============================================================================
//  GLOBAL DATA STRUCTURES DEFINITION
//==============================================================================

typedef struct
{
  float32_t      longitude;
  float32_t      latitude;
  float32_t      horizantalDilution;
}GPSInfo_t;


typedef struct
{
  GPSInfo_t      gpsInfo;  
  int8_t         longitudeDir;
  int8_t         latitudeDir;
  BOOLEAN        isLockAcquired;
}GPSFullInfo_t;

//==============================================================================
//  EXTERNAL OR GLOBAL FUNCTIONS
//==============================================================================
typedef struct
{
  float32_t utcTime;
  float32_t altitudeAboveSeaLevel;
  float32_t heightOfGeoid;
  int8_t heightOfGeoidUnits;
  int32_t fixQuality;
  int32_t numberOfSatellites;
  int8_t altitudeUnits;
  float32_t bearingToDestination;
  float32_t speedOverGround;
  BOOLEAN isLocationDataAvailable;
  BOOLEAN isSpeedDataAvailable;
  uint32_t CurrentSNR;
  uint32_t lastGPSLockTimeStamp;
}GPSMiscellaneousInfo_t;

typedef struct
{
    DateTimeInfo_t lastUpdatedOn;
    GPSFullInfo_t gpsLocationInfo;
    GPSInfo_t gpsConvertedInfo;
}GPSLastKnownInfo_t;

typedef enum GPS_SATATE
{
  GPS_INITIALIZATION_STATE = 0,
  GPS_DATA_RECV_STATE,
  GPS_POWER_SAVING_STATE,
  GPS_WAKE_UP_STATE,
  GPS_IDLE_STATE,
  
  GPS_LAST_STATE_INDEX
}GPS_SATATE_t;

typedef enum GPS_EVENT
{
    GPS_EVENT_TIME_OUT = 0,
    GPS_EVENT_REC_BUFFER_FULL,
    GPS_EVENT_REINIT_EVENT,
    GPS_EVENT_SLEEP_EVENT
}GPS_EVENT_t;

typedef struct GPSStateStruct
{
    PTR_TO_FUNC const entryFunc;
    PTR_TO_FUNC const exitFunc;
    uint32_t const stateTimeOut;
}GPSStateStruct_t;

extern GPSLastKnownInfo_t GPSLastKnownInfoObj;

void GPSInit(void);
void GPSOpen(void);
void GPSClose(void);
BOOLEAN GPSRead(uint8_t ptrBuffer[]);
BOOLEAN GPSWrite(uint8_t cmd[],uint8_t cmdSize);
void GPSGetConvertedLocationInfo(GPSInfo_t *ptrGPSInfo);
void GPSGetTimeInfo();
void GPSStateMachine (uint16_t EventType);
void GPSSleepCommand(void);
void GpsDataConversion( GPSFullInfo_t *ptrGPSFullInfo,GPSInfo_t *ptrGPSInfo);

void SetDebugUartBusy(void);
void PeriodicGPSTimerInterruptHandler(uint16_t timeElapsed);
void SendEventToGPSStateMachine(GPS_EVENT_t gpsEvt);
uint32_t GetGpsSnr(void);

#endif /* __GPS_H */