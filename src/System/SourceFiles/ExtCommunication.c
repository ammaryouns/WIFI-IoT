//==============================================================================
//
//  ExtCommunication.c
//
//  Copyright (C) 2014 by Industrial Scientific.
//
//  This document and all  contained within are confidential and
//  proprietary property of Industrial Scientific Corporation. All rights 
//  reserved. It is not to be reproduced or reused without the prior approval 
//  of Industrial Scientific Corporation.
//
//==============================================================================
//  FILE 
//==============================================================================
//
//  Source:        ExtCommunication.c
//
//  Project:       Cornell
//
//  Author:        Dilawar Ali
//
//  Date:          2018/04/23
//
//  Revision:      1.0
//
//==============================================================================
//  FILE DESCRIPTION
//==============================================================================
//
//! \file
//! This file contains the prototypes of global functions and declaration of global 
//! data used in External Communication module. This module provides support for external
//! communication e.g json creation and response parsing.
//


//==============================================================================
//  INCLUDES
//==============================================================================
#include <ti/devices/cc32xx/inc/hw_types.h>
#include <ti/devices/cc32xx/driverlib/prcm.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <Math.h>
#include "ExtCommunication.h"
#include "SysDef.h"
#include "EventQ.h"
#include "jsmn.h"
#include "SPI_Comm.h"
#include "WifiTask.h"
#include "SysTask.h"

//==============================================================================
//  CONSTANTS, TYPEDEFS AND MACROS 
//==============================================================================

//==============================================================================
//  LOCAL DATA DECLARATIONS
//==============================================================================

//==============================================================================
//  LOCAL FUNCTION PROTOTYPES
//==============================================================================
static int32_t JSONCreateTokenAccess(uint8_t urlBuffer[], uint32_t urlBufferSize, uint8_t dataBuffer[], uint32_t dataBufferSize, PTR_EVENT_t evt);
static int32_t JSONCreateInstrumentDataUpload(uint8_t urlBuffer[], uint32_t urlBufferSize, uint8_t dataBuffer[], uint32_t dataBufferSize, PTR_EVENT_t evt);
static int32_t JSONCreateInstrumentRegister(uint8_t urlBuffer[], uint32_t urlBufferSize, uint8_t dataBuffer[], uint32_t dataBufferSize, PTR_EVENT_t evt);
static int32_t JParseGetToken(uint8_t js_data[], uint32_t len, uint8_t tokenBuffer[], uint32_t tokenBufferLength, PTR_EVENT_t evt);
static int32_t JParseInstrumentRegister(uint8_t js_data[], uint32_t len, uint8_t ResponseBuffer[], uint32_t ResponseBufferLength, PTR_EVENT_t evt);
static int32_t JParseInstrumentDataUpload(uint8_t js_data[], uint32_t len, uint8_t ResponseBuffer[], uint32_t ResponseBufferLength, PTR_EVENT_t evt);



//==============================================================================
//  GLOBAL DATA DECLARATIONS
//==============================================================================
JasonCreatorAndParser_t jsonCreatorAndParser[LAST_IINVALID_EVENT] = 
{
    {
        JSONCreateTokenAccess,
        JParseGetToken,
    },
    {
        JSONCreateInstrumentRegister,
        JParseInstrumentRegister,
    },
    {
        JSONCreateInstrumentDataUpload,
        JParseInstrumentDataUpload,
    },
    {
        JSONCreateInstrumentDataUpload,
        JParseInstrumentDataUpload,
    }
};
extern CloudInfo_t httpCloudInfo;
//==============================================================================
//  LOCAL FUNCTIONS IMPLEMENTATION
//==============================================================================

