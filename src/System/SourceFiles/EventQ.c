
//==============================================================================
//
//  Event.c
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
//! This module takes care of the system watch dog
//
//==============================================================================
//  REVISION HISTORY
//==============================================================================
//  Revision: 1.0  
//      
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================

#include "SysDef.h"
#include "EventQ.h"

#define HEAP_BLOCK_SIZE_ANY             0x0u
#define HEAP_BLOCK_ALLIGNE_BYTE         0x1u

#define HEAP_SIZE_FOR_COM_EVT           1
#define HEAP_SIZE_FOR_SYS_EVT           10
#define HEAP_SIZE_FOR_MSG_EVT           120

//memmory poll handler
HeapMultiBuf_Handle heapHandle;

//Error Block nedded for TiRTOS
Error_Block heapEB;

//SequenceNumkber Counter 
uint8_t sequenceNumber = 0;

//QUEUE Handler for Events
QueueInfo_t eventQueueInfo;

//QUEUE Handler For FirmWare
QueueInfo_t SysQueueInfo;

//QUEUE Handler For FirmWare
QueueInfo_t firmQueueInfo;

#pragma data_alignment = 8
static uint8_t commEvntBuffer[(sizeof(ComEvent_t) * HEAP_SIZE_FOR_COM_EVT)];
#pragma data_alignment = 8
static uint8_t sysEvntBuffer[(sizeof(SysEvent_t) * HEAP_SIZE_FOR_SYS_EVT)];
#pragma data_alignment = 8
static uint8_t msgEvntBuffer[(sizeof(MsgEvent_t) * HEAP_SIZE_FOR_MSG_EVT)];

Queue_Handle InitializeQueue(void);

void CreatMemmoryPoleForQueue(void)
{
  
  HeapBuf_Params bufParams[3]; 
  HeapMultiBuf_Params heapParameter;
  
  HeapBuf_Params_init(&bufParams[0]);
  HeapBuf_Params_init(&bufParams[1]);
  HeapBuf_Params_init(&bufParams[2]);
  
  HeapMultiBuf_Params_init(&heapParameter);
  heapParameter.blockBorrow =  false;
  heapParameter.numBufs = 3;
  
  heapParameter.bufParams = bufParams;
  
  heapParameter.bufParams[0].blockSize = sizeof(ComEvent_t);
  heapParameter.bufParams[0].buf = commEvntBuffer;
  heapParameter.bufParams[0].numBlocks = HEAP_SIZE_FOR_COM_EVT;
  
  heapParameter.bufParams[1].blockSize = sizeof(SysEvent_t);
  heapParameter.bufParams[1].buf = sysEvntBuffer;
  heapParameter.bufParams[1].numBlocks= HEAP_SIZE_FOR_SYS_EVT;
  
  heapParameter.bufParams[2].blockSize = sizeof(MsgEvent_t);
  heapParameter.bufParams[2].buf = msgEvntBuffer;
  heapParameter.bufParams[2].numBlocks = HEAP_SIZE_FOR_MSG_EVT;
  
  //heapParameter.
  heapHandle = HeapMultiBuf_create(&heapParameter,&heapEB); 
  
  eventQueueInfo.queueHandle = InitializeQueue();
  SysQueueInfo.queueHandle = InitializeQueue();
  firmQueueInfo.queueHandle = InitializeQueue();
  
}

uint8_t GetNextSequence(void)
{
  return sequenceNumber++;
}

uint8_t GetTotalElementsInQueue(PTR_QUEUE_INFO_t queueInfoPtr)
{
  return queueInfoPtr->eventsInQueue;
}

PTR_EVENT_t AllocateEvent(SizeT size)
{
  return HeapMultiBuf_alloc( heapHandle,size,HEAP_BLOCK_ALLIGNE_BYTE,&heapEB); 
}

void FreeEvent(PTR_EVENT_t ptrToEvnt)
{
  HeapMultiBuf_free(heapHandle,ptrToEvnt,HEAP_BLOCK_ALLIGNE_BYTE); 
}

Queue_Handle InitializeQueue(void)
{
  return Queue_create(NULL, NULL);
}

void PutOneEventToQueueTail(PTR_QUEUE_INFO_t queueInfoPtr,PTR_EVENT_t ptrToEvent)
{
  //TODO:Lock Here @ammar
  Queue_put(queueInfoPtr->queueHandle, (Queue_Elem *)ptrToEvent);
  ++queueInfoPtr->eventsInQueue;
  //TODO:UnLock Here @ammar
}

void PutOneEventToQueueHead(PTR_QUEUE_INFO_t queueInfoPtr,PTR_EVENT_t ptrToEvent)
{
  //TODO:Lock Here @ammar
  Queue_putHead(queueInfoPtr->queueHandle, (Queue_Elem *)ptrToEvent);
  ++queueInfoPtr->eventsInQueue;
  //TODO:UnLock Here @ammar
}

PTR_EVENT_t RaedOneEventFromQueue(PTR_QUEUE_INFO_t queueInfoPtr)
{
  PTR_EVENT_t ptrEvent = NULL;
  //TODO:Lock Here @ammar
  if(false == Queue_empty(queueInfoPtr->queueHandle))
  {
    ptrEvent = (PTR_EVENT_t)Queue_head(queueInfoPtr->queueHandle);
  }
  //TODO:UnLock Here @ammar
  return ptrEvent; 
}

PTR_EVENT_t GetOneEventFromQueue(PTR_QUEUE_INFO_t queueInfoPtr)
{
  PTR_EVENT_t ptrEvent = NULL;
  //TODO:Lock Here @ammar
  if(false == Queue_empty(queueInfoPtr->queueHandle))
  {
    ptrEvent = (PTR_EVENT_t)Queue_get(queueInfoPtr->queueHandle);
    --queueInfoPtr->eventsInQueue;
  }
  //TODO:UnLock Here @ammar
  return ptrEvent; 
}

void ReleaseAllEventsFromQueue(PTR_QUEUE_INFO_t queueInfoPtr)
{
  PTR_EVENT_t ptrEvent = NULL;
  
  do
  {
      ptrEvent = GetOneEventFromQueue(queueInfoPtr);
      if(ptrEvent)
      {
          FreeEvent(ptrEvent);
      }
  }
  while(NULL != ptrEvent); 
  
  //Reset sequence number count as well
  sequenceNumber = 0;
}
