//==============================================================================
//
//  GPS.c
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
//! This module takes care of the GPS attached to Smart Battery Board 
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
#include "GPS.h"
#include "string.h"
#include "stdlib.h"
#include "SysTask.h"
#include "SPI_Comm.h"
#include "SysDef.h"
#include "Board.h"
#include <ti/drivers/GPIO.h>
#include "ti/drivers/UART.h"
#include "ExtCommunication.h"
#include "TimerTask.h"
#include <ti/drivers/power/PowerCC32XX.h>

//==============================================================================
//	CONSTANTS, TYPEDEFS AND MACROS 
//==============================================================================
#define MAX_GPS_CMD_LENGTH                  30
#define KNOTS_MPS_FACTOR                    (float32_t)(0.514444f)
#define ROWS_COUNT_FOR_TOKENIZED_ARRAY      20                          //!< No of rows allocated for tokenized array
#define COLUMNS_COUNT_FOR_TOKENIZED_ARRAY   25                          //!< No of columns allocated for tokenized array
#define GPS_COUNTER_PERIOD                  30000                       //!< Set GPS Period to 30 seconds
#define CHECKSUM_LIMIT                      250
#define MAX_NMEA_LENGTH                     72
#define GPS_READ_RETRIES_COUNT              4
#define GPS_ACCURACY_LIMIT_THRESHOLD        18.0f
#define GPS_REST_ENABLE()
#define GPS_REST_DISABLE()

//Command Strings Sizes for GPS Configurations mentioned below 
#define CMD_GPS_VERSION_LENGTH                  8
#define CMD_GPS_CONFIG_GPS_SBAS_LENGTH         28
#define CMD_GPS_CONFIG_GPS_LENGTH              20 
#define CMD_GPS_CONFIG_GALILEO_SBAS_LENGTH     28
#define CMD_GPS_CONFIG_BEIDOU_SBAS_LENGTH      28
#define CMD_GPS_CONFIG_GLONASS_LENGTH          28
#define CMD_GPS_PORT_CHECK_LENGTH               9
#define CMD_GPS_CONFIG_DISABLE_NMEA_LENGTH     28
#define CMD_GPS_CONFIG_ENABLE_NMEA_LENGTH      28
#define CMD_GPS_GOTO_SLEEP_LENGTH              24
#define CMD_GPS_GOTO_1HZ_MODE_LENGTH           16
#define CMD_GPS_SAVE_CONFIG_LENGTH             21
#define CMD_UBX_CFG_PM_LENGTH                  32
#define CMD_UBX_CFG_PM2_LENGTH                 52
#define CMD_UBX_CFG_RXM_LENGTH                 10

#define GPS_PIN_ASSERT_TIME                    10

#define GPS_MAX_UART_RCV_TRANSC                1
#define GPS_UART_MAX_TRANSACTION_SIZE          1024

#define GPS_RCV_BUFFER_LENGTH                 (GPS_MAX_UART_RCV_TRANSC * GPS_UART_MAX_TRANSACTION_SIZE)
#define GPS_RCV_BUFFER_LENGTH_PLUS_END_OF_FRAME (GPS_RCV_BUFFER_LENGTH + END_OF_FRAME_BYTE)

#define GPS_CMD_BUFFER_LENGTH                   GPS_UART_MAX_TRANSACTION_SIZE
#define GPS_CMD_BUFFER_LENGTH_PLUS_END_OF_FRAME (GPS_CMD_BUFFER_LENGTH + END_OF_FRAME_BYTE)


//Command Strings for GPS Configurations
uint8_t const CMD_GPS_VERSION[]={0xB5,0x62,0x0A,0x04,0x00,0x00,0x0E,0x34};
uint8_t const CMD_GPS_CONFIG_GPS_SBAS[]={0xB5,0x62,0x06,0x3E,0x14,0x00,0x00,0x20,0x20,0x02,0x00,0x08,0x10,0x00,0x00,0x80,0x00,0x01,0x01,0x01,0x03,0x00,0x00,0x80,0x00,0x01,0xB9,0x1F};
uint8_t const CMD_GPS_CONFIG_GPS[] = {0xB5,0x62,0x06,0x3E,0x0C,0x00,0x00,0x20,0x20,0x01,0x00,0x08,0x10,0x00,0x30,0x5A,0x01,0x01,0x35,0xBC};
uint8_t const CMD_GPS_CONFIG_GALILEO_SBAS[]={0xB5,0x62,0x06,0x3E,0x14,0x00,0x00,0x20,0x20,0x02,0x02,0x04,0x08,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x03,0x00,0x00,0x80,0x00,0x01,0x2E,0x0A};
uint8_t const CMD_GPS_CONFIG_BEIDOU_SBAS[]={0xB5,0x62,0x06,0x3E,0x14,0x00,0x00,0x20,0x20,0x02,0x03,0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x03,0x00,0x00,0x80,0x00,0x01,0x3B,0xC6};
uint8_t const CMD_GPS_CONFIG_GLONASS[]={0xB5,0x62,0x06,0x3E,0X14,0x00,0x00,0x20,0x20,0x02,0x02,0x04,0x08,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x03,0x00,0x00,0x80,0x00,0x01,0x2E,0x0A};
uint8_t const CMD_GPS_PORT_CHECK[]={0xB5,0x62,0x06,0x00,0x01,0x00,0x01,0x08,0x22};
uint8_t const CMD_GPS_CONFIG_DISABLE_NMEA[]={0xB5,0x62,0x06,0x00,0x14,0x00,0x01,0x00,0x00,0x00,0xC0,0x08,0x00,0x00,0x80,0x25,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x8A,0x79};
uint8_t const CMD_GPS_CONFIG_ENABLE_NMEA[]={0xB5,0x62,0x06,0x00,0x14,0x00,0x01,0x00,0x00,0x00,0xC0,0x08,0x00,0x00,0x80,0x25,0x00,0x00,0x03,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x8E,0x95};
uint8_t const CMD_GPS_GOTO_SLEEP[]={0xB5,0x62,0x02,0x41,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x75,0xAB};
uint8_t const CMD_GPS_GOTO_1HZ_MODE[]={0xB5,0x62,0x06,0x86,0x08,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x97,0x6F};
//uint8_t const CMD_GPS_SAVE_CONFIG[] = {0xB5,0x62,0x06,0x09,0x0D,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x21,0xAF};
uint8_t const CMD_GPS_SAVE_CONFIG[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x1B, 0xA9};
uint8_t const CMD_UBX_CFG_PM[] = {0xB5, 0x62, 0x06, 0x32, 0x18, 0x00, 0x00, 0x06, 0x00, 0x00, 0x24, 0x90, 0x00, 0x00, 0x60, 0xEA, 0x00, 0x00, 0x20, 0xBF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x2D, 0x00, 0x71, 0x7D}; 
/*UBX_CFG_PM
:- Update Period(s): 60
:- Search Period(s): 180
:- Grid Offset(s): 0
:- Acquisition timeout(s): 45
:- On Time(s): 15
:- Wait for timefix: Null
:- Update: Eph
:- EXTINT: 0
:- EXTINT 'high' keeps awake: yes
*/