//------------------------------------------------------------------------------
//  int32_t JSONCreateTokenAccess(uint8_t urlBuffer[], uint32_t urlBufferSize, uint8_t dataBuffer[], uint32_t dataBufferSize)
//
//   Author:  Dilawar Ali
//   Date:    2018/04/20
//
//!  This function create JSON to get Token from iNet server
//
//------------------------------------------------------------------------------
static int32_t JSONCreateTokenAccess(uint8_t urlBuffer[], uint32_t urlBufferSize, uint8_t dataBuffer[], uint32_t dataBufferSize, PTR_EVENT_t evt)
{
    int32_t ret = -1;
    //Update URL buffer of communication interface
    ret = snprintf((char *)urlBuffer, urlBufferSize, "/oauth2/endpoint/iNet/token\0");
    // Update data buffer of communication with event correspoding JSON data
    ret = snprintf((char *)dataBuffer, dataBufferSize, "grant_type=password&client_id=%s&client_secret=%s&username=%s&password=%s\0",
                                                        CLIENT_ID,
                                                        CLIENT_SECRET,
                                                        httpCloudInfo.CloudUserID,
                                                        httpCloudInfo.CloudUserPassword
                                                        );
    
    return ret;
}

//------------------------------------------------------------------------------
//  static int32_t JSONCreateInstrumentDataUpload(uint8_t urlBuffer[], uint32_t urlBufferSize, uint8_t dataBuffer[], uint32_t dataBufferSize)
//
//   Author:  Dilawar Ali
//   Date:    2018/04/20
//
//!  This function create JSON For Instrument Join and Sensor Update event
//
//------------------------------------------------------------------------------
static int32_t JSONCreateInstrumentDataUpload(uint8_t urlBuffer[], uint32_t urlBufferSize, uint8_t dataBuffer[], uint32_t dataBufferSize, PTR_EVENT_t evt)
{
    uint8_t time[30] = {0};
    int32_t size = -1;
    uint32_t index = 1, loopCounter = 0;
    
    uint8_t gasCode[10] = {0};
    uint8_t sensorCode[10] = {0};
    float gasReading = 0;
    int16_t gasUnitReading = 0;
    PTR_COMM_EVT_t commEvt = (PTR_COMM_EVT_t)evt;
    
    if(DEFAULT_YEAR_STARTUP == commEvt->dateTimeInfo.date.year)
    {
      //If events are created prior to SNTP resolve then update their time stamp  
      GetCurrentTimeAndDate(&(((PTR_COMM_EVT_t)commEvt)->dateTimeInfo));
    }
    //Buffer for sensor details
    uint8_t sensorsdata[Max_LEN_FOR_SENSOR_JSON * MAX_NUMBER_OF_SENSORS] = {0};
    
    //Update URL buffer of communication interface
    
    size = snprintf((char *)urlBuffer, urlBufferSize, "/iNetAPI/v1/live/create\0");

    //Update time buffer from event time
    snprintf((char *)time, 30, "%02u-%02u-%02uT%02u:%02u:%02u.000+0000", commEvt->dateTimeInfo.date.year, commEvt->dateTimeInfo.date.month, commEvt->dateTimeInfo.date.day, commEvt->dateTimeInfo.time.hours, commEvt->dateTimeInfo.time.minutes, commEvt->dateTimeInfo.time.seconds);



    sensorsdata[0] = '[';
    
    for(loopCounter = 0; loopCounter < commEvt->instSensorInfo.numberOfSensors; loopCounter++)
    {
      gasUnitReading = 0;
      // Get gas code of sensor
      sprintf((char *)gasCode, "G%04u", commEvt->instSensorInfo.sensorArray[loopCounter].SensorType);
      // Get Component code of server
      commEvt->instSensorInfo.sensorArray[loopCounter].componentCode = (commEvt->instSensorInfo.sensorArray[loopCounter].SensorMeasuringUnits) >> 4;
      sprintf((char *)sensorCode, "S%04u", commEvt->instSensorInfo.sensorArray[loopCounter].componentCode);
      
      gasUnitReading |= (int16_t)commEvt->instSensorInfo.sensorArray[loopCounter].SensorReadingHigh;
      gasUnitReading |= gasUnitReading << 8;
      gasUnitReading |= (int16_t)commEvt->instSensorInfo.sensorArray[loopCounter].SensorReadingLow;
      
      gasReading = (float)((gasUnitReading)/(pow(10, commEvt->instSensorInfo.sensorArray[loopCounter].DecimalPlaces)));
      
      // Update buffer with sensors details in JSON format
//      index += sprintf((char *)&sensorsdata[index], "{\"componentCode\":\"%s\",\"gasCode\":\"%s\",\"uom\":%u,\"status\":%u,\"gasReading\":%2.4f},\0",sensorCode, gasCode, \
//        commEvt->instSensorInfo.sensorArray[loopCounter].SensorMeasuringUnits, commEvt->instSensorInfo.sensorArray[loopCounter].SensorStatus, gasReading);
      index += sprintf((char *)&sensorsdata[index], "{\"gasCode\":\"%s\",\"uom\":%u,\"status\":%u,\"gasReading\":%2.4f},\0", gasCode, \
        commEvt->instSensorInfo.sensorArray[loopCounter].SensorMeasuringUnits, commEvt->instSensorInfo.sensorArray[loopCounter].SensorStatus, gasReading);

    }
    
    if (commEvt->instSensorInfo.numberOfSensors > 0)
    {
      // Remove last ','
        index--;
    }
    
    sensorsdata[index] = ']';
    
    // Update buffer with instrument details
    size = snprintf((char *)dataBuffer, dataBufferSize, "{\"device\":\"Guest WiFi\",\"sn\":\"%s\",\"time\":\"%s\",\"sequence\":%d,\"status\":%d,\"equipmentCode\": \"VPRO\",\"user\":\"%.*s\",\"site\":\"%.*s\"", RemoteUnit.SerialNumber, time, commEvt->sequenceNumber, commEvt->InstrumentState,USER_SITE_NAME_LENGTH, commEvt->WorkerInfo.UserName, USER_SITE_NAME_LENGTH, commEvt->WorkerInfo.SiteName);
    
    // Add GPS position in JSON Data
    size += snprintf((char *)&dataBuffer[size], (dataBufferSize - size), ",\"position\":{\"latitude\":%4.6f,\"longitude\":%4.6f,\"accuracy\":%4.6f}", commEvt->GPSLocationInfo.latitude, commEvt->GPSLocationInfo.longitude,commEvt->GPSLocationInfo.horizantalDilution);

    // Add Sensor Data in jSON data
    size += snprintf((char *)&dataBuffer[size], (dataBufferSize - size), ",\"sensors\":%s}\0", sensorsdata);
    
    return size;
}

