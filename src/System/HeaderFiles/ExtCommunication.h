//==============================================================================
//
//  ExtCommunication.h
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
//  Source:        ExtCommunication.h
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

#ifndef __EXTCOMMUNICATION_H
#define __EXTCOMMUNICATION_H

//==============================================================================
//  INCLUDES
//==============================================================================
#include "Systask.h"
#include "EventQ.h"
//==============================================================================
//  GLOBAL CONSTANTS, TYPEDEFS AND MACROS 
//==============================================================================


#define FIND_MIN(X,Y)                           ( (X) < (Y) ? (X) : (Y) )


#define CLIENT_ID 		                "f171b350-191d-4c56-93ec-b06cfd8e5d1c"
#define CLIENT_SECRET	                        "8b09738f-d519-4790-a8b7-57dbbf596f46"

#define CLOUD_URL		                "inetuploadft.indsci.com"
#define CLOUD_USER_ID		                "inetdevapi"
#define CLOUD_USER_PWD		                "iNetDev!23"

#define CONTENT_TYPE_URL_ENCODED                "application/x-www-form-urlencoded"
#define CONTENT_TYPE_JSON                       "application/json"

#define SOCKET_SND_RCV_TIMEOUT                  8u//.15u

#define SL_HTTP_CERTIFICATE                     "/sys/cert/inetcert.pem"

#define SL_USER_AGENT                           "HTTPCli (ARM; TI-RTOS)"
#define SL_CONTENT_TYPE_JSON                    "application/json;charset=UTF-8"
#define SL_CONTENT_TYPE_URL_ENCODED             "application/x-www-form-urlencoded;charset=UTF-8"
#define SL_SG_CONTENT_TYPE_URL_ENCODED          "application/x-www-form-urlencoded"
#define SL_CONNECTION_TYPE                      "keep-alive"

#define Max_LEN_FOR_SENSOR_JSON                 85
#define MAX_NUMBER_OF_SENSORS	                8u
#define MAX_JSON_TOKEN_NUM                      50
#define MAX_JSON_TOKEN_STRING_SIZE              60
#define INET_SSL_PORT                           443u


#define EVENT_RETRANSMIT_ATTEMPTS               3

#define SL_HTTP_RECV_BUFFER_SIZE                1000u
#define SL_HTTP_DATA_BUFFER_SIZE                1000u
#define SL_HTTP_URL_BUFFER_SIZE                 1000u
#define SL_INT_TO_STRING_BUFFER_SIZE            10u

#define SECOND_SINCE_1970                       2208988800UL
#define GET_INET_TOKEN                          0

#define DEFAULT_YEAR_STARTUP                    1970
typedef int32_t (*FPtrJSONCreator_t)(uint8_t urlBuffer[], uint32_t urlBufferSize, uint8_t dataBuffer[], uint32_t dataBufferSize, PTR_EVENT_t commEvt);
typedef int32_t (*FPtrJSONParser_t)(uint8_t srcBuffer[], uint32_t srcBufferSize, uint8_t distBuffer[], uint32_t distBufferSize, PTR_EVENT_t commEvt);



typedef struct
{
    FPtrJSONCreator_t jCreator;
    FPtrJSONParser_t jParser;
}JasonCreatorAndParser_t;

//==============================================================================
//  GLOBAL DATA
//==============================================================================
extern JasonCreatorAndParser_t jsonCreatorAndParser[LAST_IINVALID_EVENT];

//==============================================================================
//  EXTERNAL OR GLOBAL FUNCTIONS
//==============================================================================

//------------------------------------------------------------------------------
//  void UpdateRTCTimeFromSNTP(unsigned long timeStamp)
//
//   Author:  Dilawar Ali
//   Date:    2018/04/23
//
//!  This function Update RTC Clock time from SNTP server
//
//------------------------------------------------------------------------------
void UpdateRTCTimeFromSNTP(unsigned long timeStamp);

//------------------------------------------------------------------------------
//  void GetCurrentTimeAndDate(void)
//
//   Author:  Dilawar Ali
//   Date:    2018/04/23
//
//!  This function Get current time from RTC Clock
//
//------------------------------------------------------------------------------
void GetCurrentTimeAndDate(DateTimeInfo_t *currentTime);

int32_t ParseResponce(uint8_t js_data[], uint32_t len, uint8_t ResponseBuffer[], uint32_t ResponseBufferLength, PTR_EVENT_t evt);

#endif // __EXTCOMMUNICATION_H