//uint8_t const CMD_UBX_CFG_PM2[] = {0xB5, 0x62, 0x06, 0x3B, 0x2C, 0x00, 0x01, 0x06, 0x64, 0x00, 0x0E, 0x9C, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x14, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x4F, 0xC1, 0x03, 0x00, 0x87, 0x02, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x64, 0x40, 0x01, 0x00, 0x53, 0xCC};
/*
0xB5, 0x62,
0x06, 0x3B, //Message identification
0x2C, 0x00,	//Length 44
0x01, // Message Version
0x06, //Reserved 1
0x64, //maxStartupStateDur 100
0x00, //reserved2
0x0E, 0x9C, 0x40, 0x01, //flags
0x00, 0x00, 0x00, 0x00, //Update Period 0
0x00, 0x00, 0x00, 0x00, //Searcg Period 0
0x00, 0x00, 0x00, 0x00, //Grid Offset 0
0x0F, 0x00, //On Time 15
0x14, 0x00, //minAcqTime 20
0x2C, 0x01, 0x00, 0x00, 0x4F, 0xC1, 0x03, 0x00, 0x87, 0x02, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x64, 0x40, 0x01, 0x00, //Reserved3
0x53, 0xCC //checksum
*/
                                            
uint8_t const CMD_UBX_CFG_PM2[] = {0xB5, 0x62, 0x06, 0x3B, 0x2C, 0x00, 0x01, 0x06, 0x2C, 0x00, 0x0E, 0x9C, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x14, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x4F, 0xC1, 0x03, 0x00, 0x87, 0x02, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x64, 0x40, 0x01, 0x00, 0x1B, 0x9C};
/*
0xB5, 0x62,
0x06, 0x3B, //Message identification
0x2C, 0x00,	//Length 44
0x01, // Message Version
0x06, //Reserved 1
0x64, //maxStartupStateDur 300
0x00, //reserved2
0x0E, 0x9C, 0x40, 0x01, //flags
0x00, 0x00, 0x00, 0x00, //Update Period 0
0x00, 0x00, 0x00, 0x00, //Searcg Period 0
0x00, 0x00, 0x00, 0x00, //Grid Offset 0
0x0F, 0x00, //On Time 15
0x14, 0x00, //minAcqTime 20
0x2C, 0x01, 0x00, 0x00, 0x4F, 0xC1, 0x03, 0x00, 0x87, 0x02, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x64, 0x40, 0x01, 0x00, //Reserved3
0x53, 0xCC //checksum
*/
uint8_t const CMD_UBX_CFG_RXM[] = {0xB5, 0x62, 0x06, 0x11, 0x02, 0x00, 0x08, 0x01, 0x22, 0x92};
//==============================================================================
//	LOCAL DATA STRUCTURE DEFINITION
//==============================================================================
typedef enum 
{
    PARSE_GNGGA=0,
    PARSE_FIX_TIME,
    PARSE_LATITUDE,
    PARSE_LATITUDE_DIR,
    PARSE_LONGITUDE,
    PARSE_LONGITUDE_DIR,
    PARSE_FIX_QUALITY,
    PARSE_NUMBER_OF_SATTELITES,
    PARSE_HORIZONATAL_DILUTION,
    PARSE_ALTITUDE,
    PARSE_ALTITUDE_UNIT,
    PARSE_HIGHT_ABOVE_SEA_LEVEL,
    PARSE_HIGHT_ABOVE_SEA_LEVEL_UNIT,
    PARSE_TIME_SINCE_LAST_UPDATE,
    PARSE_STATION_ID_NUMBER
}GPSGNGGA_PARSER_STATE_t;



enum
{
    GPS_DISABLE,
    GPS_IN_WARM_UP,
    GPS_LOCK_ACCUIRED,
}GPS_STATUS_t;

typedef enum 
{
    GPS_RUNNING_NORMAL=0,
    GPS_RUNNING_IN_ALARM
}GPS_RUNNING_STATE_t;

typedef enum 
{
  GNGGA_STRING =0,
  GPRMC_STRING,
  GPGSV_STRING,
  
  NMEA_LAST_STRING
}NMEA_STRINGS_t;

typedef struct GPSStateMachine
{
    GPSStateStruct_t const gpsStates[GPS_LAST_STATE_INDEX];
    GPS_SATATE_t currentGPSState;
    GPS_SATATE_t previousGPSState;
    int32_t gpsCurrentSateTimeOut;
}GPSStateMachine_t;

//==============================================================================
//	GLOBAL DATA DECLARATIONS
//==============================================================================
GPSLastKnownInfo_t GPSLastKnownInfoObj = { 
                                      .gpsLocationInfo.gpsInfo.latitude=4851.504, 
                                      .gpsLocationInfo.latitudeDir='N', 
                                      .gpsLocationInfo.gpsInfo.longitude=217.67, 
                                      .gpsLocationInfo.longitudeDir='E',
                                      .gpsLocationInfo.gpsInfo.horizantalDilution = 1.0,
                                      
                                      .gpsConvertedInfo.latitude = 48.858395,
                                      .gpsConvertedInfo. longitude= 2.2945,
                                      .gpsConvertedInfo. horizantalDilution= 1.0,
                                      };

UART_Handle              gpsUartHandle;

static Clock_Struct      GPSClkStruct;
//static Clock_Params      GPSClkParam;

//==============================================================================
//	LOCAL DATA DECLARATIONS
//==============================================================================
uint8_t gpsReadBuff[GPS_RCV_BUFFER_LENGTH_PLUS_END_OF_FRAME];
uint8_t gpsWriteBuff[GPS_CMD_BUFFER_LENGTH_PLUS_END_OF_FRAME];

static GPSFullInfo_t gpsLiveInfo;
static GPSMiscellaneousInfo_t gpsExtraInfo;

BOOLEAN isGPSSpeedInfoFound             = FALSE;
BOOLEAN isGPSFixFound                   = FALSE;

static void GPSInitStateEntry(void);
static void GPSInitStateExit(void);

static void GPSWakeUpStateEntry(void);
static void GPSWakeUpStateExit(void);

static void GPSDataRecvStateEntry(void);
static void GPSDataRecvStateExit(void);

static void GPSIdleStateEntry(void);
static void GPSIdleStateExit(void);

static void GPSPowerSavingStateEntry(void);
static void GPSPowerSavingStateStateExit(void);

