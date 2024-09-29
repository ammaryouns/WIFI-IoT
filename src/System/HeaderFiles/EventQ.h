#ifndef __EVENTQ_H

#define __EVENTQ_H

#include <stdbool.h>
#include <SysTask.h>
#include <SPI_Comm.h>
#include <ti/sysbios/knl/Queue.h>
#include "GPS.h"

#define PUT_EVENT_TO_COMM_QUEUE(ptrToEvent)             PutOneEventToQueueTail(&eventQueueInfo,ptrToEvent)
#define PUT_EVENT_TO_COMM_QUEUE_HEAD(ptrToEvent)        PutOneEventToQueueHead(&eventQueueInfo,ptrToEvent)

#define READ_EVENT_FROM_COMM_QUEUE()                    RaedOneEventFromQueue(&eventQueueInfo)
#define GET_EVENT_FROM_COMM_QUEUE()                     GetOneEventFromQueue(&eventQueueInfo)
#define GET_TOTAL_ELEMENTS_IN_COMM_QUEUE()              GetTotalElementsInQueue(&eventQueueInfo)
#define RELEASE_ALL_EVENTS_FROM_COMM_QUEUE()            ReleaseAllEventsFromQueue(&eventQueueInfo)

#define PUT_EVENT_TO_SYS_QUEUE(ptrToEvent)              PutOneEventToQueueTail(&SysQueueInfo,ptrToEvent)
#define PUT_EVENT_TO_SYS_QUEUE_HEAD(ptrToEvent)         PutOneEventToQueueHead(&SysQueueInfo,ptrToEvent)

#define READ_EVENT_FROM_SYS_QUEUE()                     RaedOneEventFromQueue(&SysQueueInfo)
#define GET_EVENT_FROM_SYS_QUEUE()                      GetOneEventFromQueue(&SysQueueInfo)
#define GET_TOTAL_ELEMENTS_IN_SYS_QUEUE()               GetTotalElementsInQueue(&SysQueueInfo)
#define RELEASE_ALL_EVENTS_FROM_SYS_QUEUE()             ReleaseAllEventsFromQueue(&SysQueueInfo)

#define PUT_EVENT_TO_FIRM_QUEUE(ptrToEvent)             PutOneEventToQueueTail(&firmQueueInfo,ptrToEvent)
#define PUT_EVENT_TO_FIRM_QUEUE_HEAD(ptrToEvent)        PutOneEventToQueueHead(&firmQueueInfo,ptrToEvent)

#define READ_EVENT_FROM_FIRM_QUEUE()                    RaedOneEventFromQueue(&firmQueueInfo)
#define GET_EVENT_FROM_FIRM_QUEUE()                     GetOneEventFromQueue(&firmQueueInfo)
#define GET_TOTAL_ELEMENTS_IN_FIRM_QUEUE()              GetTotalElementsInQueue(&firmQueueInfo)
#define RELEASE_ALL_EVENTS_FROM_FIRM_QUEUE()            ReleaseAllEventsFromQueue(&firmQueueInfo)

#define MAX_SENSORS_PER_INSTRUMENT                      7u
#define MAX_ALLOWED_MSG_SIZE                            128u


typedef enum
{
    EVENT_NONE = 0,
    EVENT_COMM_EVT,
    EVENT_SYS_EVT,
    EVENT_MSG_EVT
}EVENT_ID_t;

typedef struct
{
    Queue_Elem elem;
    EVENT_ID_t id;
}Event_t;

typedef enum
{
    REGISTER_INST_ON_INET = 1,
    INSTRUMENT_DATA_UPLOAD,
    INSTRUMENT_SHUTDOWN_EVENT,
    
    LAST_IINVALID_EVENT,
}COMM_EVT_TYPE_t;

typedef enum
{
    SYTEM_FIRMWARE_UPDATE_EVENT = 1,
}SYS_EVT_TYPE_t;

typedef struct
{
    Event_t evtObj;
    COMM_EVT_TYPE_t commEvtType;
    uint8_t sequenceNumber;
    INSTRUMENT_STATUS_t InstrumentState;
    InstSensorInfo_t instSensorInfo;
    Worker_Info_t WorkerInfo;
    GPSInfo_t GPSLocationInfo;
    DateTimeInfo_t dateTimeInfo;
}ComEvent_t;


typedef struct
{
    Event_t evtObj;
    SYS_EVT_TYPE_t sysEvtType;
}SysEvent_t;

typedef struct
{
    Event_t evtObj;
    uint8_t msgSize;
    uint8_t Msg[MAX_ALLOWED_MSG_SIZE];
}MsgEvent_t;

typedef struct
{
    Queue_Handle queueHandle;
    uint8_t eventsInQueue;
    
}QueueInfo_t;

typedef Event_t*  PTR_EVENT_t;
typedef ComEvent_t* PTR_COMM_EVT_t;
typedef SysEvent_t* PTR_SYS_EVT_t;
typedef MsgEvent_t* PTR_MSG_EVT_t;

typedef QueueInfo_t* PTR_QUEUE_INFO_t;

//QUEUE Handler for Events
extern QueueInfo_t eventQueueInfo;

//QUEUE Handler For SysQueue
extern QueueInfo_t SysQueueInfo;

//QUEUE Handler For FirmWare
extern QueueInfo_t firmQueueInfo;

uint8_t GetNextSequence(void);

uint8_t GetTotalElementsInQueue(PTR_QUEUE_INFO_t queueInfoPtr);


PTR_EVENT_t AllocateEvent(SizeT size);
void FreeEvent(PTR_EVENT_t ptrToEvnt);


void PutOneEventToQueueTail(PTR_QUEUE_INFO_t queueInfoPtr,PTR_EVENT_t ptrToEvent);
void PutOneEventToQueueHead(PTR_QUEUE_INFO_t queueInfoPtr,PTR_EVENT_t ptrToEvent);

PTR_EVENT_t RaedOneEventFromQueue(PTR_QUEUE_INFO_t queueInfoPtr);
PTR_EVENT_t GetOneEventFromQueue(PTR_QUEUE_INFO_t queueInfoPtr);
void ReleaseAllEventsFromQueue(PTR_QUEUE_INFO_t queueInfoPtr);

void CreatMemmoryPoleForQueue(void);

#endif /* __EVENT_H */