//------------------------------------------------------------------------------
//  static int32_t JSONCreateInstrumentRegister(uint8_t urlBuffer[], uint32_t urlBufferSize, uint8_t dataBuffer[], uint32_t dataBufferSize)
//
//   Author:  Dilawar Ali
//   Date:    2018/04/20
//
//!  This function create JSON For Instrument Join and Sensor Update event
//
//------------------------------------------------------------------------------
static int32_t JSONCreateInstrumentRegister(uint8_t urlBuffer[], uint32_t urlBufferSize, uint8_t dataBuffer[], uint32_t dataBufferSize, PTR_EVENT_t evt)
{
    int32_t size = -1;
    //Update URL buffer of communication interface
    size = snprintf((char *)urlBuffer, urlBufferSize, "/iNetAPI/v1/live/%s/register", RemoteUnit.SerialNumber);
    
    return size;
}

//------------------------------------------------------------------------------
//  void UpdateRTCTimeFromSNTP(unsigned long timeStamp)
//
//   Author:  Dilawar Ali
//   Date:    2018/04/23
//
//!  This function Update RTC Clock time from SNTP server
//
//------------------------------------------------------------------------------
void UpdateRTCTimeFromSNTP(unsigned long timeStamp)
{
//    timeStamp -= SECOND_SINCE_1970;
    PRCMRTCSet(timeStamp, 0);
}

//------------------------------------------------------------------------------
//  void GetCurrentTimeAndDate(void)
//
//   Author:  Dilawar Ali
//   Date:    2018/04/23
//
//!  This function Get current time from RTC Clock
//
//------------------------------------------------------------------------------
void GetCurrentTimeAndDate(DateTimeInfo_t *currentTime)
{
    unsigned long secondsSince1970 = 0u;
    unsigned short msOffset = 0u;
    PRCMRTCGet(&secondsSince1970, &msOffset);
    
    struct tm *tms;
    tms = gmtime((time_t*)&secondsSince1970);
    
    // Synchronize system clock
    currentTime->date.month     = (uint8_t)(tms->tm_mon + 1); // struct tm monthId(0-11)
    currentTime->date.day       = (uint8_t)tms->tm_mday;
    currentTime->date.year      = (uint16_t)(tms->tm_year + 1900);
    currentTime->time.hours     = (uint8_t)tms->tm_hour;
    currentTime->time.minutes   = (uint8_t )tms->tm_min;
    currentTime->time.seconds   = (uint8_t )tms->tm_sec;
    
}