static GPSStateMachine_t gpsStateMachine = 
{
    .gpsStates =
    {
        {
            .entryFunc = GPSInitStateEntry,
            .exitFunc = GPSInitStateExit,
            .stateTimeOut = GPS_INITIALIZATION_STATE_TIME_OUT,
        },
        {
            .entryFunc = GPSDataRecvStateEntry,
            .exitFunc = GPSDataRecvStateExit,
            .stateTimeOut = GPS_DATA_RECV_STATE_TIME_OUT,
        },
        {
            .entryFunc = GPSPowerSavingStateEntry,
            .exitFunc = GPSPowerSavingStateStateExit,
            .stateTimeOut = GPS_TIME_OUT_MAX,
        },
        {
            .entryFunc = GPSWakeUpStateEntry,
            .exitFunc = GPSWakeUpStateExit,
            .stateTimeOut = GPS_WAKE_UP_STATE_TIME_OUT,
        },
        {
          .entryFunc = GPSIdleStateEntry,
          .exitFunc = GPSIdleStateExit,
          .stateTimeOut = GPS_IDLE_STATE_TIME_OUT,
        }
        
    },
    .currentGPSState = GPS_INITIALIZATION_STATE,
    .previousGPSState = GPS_INITIALIZATION_STATE,
    .gpsCurrentSateTimeOut = GPS_INITIALIZATION_STATE_TIME_OUT
};

//==============================================================================
//	LOCAL FUNCTION PROTOTYPES
//==============================================================================

static void GPSSateDispatch(GPS_SATATE_t nextState);

static void  ParseGPSData(UART_Handle handle, void * GPSData, size_t dataLen);

static void GPSDriverInit(void);
static void ExtractGPSData(void);

static void GPSUARTRXDisable(void);
static void GPSUARTRXEnable(void);
static BOOLEAN ExtractGNGGAData(uint8_t ReadBuffer[],uint32_t startIndex,uint8_t messageLength);
static BOOLEAN ExtractGPRMCData(uint8_t ReadBuffer[],uint32_t startIndex,uint8_t messageLength);
static BOOLEAN ExtractGPGSVData(uint8_t ReadBuffer[],uint32_t startIndex,uint8_t messageLength);
static int TokenizeString(uint8_t *s_String,int8_t s_Token[][25],int8_t c_Delimiter,uint8_t messageLength);
static BOOLEAN ValidateCheckSum(uint8_t ReadBuffer[],uint32_t startAddress);
static uint8_t CharToHex(uint8_t character);
static void BufferCopy(uint8_t* destination, uint8_t* source, uint8_t size);
static void GPSWakeUpAssert(void);
static void GPSWakeUpDeAssert(void);
static void GPSResetAssert(void);
static void ExtractNMEAString(NMEA_STRINGS_t stringType, uint8_t IdenStr[]);
//==============================================================================
//	LOCAL AND GLOBAL FUNCTIONS IMPLEMENTATION
//==============================================================================



//==============================================================================
//
//   void GPSStateMachine (uint16_t EventType)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/21      
//
//!  This function handels the messages posted by systask to GPS state machine 
//
//==============================================================================
void GPSStateMachine (uint16_t EventType)
{
  BOOLEAN isInError = false;
  
  uint32_t currentTimeStamp = 0;
  uint16_t msOffset = 0u;
  
  switch(gpsStateMachine.currentGPSState)
  {
  case GPS_INITIALIZATION_STATE:
      if(GPS_EVENT_SLEEP_EVENT == EventType)
      {
          GPSSateDispatch(GPS_POWER_SAVING_STATE);
      }
      if(GPS_EVENT_TIME_OUT == EventType)
      {
          GPSSateDispatch(GPS_WAKE_UP_STATE);
      }
    break;
    
  case GPS_WAKE_UP_STATE:
      if(GPS_EVENT_SLEEP_EVENT == EventType)
      {
          GPSSateDispatch(GPS_POWER_SAVING_STATE);
      }
      if(GPS_EVENT_TIME_OUT == EventType)
      {
          GPSSateDispatch(GPS_DATA_RECV_STATE);
      }
    break;
    
  case GPS_DATA_RECV_STATE:
      if(GPS_EVENT_SLEEP_EVENT == EventType)
      {
          GPSSateDispatch(GPS_POWER_SAVING_STATE);
      }
      else if(GPS_EVENT_REC_BUFFER_FULL == EventType)
      {
          PRCMRTCGet(&currentTimeStamp, &msOffset);
          
        ExtractGPSData();
        
          if( GPS_CLOCK_DRIFT_TIME_OUT > (currentTimeStamp - gpsExtraInfo.lastGPSLockTimeStamp) )
          {
          if(false == gpsLiveInfo.isLockAcquired)
          {
            //Try rereading the data until uart has come incoming
            GPSSateDispatch(GPS_DATA_RECV_STATE);
          }
          else
          {
            if ( GPS_ACCURACY_LIMIT_THRESHOLD < gpsLiveInfo.gpsInfo.horizantalDilution)
            {
              GPSDebugPrint("Horizontal dilution is %f, re-reading again\n\r",GPSLastKnownInfoObj.gpsLocationInfo.gpsInfo.horizantalDilution);
              GPSDebugFlush();
              GPSSateDispatch(GPS_DATA_RECV_STATE);
            }
            else if (TRUE == IsRemoteUnitInAlarm())
            {
              GPSSateDispatch(GPS_WAKE_UP_STATE);
            }
            else
            {
              GPSSateDispatch(GPS_IDLE_STATE);
            }
            }
          }
          else
          {
              //In Case of NO GPS Fixed after GPS_CLOCK_DRIFT_TIME_OUT ReInitialize.
              isInError = true;
          }
          GPSDebugPrint("%d seconds to reset GPS chip\r\n",currentTimeStamp - gpsExtraInfo.lastGPSLockTimeStamp);
          GPSDebugFlush();
      }
      else if(GPS_DATA_RECV_STATE == gpsStateMachine.previousGPSState)
      { 
        if (TRUE == IsRemoteUnitInAlarm())
        {
          GPSSateDispatch(GPS_WAKE_UP_STATE);
        }
      else
      {
          GPSSateDispatch(GPS_IDLE_STATE);
        }
      }
      else
      {
        //In Case of GPS UART Read TimeOut ReInitialize GPS module.
          isInError = true;
      }
      
      if( true == isInError)
      {
          GPSDebugPrint("GPS Recive State TimeOut\r\n");
          GPSDebugFlush();
          GPSSateDispatch(GPS_INITIALIZATION_STATE);
      }
    break;
        
   case GPS_IDLE_STATE:
      if(GPS_EVENT_SLEEP_EVENT == EventType)
      {
          GPSSateDispatch(GPS_POWER_SAVING_STATE);
      }
      if(GPS_EVENT_TIME_OUT == EventType)
      {
          GPSSateDispatch(GPS_WAKE_UP_STATE);
      }
    break;
    
  case GPS_POWER_SAVING_STATE:
      if(GPS_EVENT_REINIT_EVENT == EventType)
      {
          GPSSateDispatch(GPS_INITIALIZATION_STATE);
      }
    break;
    
  default:
    GPSDebugPrint("Invalid GPS Mailbox Received ID:%d\r\n",EventType);
    GPSDebugFlush();
    break;
  }
}

static void GPSSateDispatch(GPS_SATATE_t nextState)
{
    gpsStateMachine.gpsStates[gpsStateMachine.currentGPSState].exitFunc();
    gpsStateMachine.previousGPSState = gpsStateMachine.currentGPSState;
    gpsStateMachine.currentGPSState = nextState;
    gpsStateMachine.gpsStates[nextState].entryFunc();
}

