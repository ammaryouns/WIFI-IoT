//==============================================================================
// Start of file
//==============================================================================
//
//  WifiHttpApp.h
//
//  Copyright (C) 2016 by Industrial Scientific.
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
//  Source:        WifiHttpApp.h
//
//  Project:       Morrison
//
//  Author:        Ammar Younas
//
//  Date:          2017/2/15
//
//  Revision:      1.0
//
//==============================================================================
//  FILE DESCRIPTION
//==============================================================================
//
//! \file
//! This is a Wifi HTPPS module Header.  
//
//==============================================================================
//  REVISION HISTORY
//==============================================================================
//
// Revision: 1.0 2016/12/09 Ammar Younas
//           Initial Version
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#ifndef _WIFI_APP_H_
#define _WIFI_APP_H_
//==============================================================================
//  CONSTANTS, TYPEDEFS AND MACROS 
//==============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <SlHttpCli.h>
#include <WifiTask.h>
#include <picohttpparser.h>
#include "ExtCommunication.h"
#include <SysDef.h>
#include "EventQ.h"

//==============================================================================
//  DATA Structures
//==============================================================================   
typedef enum
{
  CHUNKED=1,
  TRANSFER_CONTENT_LENGTH
}TRANSFER_ENCODING;

typedef enum CONNECTION_TYPE
{
  KEEP_ALIVE=1,
  CLOSE
}CONNECTION_TYPE_t;

typedef struct slHTTPParse
{
  struct phr_header headers[20];
  uint8_t * msg;
  size_t msg_len;
  int32_t status;
  TRANSFER_ENCODING transfer_encoding;
  uint32_t content_length;
  CONNECTION_TYPE_t connection_type;
  int32_t minor_version;
  
}slHTTPParse_t;

//------------------------------------------------------------------------------
//   void TestConnectionWithINET( slHTTPCliStruct_t *httpCli )
//
//   Author:  Ammar Younas
//   Date:    2017/04/5
//   Test Connection with INet Server
//
//------------------------------------------------------------------------------

void TestConnectionWithINET( slHTTPCliStruct_t *httpCli );

void SndQueueEventsToINET(slHTTPCliStruct_t *httpCli,uint8_t eventToSend);

//------------------------------------------------------------------------------
// void HttpInit(slHTTPCliStruct_t *httpCli)
//
//   Author:  Ammar Younas
//   Date:    2017/04/5
//   Wifi HTTP Task
//
//------------------------------------------------------------------------------
void HttpInit(slHTTPCliStruct_t *httpCli);

BOOLEAN WifiGetCommErrorStatus(void);

//==============================================================================
//
//  void UpdateConnectionStaus(void)
//
//   Author:   Tayyab Tahir
//   Date:     2019/01/10
//
//!  This function updates the Instrument connection staus to wifi.
//
//==============================================================================
void UpdateConnectionStaus(void);

void UpdateHTTPHostInfo(CloudInfo_t *cloudInfo);

#endif /*_WIFI_APP_H_*/