//------------------------------------------------------------------------------
//  static int32_t JParseGetToken(uint8_t js_data[], uint32_t len, uint8_t tokenBuffer[], uint32_t tokenBufferLength, PTR_COMM_EVT_t commEvt);
//
//   Author:  Dilawar Ali
//   Date:    2018/04/23
//
//!  This function Parse server response incase of Token event and update token buffer with valid received token
//
//------------------------------------------------------------------------------
static int32_t JParseGetToken(uint8_t js_data[], uint32_t len, uint8_t tokenBuffer[], uint32_t tokenBufferLength, PTR_EVENT_t evt)
{
    jsmn_parser js;
    jsmntok_t js_tok[MAX_JSON_TOKEN_NUM];
    jsmn_init(&js);
    uint32_t size=0;
    int32_t status = -1;

    int numOfToken=(sizeof(js_tok)/sizeof(js_tok[0]));

    status = jsmn_parse(&js, (char const*)js_data, len, js_tok, numOfToken);
    if( status > 0)
    {
        for(uint32_t i=1; i<status;++i)
        {
            size=(js_tok[i].end-js_tok[i].start);
            if((0==strncmp((char const*)&js_data[js_tok[i].start],"access_token",size)) && (size !=0))
            {
                size=(js_tok[i+1].end-js_tok[i+1].start);
                snprintf((char *)tokenBuffer,tokenBufferLength,"Bearer %.*s",size,(char *)&js_data[js_tok[i+1].start]);
            }
            else
            {
                //Do Nothing
            }
        }
    }
    else
    {
        status = -1;
    }
    return status;
}

//------------------------------------------------------------------------------
//  static int32_t JParseInstrumentRegister(uint8_t js_data[], uint32_t len, uint8_t ResponseBuffer[], uint32_t ResponseBufferLength, PTR_COMM_EVT_t commEvt)
//
//   Author:  Dilawar Ali
//   Date:    2018/04/23
//
//!  This function Parse server response incase of Token event and update token buffer with valid received token
//
//------------------------------------------------------------------------------
static int32_t JParseInstrumentRegister(uint8_t js_data[], uint32_t len, uint8_t ResponseBuffer[], uint32_t ResponseBufferLength, PTR_EVENT_t evt)
{
    return 0;
}

//------------------------------------------------------------------------------
//  static int32_t JParseInstrumentDataUpload(uint8_t js_data[], uint32_t len, uint8_t ResponseBuffer[], uint32_t ResponseBufferLength, PTR_COMM_EVT_t commEvt)
//
//   Author:  Dilawar Ali
//   Date:    2018/04/23
//
//!  This function Parse server response incase of Token event and update token buffer with valid received token
//
//------------------------------------------------------------------------------
static int32_t JParseInstrumentDataUpload(uint8_t js_data[], uint32_t len, uint8_t ResponseBuffer[], uint32_t ResponseBufferLength, PTR_EVENT_t evt)
{
  PTR_COMM_EVT_t ptrComEvt = (PTR_COMM_EVT_t)evt;
  
  switch( ptrComEvt->commEvtType )
  {
  case INSTRUMENT_SHUTDOWN_EVENT:
    break;
  default:
    break;
  }
  
  return 0;
}

int32_t ParseResponce(uint8_t js_data[], uint32_t len, uint8_t ResponseBuffer[], uint32_t ResponseBufferLength, PTR_EVENT_t evt)
{
    if(EVENT_COMM_EVT == evt->id)
    {
        jsonCreatorAndParser[((PTR_COMM_EVT_t)evt)->commEvtType].jParser(js_data, len, NULL, NULL, evt);
    }
    else if(EVENT_SYS_EVT == evt->id)
    {
        
    }
    else if(EVENT_MSG_EVT == evt->id)
    {
        //Handel Return Value Here 
    }
    else
    {
        //Invalid event
    }
    
    return 0;
}