static void GPSWakeUpAssert(void)
{
    GPIO_write(Board_GPS_EXT_INT_PIN,1);
}

static void GPSWakeUpDeAssert(void)
{
    GPIO_write(Board_GPS_EXT_INT_PIN,0);
}

static void GPSResetAssert(void)
{
    Power_setConstraint(PowerCC32XX_DISALLOW_LPDS);
    GPIO_write(Board_GPS_RST_PIN,0);
    Task_sleep(GPS_PIN_ASSERT_TIME);
    GPIO_write(Board_GPS_RST_PIN,1);
    Power_releaseConstraint(PowerCC32XX_DISALLOW_LPDS);
}

static void GPSInitStateEntry(void)
{
    uint16_t msOffset = 0u;
    GPSDebugPrint("GPS In Initializing State\r\n");
    GPSDebugFlush();
    
    gpsStateMachine.gpsCurrentSateTimeOut = GPS_INITIALIZATION_STATE_TIME_OUT;
    
    GPSResetAssert();

    GPSDriverInit();
  //Send GPS receiver to power saving mode
  //Send PSM configuration
  GPSWrite((uint8_t *)CMD_UBX_CFG_PM2,CMD_UBX_CFG_PM2_LENGTH);
  Task_sleep(10);
  GPSWrite((uint8_t *)CMD_UBX_CFG_RXM,CMD_UBX_CFG_RXM_LENGTH);
  Task_sleep(10);
  GPSWrite((uint8_t *)CMD_UBX_CFG_PM2,CMD_UBX_CFG_PM2_LENGTH);
  Task_sleep(10);
  GPSWrite((uint8_t *)CMD_UBX_CFG_RXM,CMD_UBX_CFG_RXM_LENGTH);
  Task_sleep(10);
  //Send save configuration command to the GPS module
  GPSWrite((uint8_t *)CMD_GPS_SAVE_CONFIG,CMD_GPS_SAVE_CONFIG_LENGTH);
  Task_sleep(10);
  GPSWrite((uint8_t *)CMD_GPS_GOTO_SLEEP,CMD_GPS_GOTO_SLEEP_LENGTH);
  
    
    PRCMRTCGet(&gpsExtraInfo.lastGPSLockTimeStamp, &msOffset); 
}

static void GPSInitStateExit(void)
{
    
}

static void GPSWakeUpStateEntry(void)
{
    GPSDebugPrint("GPS In Wake-Up State\r\n");
    GPSDebugFlush();
    
    GPSWakeUpAssert();  
    gpsStateMachine.gpsCurrentSateTimeOut = GPS_WAKE_UP_STATE_TIME_OUT;
    GPSWakeUpDeAssert();
}

static void GPSWakeUpStateExit(void)
{
    
}

static void GPSDataRecvStateEntry(void)
{
    GPSDebugPrint("GPS In Data Rcving State\r\n");
    GPSDebugFlush();
    if(gpsStateMachine.currentGPSState == gpsStateMachine.previousGPSState)
    {
      gpsStateMachine.gpsCurrentSateTimeOut = 2 * GPS_DATA_RECV_STATE_TIME_OUT;
    }
    else
    {
      GPSWakeUpAssert();
      GPSWakeUpDeAssert();
    gpsStateMachine.gpsCurrentSateTimeOut = GPS_DATA_RECV_STATE_TIME_OUT;
    }
        
    if (TRUE == GPSRead(gpsReadBuff))
    {
      SendEventToGPSStateMachine(GPS_EVENT_REC_BUFFER_FULL);
    }
    else
    {
      SendEventToGPSStateMachine(GPS_EVENT_TIME_OUT);
    }
}

static void GPSDataRecvStateExit(void)
{
  GPSDebugPrint("*****************************************************\r\n");
  GPSDebugFlush();  
  GPSDebugPrint("%.*s",256,gpsReadBuff[0*256]);
  GPSDebugFlush();
  GPSDebugPrint("%.*s",256,&gpsReadBuff[1*256]);
  GPSDebugFlush();
  GPSDebugPrint("%.*s",256,&gpsReadBuff[2*256]);
  GPSDebugFlush();
  GPSDebugPrint("%.*s\r\n",256,&gpsReadBuff[3*256]);
  GPSDebugFlush();
  
  GPSDebugPrint("GPS Exiting From Data Rcving State\r\n");
  GPSDebugFlush();
  //GPSUARTRXDisable();
}

static void GPSIdleStateEntry(void)
{
  
    GPSDebugPrint("GPS In Idle State\r\n");
    GPSDebugFlush();
    
    if(true == gpsLiveInfo.isLockAcquired)
    {
        gpsStateMachine.gpsCurrentSateTimeOut = GPS_IDLE_STATE_TIME_OUT_ON_LOCK;
    }
    else
    {   
        gpsStateMachine.gpsCurrentSateTimeOut = GPS_IDLE_STATE_TIME_OUT;
    }
}


static void GPSIdleStateExit(void)
{
    
}

static void GPSPowerSavingStateEntry(void)
{
    GPSDebugPrint("GPS In Power Saving State\r\n");
    GPSDebugFlush();
    
    gpsStateMachine.gpsCurrentSateTimeOut = GPS_TIME_OUT_MAX;
    
    GPSSleepCommand();
    GPSUARTRXDisable();
}

static void GPSPowerSavingStateStateExit(void)
{
    
}

//==============================================================================
//
//  void GPSInit(void)
//
//   Author:   
//   Date:     
//
//!  This function 
//
//==============================================================================
void GPSInit(void)
{
  //Initialize the GPS Uart driver
  //GPSDriverInit();
  
  GPSInitStateEntry();
  
  //Push-Pull wakeup line if GPS is not working previously 
  RegisterPeriodicFunction(&PeriodicGPSTimerInterruptHandler);
  
  //GPSRead(gpsReadBuff);
}

//==============================================================================
//
//  void GPSOpen(void)
//
//   Author:   
//   Date:     
//
//!  This function 
//
//==============================================================================
void GPSOpen(void)
{
  
}

//==============================================================================
//
//  BOOLEAN GPSRead(uint8_t ptrBuffer[])
//
//   Author:   
//   Date:     
//
//!  This function 
//
//==============================================================================
BOOLEAN GPSRead(uint8_t ptrBuffer[])
{
  //Local data declaration
  int32_t retunStatus = 0;
  
  GPSUARTRXEnable();
  retunStatus = UART_read(gpsUartHandle,ptrBuffer, GPS_UART_MAX_TRANSACTION_SIZE);
  GPSUARTRXDisable();
  
  //Return status
  if ((retunStatus == UART_STATUS_ERROR) || ( GPS_UART_MAX_TRANSACTION_SIZE > retunStatus))
    return FALSE;
  else
    return TRUE;
}

