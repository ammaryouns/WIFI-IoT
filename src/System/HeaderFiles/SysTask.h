#ifndef __SYSTASK_H
#define __SYSTASK_H

#include <stdbool.h>
#include <stdint.h>
#include "SysDef.h"
#include "SPI_Comm.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus*/

#define SYS_TASK_PRIORITY                1u
#define SYS_TASK_STACK_SIZE              2048
    
#define SYS_MAILBOX_MSG_COUNT            15u
    
#define COM_EVENT_DISPATCH_INTERVAL      30u
#define ADC_EVENT_DISPATCH_INTERVAL      60
    
#define SysDebugPrint(format,...)      SYSTEM_PRINT(ENABLE_SYS_DEBUG_LOG, format, ##__VA_ARGS__)
#define SysDebugFlush()                SYSTEM_FLUSH(ENABLE_SYS_DEBUG_LOG)
  
#define StartupDebugPrint(format,...)  SYSTEM_PRINT(ENABLE_STARTUP_DEBUG_LOG, format, ##__VA_ARGS__)
#define StartupDebugFlush()            SYSTEM_FLUSH(ENABLE_STARTUP_DEBUG_LOG)
  

typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
}TimeInfo_t;

typedef struct
{
    uint8_t day;
    uint8_t month;
    uint16_t year;
}DateInfo_t;

typedef struct
{
    TimeInfo_t time;
    DateInfo_t date;
}DateTimeInfo_t;
    
typedef enum
{
  
  VPRO_SLAVE_EVENT_RECEVIED = 0,
  EVENT_MANAGEMENT_EVENT_RECEVIED,
  POWER_MANAGEMENT_EVENT_RECEVIED,
  DISPATCH_QUEUE_EVENT_TO_COMM_TASK,
  READ_BATTERY_LEVEL_ADC_EVENT,
  GPS_EVENT,

#ifdef MANFACTURING_TEST_CODE
  MANUFACTURING_TEST_REQ,
#endif /*MANFACTURING_TEST_CODE*/  

  EMPTY_EVENT_REQ
}SYS_MSG_ID_t;

typedef enum SYS_TASK_VPRO_STATE_MACHINE_STATES
{
  VPRO_SLAVE_INITIALIZATION_STATE  = 0u,
  VPRO_SLAVE_DATA_RECV_START       = 1u,
  VPRO_SLAVE_DATA_RECV_STOP        = 2u,
  VPRO_SLAVE_CREATE_EVENT          = 3U,
  
}SYS_TASK_VPRO_STATE_MACHINE_STATES_t;

typedef enum SYS_POWER_MANAGMENT_EVENT
{
    POWER_WAKE_UP_EVENT = 0,
    POWER_DEEP_SLEEP_EVENT,
    POWER_SHUTDOWN_EVENT,
    POWER_REBOOT_EVENT,
    
}SYS_POWER_MANAGMENT_EVENT_t;


#ifdef MANFACTURING_TEST_CODE

typedef enum MANUFACTURING_TESTS_t
{
  STOP_TEST                     = 0,//!< This constant is used to stop any ongoing tests
  DATA_FLASH_TEST,                  //!< Resereved for Cellular battery only
  START_GPS_TEST,                   //!< This constant is used to stop any ongoing tests
  LOW_POWER_FREQUENCY_TEST,         //!< Resereved for Cellular battery only
  HIGH_FREQUENCY_TEST,              //!< Resereved for Cellular battery only
  START_40MHZ_CRYSTAL_TEST,         //!< This constant is used to test the crystal
  START_RTC_TEST,                   //!< This constant is used to test the crystal
  WATCH_DOG_TEST,                   //!< This constant is used to test the watch dog reset functionality
}
MANUFACTURING_TESTS_t;

#endif /*MANFACTURING_TEST_CODE*/


typedef struct
{
    MsjSenderInfo_t msgSender;
    SYS_MSG_ID_t msgId;
    uint16_t msgInfo;
    void * ptrData;
}SysMsg_t;

extern Mailbox_Handle sysMailboxHandle;
void SysTaskInit();
void EnteringDeepSleep(void);
void LeavingDeepSleep(void);
void PostRebootEvent(void);
void SendEvntToCommTask(void);

void PeriodicCommEvtDisspatch(uint16_t timeElapsed);
void PeriodicADCEevent(uint16_t timeElapsed);

void PostDeepSleepEventToSysTask(void);
void PostWakeUpEventToSysTask(void);

void UpdateAllTimeStamps(uint32_t LatestTimeStampSince1970);
void ResetInstrumentConnection(INET_STATUS_t inetStatus,SMART_BATTERY_STATUS_t sBatteryStatus);
void PostMfgTestEvent(uint16_t EventType);

#ifdef __cplusplus
}
#endif /* __cplusplus*/ 

#endif /* __SYSTASK_H */