//==============================================================================
//
//  BOOLEAN GPSWrite(uint8_t cmd[],uint8_t cmdSize)
//
//   Author:   
//   Date:     
//
//!  This function 
//
//==============================================================================
BOOLEAN GPSWrite(uint8_t cmd[],uint8_t cmdSize)
{
    //Local data declaration
    int8_t retunStatus = UART_STATUS_ERROR;
    
    memset(gpsWriteBuff,0,GPS_CMD_BUFFER_LENGTH_PLUS_END_OF_FRAME);
    
    //Copy the parsed string from ROM to local buffer on stack as DMA channel can't access ROM area
    BufferCopy((uint8_t*)gpsWriteBuff,(uint8_t*)cmd,cmdSize);
    
    //Write the parsed buffer to UART channel 
    retunStatus = UART_write(gpsUartHandle, gpsWriteBuff, cmdSize);
    //Return status
    if (retunStatus == UART_STATUS_SUCCESS)
    {
        return true;
    }
    return false;
}

//==============================================================================
//
//  void GPSGetConvertedLocationInfo(GPSInfo_t ptrGPSInfo)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/21   
//
//!  This function is used by other modules to fetch the Current GPS data 
//
//==============================================================================
void GPSGetConvertedLocationInfo(GPSInfo_t *ptrGPSInfo)
{
    ptrGPSInfo->longitude    =  GPSLastKnownInfoObj.gpsConvertedInfo.longitude; 
    ptrGPSInfo->latitude     =  GPSLastKnownInfoObj.gpsConvertedInfo.latitude;
    ptrGPSInfo->horizantalDilution = GPSLastKnownInfoObj.gpsConvertedInfo.horizantalDilution;
}

//==============================================================================
//
//  void GPSGetTimeInfo()
//
//   Author:   
//   Date:     
//
//!  This function 
//
//==============================================================================
void GPSGetTimeInfo()
{
    
}

//==============================================================================
//
//  void GPSClose(void)
//
//   Author:   
//   Date:     
//
//!  This function 
//
//==============================================================================
void GPSClose(void)
{
    
}

//==============================================================================
//
//  void ParseGPSData(UART_Handle handle, void *GPSData, size_t dataLen)
//
//   Author:   
//   Date:     
//
//!  This function 
//
//==============================================================================
void ParseGPSData(UART_Handle handle, void *GPSData, size_t dataLen)
{
  SendEventToGPSStateMachine(GPS_EVENT_REC_BUFFER_FULL);
}

//==============================================================================
//
//   void GPSDriverInit(void)
//
//   Author:   Ammar Younas 
//   Date:     2018/05/21   
//
//!  This function Initializes the UART driver for GPS communication
//
//==============================================================================
void GPSDriverInit(void)
{
  static BOOLEAN isGPSDriverConfigured = FALSE;
  
  if (NULL != gpsUartHandle) 
  {
      UART_close(gpsUartHandle);
      isGPSDriverConfigured = false;
      gpsUartHandle = NULL;
  }
  
  if (isGPSDriverConfigured == false)
  {
    UART_Params      gpsUartParams;
    UART_Params_init(&gpsUartParams);
    gpsUartParams.baudRate  = GPS_UART_BAUD_RATE;
    gpsUartParams.dataLength = UART_LEN_8;
    gpsUartParams.parityType = UART_PAR_NONE;
    gpsUartParams.stopBits = UART_STOP_ONE;
    gpsUartParams.writeDataMode = UART_DATA_BINARY;
    gpsUartParams.readDataMode = UART_DATA_BINARY;
    gpsUartParams.readReturnMode = UART_RETURN_FULL;
    gpsUartParams.readMode = UART_MODE_BLOCKING;
    gpsUartParams.readTimeout = GPS_UART_READ_WRITE_TIME_OUT;
    gpsUartParams.writeTimeout = GPS_UART_READ_WRITE_TIME_OUT;
    gpsUartParams.readEcho = UART_ECHO_OFF;
    gpsUartParams.readCallback = ParseGPSData;
    gpsUartHandle = UART_open(Board_GPS_UART, &gpsUartParams);
    if (NULL == gpsUartHandle) 
    {
      GPSDebugPrint("Unable to Initialize GPS UART\r\n");
      GPSDebugFlush();
    }
    else
    {
      isGPSDriverConfigured = TRUE;
      UART_control(gpsUartHandle, UART_CMD_RXDISABLE, NULL);
    }
  }
}

//==============================================================================
//
//   void PeriodicGPSTimerInterruptHandler(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/21   
//
//!  This function posts messgaes related to GPS to Systask 
//
//==============================================================================
void PeriodicGPSTimerInterruptHandler(uint16_t timeElapsed)
{
  
  gpsStateMachine.gpsCurrentSateTimeOut -= timeElapsed;
  
  GPSDebugPrint("GPS Periodic caller %d\n", gpsStateMachine.gpsCurrentSateTimeOut);
  GPSDebugFlush();
  
  if(0 >= gpsStateMachine.gpsCurrentSateTimeOut) 
  {
    GPSDebugPrint("GPS timeout Event Dispatched\r\n");
    GPSDebugFlush();  
    SendEventToGPSStateMachine(GPS_EVENT_TIME_OUT);
  }

}

//==============================================================================
//
//   void ExtractGPSData (void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/21   
//
//!  This function Extracts NMEA data from gps read string  
//
//==============================================================================
void ExtractGPSData (void)
{
  uint8_t const * const NMEA_String[NMEA_LAST_STRING] ={"$GNGGA,", "$GPRMC,", "$GPGSV,"};
  
  
//  GPSDebugPrint("*****************************************************\r\n");
//  GPSDebugFlush();  
//  GPSDebugPrint("%.*s",GPS_UART_MAX_TRANSACTION_SIZE,gpsReadBuff);
//  GPSDebugFlush();
//  GPSDebugPrint("%.*s",GPS_UART_MAX_TRANSACTION_SIZE,&gpsReadBuff(GPS_UART_MAX_TRANSACTION_SIZE));
//  GPSDebugFlush();
//  GPSDebugPrint("%.*s",GPS_UART_MAX_TRANSACTION_SIZE,&gpsReadBuff(2*GPS_UART_MAX_TRANSACTION_SIZE));
//  GPSDebugFlush();
//  GPSDebugPrint("%.*s\r\n",GPS_UART_MAX_TRANSACTION_SIZE,&gpsReadBuff(3*GPS_UART_MAX_TRANSACTION_SIZE));
//  GPSDebugFlush();
  
  for (NMEA_STRINGS_t index = GNGGA_STRING; index < NMEA_LAST_STRING; index++)
  {
    ExtractNMEAString(index, (uint8_t *)NMEA_String[index]);
  }
}

//==============================================================================
//
//   void ExtractNMEAString(NMEA_STRINGS_t stringType, uint8_t IdenStr[])
//
//   Author:   Ammar Younas
//   Date:     2019/03/11   
//
//!  This function Extracts NMEA data from gps read string  
//
//==============================================================================
static void ExtractNMEAString(NMEA_STRINGS_t stringType, uint8_t IdenStr[])
{
  uint8_t *ptrToData = gpsReadBuff;
  uint8_t *ptrToEndIdentifier = NULL;
  uint8_t ExpectedCounter = 0u;
  uint8_t ExpectedLength  = 0u;
  BOOLEAN isLastIteration = FALSE;
  
  //Null terminate the string for safety
  gpsReadBuff[GPS_RCV_BUFFER_LENGTH] = (int8_t) NULL;
  
  //Find the GPGGA string for parsing 
  while ((ptrToData = (uint8_t *)strstr((char const*)ptrToData,(char const*)IdenStr)) && (FALSE == isLastIteration)) 
  {
    ptrToEndIdentifier = (uint8_t *)strstr((char const*)ptrToData,"*");
    
    if (NULL != ptrToEndIdentifier)
    {
      ExpectedLength = ptrToEndIdentifier - ptrToData;
      if (ExpectedLength > MAX_NMEA_LENGTH)
      {
        ExpectedLength = MAX_NMEA_LENGTH;
      }
      //Dont Parse data if incomplete 
      if((ExpectedCounter + MAX_NMEA_LENGTH) < GPS_RCV_BUFFER_LENGTH)
      {
        switch(stringType)
        {
        case GNGGA_STRING:
          isLastIteration = ExtractGNGGAData(ptrToData,7u,ExpectedLength-3);
          break;
          
        case GPRMC_STRING:
          isLastIteration = ExtractGPRMCData(ptrToData,7u,ExpectedLength-3);
          break;
          
        case GPGSV_STRING:
          isLastIteration = ExtractGPGSVData(ptrToData,7u,ExpectedLength-3);
          break;
  
        default:
          break; 
        }
      }
      ptrToData = ptrToEndIdentifier;
    }
    else
    {
      //Break the loop as there is no end identifier "*"
      ptrToData = NULL;
      break;
    }
  }
}

//==============================================================================
//
//   TokenizeString(char *s_String, char s_Token[][25], char c_Delimiter, 
//                                                  unsigned char messageLength)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/21   
//
//!  This function Tokenizes GPS string wrt to parsed delimeter 
//
//==============================================================================
static int TokenizeString(
                          uint8_t *s_String,                                            //!< String to be tokenized
                          int8_t s_Token[][25],                                        //!< Strings after the separation
                          int8_t c_Delimiter,                                          //!< Delimiter which will be used for tokenizing
                          uint8_t messageLength                                        //!< Actual message length
                            )
{
  int j = 0;
  unsigned int i_Offset = 0;
  char b_Flag = 0;
  int count = 0;
  for (i_Offset = 0;i_Offset <= messageLength; i_Offset++)
  {
    // loop length controlers
    if(((count+1) >= (ROWS_COUNT_FOR_TOKENIZED_ARRAY)) || ((j+1) >= (COLUMNS_COUNT_FOR_TOKENIZED_ARRAY)))
    {
      //Buffer Overflow control
      break;
    }
    
    if (s_String[i_Offset] != c_Delimiter && s_String[i_Offset] != '\t' && s_String[i_Offset] != '\n' && s_String[i_Offset] != '\0')
    {
      s_Token[count][j] = s_String[i_Offset];
      j++;
      b_Flag = 1;
      continue;
    }
    if (b_Flag)
    {
      s_Token[count][j] = '\0';
      count++;
      j = 0;
      b_Flag = 0;
    }
  }
  return (count - 1);
}

//==============================================================================
//
//   static bool ValidateCheckSum(uint8_t *ReadBuffer,uint8_t startAddress);
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/21   
//
//!  This function Validates GPS string checksum
//
//==============================================================================
static BOOLEAN ValidateCheckSum(
                                uint8_t ReadBuffer[],                                       //!< data received from the GPS receiver
                                uint32_t startAddress                              //!< Start address of the message
                                    )
{
   // for getting the character
   uint8_t Character = 0u;
   //for indexing the loop
   uint32_t loopIndex = 0u;
   //for indexing the array
   uint32_t arrayIndex = startAddress;
   //For storing the calculated checksum
   uint8_t checksumCal = 0u;
   //For getting the received checksum
   uint8_t receivedChecksum = 0u;
   //For calculating the result
   BOOLEAN compareResult = false;
   //Calcuate the checksum
   for(loopIndex = 0u; loopIndex < (uint32_t)CHECKSUM_LIMIT; loopIndex++)
   {
       Character = (uint8_t)ReadBuffer[arrayIndex];
       switch(Character)
       {
       case '$':
         //Ignore this
         break;
       case '*':
         //Terminte the calculation here as we have reached the end
         loopIndex = (uint8_t)CHECKSUM_LIMIT;
         break;
       default:
         //First value for the checksum
         if( checksumCal == 0u )
         {
            checksumCal = Character;
         }
         else
         {
            checksumCal = checksumCal ^ Character;
         }
         break;
       }
       arrayIndex = arrayIndex + 1u;
   }
   //Get the checksum received from the buffer
   receivedChecksum = (16u*CharToHex((uint8_t)ReadBuffer[arrayIndex])) + CharToHex((uint8_t)ReadBuffer[arrayIndex+1u]);
   //Match the checksums
   if( checksumCal == receivedChecksum )
   {
      compareResult = true;
   }
   else
   {
      compareResult =  false;
   }
   return compareResult;
}

//==============================================================================
//
//   CharToHex( uint8_t character)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/21   
//
//!  This function takes the ascii charater and returns the corresponsding
//!  hex value 
//
//==============================================================================

static uint8_t CharToHex(
                          uint8_t character                               //!< Ascii charater for conversion
                       )
{
   uint8_t result = 0u;
   
   if( (character >= '0') && (character <= '9') )
   {
      result = character - '0';
   }
   else if( (character == 'a') || (character == 'A') )
   {
      result = 0xau;
   }
   else if( (character == 'b') || (character == 'B') )
   {
      result = 0xbu;
   }
   else if ( (character == 'c') || (character == 'C') )
   {
      result = 0xcu;
   }
   else if ( (character == 'd') || (character == 'D') )
   {
      result = 0xdu;
   }
   else if ( (character == 'e') || (character == 'E') )
   {
      result = 0xeu;
   }
   else if ( (character == 'f') || (character == 'F') )
   {
      result = 0xfu;
   }
   else
   {
      //Do nothing
   }
   return result;
}
//==============================================================================
//
//   ExtractGNGGAData(uint8_t ReadBuffer[],uint32_t startIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/21   
//
//!  This function Extracts GPGGA data from NMEA string 
//
//==============================================================================
static BOOLEAN ExtractGNGGAData(
                                uint8_t ReadBuffer[],  //!< data received from the GPS receiver
                                uint32_t startIndex,   //!< Start index of the message
                                uint8_t messageLength  //!< Message length
                                  )
{
  int8_t sToken[ROWS_COUNT_FOR_TOKENIZED_ARRAY][COLUMNS_COUNT_FOR_TOKENIZED_ARRAY];
  uint32_t count = 0u;
  uint16_t msOffset = 0u;
  BOOLEAN vaildateCheckSum = false;
  BOOLEAN retStatus = FALSE;
  
  vaildateCheckSum = ValidateCheckSum(ReadBuffer, startIndex-6u);
  if( vaildateCheckSum == true )
  {
    //Clear the token array prior to populating 
    memset(sToken,0,375);
    //Clear the "GPS lock acquired" flag in case set previously 
    gpsLiveInfo.isLockAcquired = false;
    //Tokenize the parsed string keeping the parsed delimeter as tokenizing reference
    count = TokenizeString(&ReadBuffer[startIndex], sToken, ',',messageLength - 5);
    //After tokenizing acquire the data
    gpsExtraInfo.utcTime                   = (float32_t)atof((char*)sToken[0u]);
    gpsLiveInfo.gpsInfo.latitude                   = (float32_t)atof((char*)sToken[1u]);
    gpsLiveInfo.latitudeDir                = (int8_t)(*sToken[2u]);
    gpsLiveInfo.gpsInfo.longitude                  = (float32_t)atof((char*)sToken[3u]);
    gpsLiveInfo.longitudeDir               = (int8_t)*sToken[4u];
    gpsExtraInfo.fixQuality                = (int32_t)atoi((char*)sToken[5u]);
    gpsExtraInfo.numberOfSatellites        = (int32_t)atoi((char*)sToken[6u]);
    gpsLiveInfo.gpsInfo.horizantalDilution        = (float32_t)atof((char*)sToken[7u]);
    gpsExtraInfo.altitudeAboveSeaLevel     = (float32_t)atof((char*)sToken[8u]);
    gpsExtraInfo.altitudeUnits             = (int8_t)*sToken[9];
    gpsExtraInfo.heightOfGeoid             = (float32_t)atof((char*)sToken[10]);
    gpsExtraInfo.heightOfGeoidUnits        = (int8_t)*sToken[11];
    //Minimum 5 sattelites are required to get the fix
    gpsExtraInfo.isLocationDataAvailable = true;
    GetCurrentTimeAndDate(&(GPSLastKnownInfoObj.lastUpdatedOn));
    
    // Criteria for valid GPS location
    if(gpsExtraInfo.fixQuality > 0)
    {
      PRCMRTCGet(&gpsExtraInfo.lastGPSLockTimeStamp, &msOffset);
      
      Task_disable();
      //Set the flags indicating GPS has acquired lock
      isGPSFixFound = true;
      //Keep the data of the last fix until new fix is found 
      gpsLiveInfo.isLockAcquired = true;
      GPSLastKnownInfoObj.gpsLocationInfo.isLockAcquired = true;
      
      if (GPS_ACCURACY_LIMIT_THRESHOLD > gpsLiveInfo.gpsInfo.horizantalDilution)
      {
        GPSLastKnownInfoObj.gpsLocationInfo.gpsInfo.latitude = gpsLiveInfo.gpsInfo.latitude;
        GPSLastKnownInfoObj.gpsLocationInfo.latitudeDir = gpsLiveInfo.latitudeDir;
        GPSLastKnownInfoObj.gpsLocationInfo.gpsInfo.longitude = gpsLiveInfo.gpsInfo.longitude;
        GPSLastKnownInfoObj.gpsLocationInfo.longitudeDir = gpsLiveInfo.longitudeDir;
        GPSLastKnownInfoObj.gpsLocationInfo.gpsInfo.horizantalDilution = gpsLiveInfo.gpsInfo.horizantalDilution;
        GpsDataConversion( &GPSLastKnownInfoObj.gpsLocationInfo,&GPSLastKnownInfoObj.gpsConvertedInfo);
      }
      
      Task_enable();
      
      GPSDebugPrint("*******************************************************\r\n");
      GPSDebugFlush();
      
      GPSDebugPrint("\r\nGPS lock acquired with %d satellites\r\n",gpsExtraInfo.numberOfSatellites);
      GPSDebugFlush();
      GPSDebugPrint("\r\nGPS message parsed successfully - Fix Quality = Valid\r\n");
      GPSDebugFlush();
      
      GPSDebugPrint("\n*******************************************************\n");
      GPSDebugFlush();
    }
    else
    {
      // Fix value is not upto standard
      gpsLiveInfo.isLockAcquired = false;
      GPSLastKnownInfoObj.gpsLocationInfo.isLockAcquired = false;
    }
    
    retStatus = TRUE;
  }
  else if ( count == 0u )
  {
    // If the input string is not tokenized 
    isGPSFixFound = false;
  }
  else
  {
    // If the checksum fails as well as the tokenization of the data fails
    isGPSFixFound = false;
  }
  return retStatus;
}

//==============================================================================
//
//   ExtractGPRMCData(uint8_t ReadBuffer[],uint32_t startIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/21   
//
//!  This function Extracts GPRMC and GNRMC  data from NMEA string  
//
//==============================================================================
static BOOLEAN ExtractGPRMCData(
                                uint8_t ReadBuffer[],  //!< data received from the GPS receiver
                                uint32_t startIndex,   //!< Start index of the message
                                uint8_t messageLength  //!< Message length
                                  )
{
  int8_t sToken[ROWS_COUNT_FOR_TOKENIZED_ARRAY][COLUMNS_COUNT_FOR_TOKENIZED_ARRAY];
  uint32_t count = 0u;
  BOOLEAN vaildateCheckSum = false;
  BOOLEAN retStatus = FALSE;
  vaildateCheckSum = ValidateCheckSum(ReadBuffer, startIndex - 6u);
  
  if( vaildateCheckSum == true )
  {
    memset(sToken,0,200);
    count = TokenizeString(&ReadBuffer[startIndex],sToken,',',messageLength);
    gpsExtraInfo.speedOverGround = (float32_t)atof((char*)sToken[6]); 
    // Convert speed value from knots to meters per second
    gpsExtraInfo.speedOverGround = ((gpsExtraInfo.speedOverGround) * (KNOTS_MPS_FACTOR));
    gpsExtraInfo.isSpeedDataAvailable = true;
    
    //Set the flag
    isGPSSpeedInfoFound = true;
    retStatus = TRUE;
  }
  else if ( count == 0u ) 
  {
    // If the input string is not tokenized 
    isGPSSpeedInfoFound = false;
  }
  else
  {
     // If the checksum fails as well as the tokenization of the data fails
    isGPSSpeedInfoFound = false;
  }
  return retStatus;
}

//==============================================================================
//
//   ExtractGPGSVData(uint8_t ReadBuffer[],uint32_t startIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2019/05/21   
//
//!  This function Extracts GPGSV data from NMEA string 
//
//==============================================================================
static BOOLEAN ExtractGPGSVData(
                                uint8_t ReadBuffer[],  //!< data received from the GPS receiver
                                uint32_t startIndex,   //!< Start index of the message
                                uint8_t messageLength  //!< Message length
                                  )
{
  int8_t sToken[ROWS_COUNT_FOR_TOKENIZED_ARRAY][COLUMNS_COUNT_FOR_TOKENIZED_ARRAY];
  uint32_t sentenceCount = 0u;
  BOOLEAN vaildateCheckSum = false;
  BOOLEAN retStatus = FALSE;
  uint32_t SatteliteCount = 0;
  vaildateCheckSum = ValidateCheckSum(ReadBuffer, startIndex-6u);
  if( vaildateCheckSum == true )
  {
    //Clear the token array prior to populating 
    memset(sToken,0,375);
    //Tokenize the parsed string keeping the parsed delimeter as tokenizing reference
    (void)TokenizeString(&ReadBuffer[startIndex], sToken, ',',messageLength - 5);
    
    //After tokenizing acquire the data, we will be using SNR only so ignoring all the other information
    //SNR can range from 0 to 99 and has units of dB according to the NMEA standard
    
    //GSV- Sentence formulation
    //1st Byte: Number of sentences for full data
    //2nd Byte: Sentence 1 of 2
    sentenceCount = (int32_t)atoi((char*)sToken[1u]);
    if (sentenceCount == 1)
    {
      //3rd Byte: Number of satellites in view
      SatteliteCount = (int32_t)atoi((char*)sToken[2u]);
      //4th Byte: Satellite PRN number
      //5th Byte: Elevation, degrees
      //6th Byte: Azimuth, degrees
      //7th Byte: SNR - higher is better
      //gpsExtraInfo.CurrentSNR = (int32_t)atoi((char*)sToken[6u]);
      if (1 <= SatteliteCount)
      ((uint8_t *)&gpsExtraInfo.CurrentSNR)[0] = (uint8_t) atoi((char*)sToken[6u]);
      if (2 <= SatteliteCount)
      ((uint8_t *)&gpsExtraInfo.CurrentSNR)[1] = (uint8_t) atoi((char*)sToken[10u]);
      if (3 <= SatteliteCount)
      ((uint8_t *)&gpsExtraInfo.CurrentSNR)[2] = (uint8_t) atoi((char*)sToken[14u]);
      if (4 <= SatteliteCount)
      ((uint8_t *)&gpsExtraInfo.CurrentSNR)[3] = (uint8_t) atoi((char*)sToken[18u]);
      //8th Byte: Satellites per sentence
      //9th Byte: The checksum data, always begins with *
      
      GPSDebugPrint("*******************************************************\r\n");
      GPSDebugFlush();
      GPSDebugPrint("\r\nGPS SNR is : %d, %d, %d, %d from Sentence No %d, Sattelites Caught %d\r\n",((uint8_t *)& gpsExtraInfo.CurrentSNR)[0],((uint8_t *)& gpsExtraInfo.CurrentSNR)[1],((uint8_t *)& gpsExtraInfo.CurrentSNR)[2],((uint8_t *)& gpsExtraInfo.CurrentSNR)[3], sentenceCount,SatteliteCount);
      GPSDebugFlush();
      GPSDebugPrint("\n*******************************************************\n");
      GPSDebugFlush();
    }
//    else
//    {
//      
//      GPSDebugPrint("\n ********GPS SNR SENTENCE : %d , SNR = %d \n", sentenceCount, (int32_t)atoi((char*)sToken[6u]));
//      GPSDebugFlush();
//    }
  }
  return retStatus;
}

//------------------------------------------------------------------------------
//  void JSONGpsDataConversion(float *latitude, float *longitude, char *latitudeDirection, char *longitudeDirection)
//
//   Author:  Dilawar Ali
//   Date:    2018/04/20
//
//!  This function convert gps data received from settelite to gps data needed to iNet
//
//------------------------------------------------------------------------------
void GpsDataConversion( GPSFullInfo_t *ptrGPSFullInfo,GPSInfo_t *ptrGPSInfo)
{
    int localLatitude,localLongitude;
    float localF, localL;
    localF = (ptrGPSFullInfo->gpsInfo.latitude)/100.0f;
    localL = (ptrGPSFullInfo->gpsInfo.longitude)/100.0f;
    localLatitude = (int) localF;
    localLongitude = (int) localL;
    float temp1 = (localF - (float)localLatitude) * (100.0/60.0);
    float temp2 = (localL - (float)localLongitude) * (100.0/60.0);
        
    ptrGPSInfo->latitude = (float)(localLatitude + temp1);
    ptrGPSInfo->longitude = (float)(localLongitude + temp2);
    // Conversion added
    if( ptrGPSFullInfo->latitudeDir == 'S' )
    {
        // Convert latitude
        ptrGPSInfo->latitude =  ( (ptrGPSInfo->latitude) * (-1) );
    }
    // Check logitude Direction
    if( ptrGPSFullInfo->longitudeDir == 'W' )
    {
        // Convert Longitude sign
        ptrGPSInfo->longitude =  ( (ptrGPSInfo->longitude) * (-1) );
    }
    ptrGPSInfo->horizantalDilution = ptrGPSFullInfo->gpsInfo.horizantalDilution;
}

//==============================================================================
//
//   void GPSSleepCommand(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/07/03   
//
//!  This function Sends GPS to Sleep mode   
//
//==============================================================================
void GPSSleepCommand(void)
{
  GPSWrite((uint8_t *)CMD_GPS_GOTO_SLEEP,CMD_GPS_GOTO_SLEEP_LENGTH);
}

//==============================================================================
//
//  void BufferCopy(int8_t* destination, int8_t* source, int8_t size)
//
//   Author:   Tayyab Tahir
//   Date:     2018/06/06   
//
//!  This function Extracts GPRMC and GNRMC  data from NMEA string  
//
//==============================================================================
void BufferCopy(uint8_t* destination, uint8_t* source, uint8_t size)
{
  uint8_t counter;
  
  for(counter=0; counter< size; counter++)
  {
    destination[counter] = source[counter];
  }
}

void GPSUARTRXEnable(void)
{
    UART_control(gpsUartHandle, UART_CMD_RXENABLE, NULL);
}

void GPSUARTRXDisable(void)
{
    UART_readCancel(gpsUartHandle);
    UART_control(gpsUartHandle, UART_CMD_RXDISABLE, NULL);
}

//==============================================================================
//
//  uint32_t GetGpsSnr(void)
//
//   Author:   Tayyab Tahir
//   Date:     2019/02/03   
//
//!  This function gets the latest value of SNR from the GPS if caught 
//
//==============================================================================
uint32_t GetGpsSnr(void)
{
  uint32_t retStatus = 0;
  
  //  if (99 < gpsExtraInfo.CurrentSNR)
  //  {
  //    retStatus = 0;
  //  }
  //  else
  //  {
  retStatus = gpsExtraInfo.CurrentSNR;
  //  }
  return retStatus;
}

void SendEventToGPSStateMachine(GPS_EVENT_t gpsEvt)
{
    SysMsg_t sysTaskEvent;
    sysTaskEvent.msgId = GPS_EVENT;
    sysTaskEvent.msgInfo = (uint16_t)gpsEvt;
    Mailbox_post(sysMailboxHandle, &sysTaskEvent, BIOS_NO_WAIT);    
}