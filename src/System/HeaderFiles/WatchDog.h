#ifndef __WATCHDOG_H
#define __WATCHDOG_H

#include "SysTask.h"
#include <Board.h>
#include <SysDef.h>
#include <ti/drivers/GPIO.h>


#define WatchDogDebugPrint(format,...)      SYSTEM_PRINT(ENABLE_WATCHDOG_DEBUG_LOG, format, ##__VA_ARGS__)
#define WatchDogDebugFlush()                SYSTEM_FLUSH(ENABLE_WATCHDOG_DEBUG_LOG)


typedef enum
{
  WATCH_DOG_WIFI_TASK_INDEX = 0,
  WATCH_DOG_SYS_TASK_INDEX  = 1, 
  WATCH_DOG_LAST_TASK_INDEX = 2, 
}SYS_WATCH_DOG_TASK_INDEX_t;

typedef enum 
{
  TASK_STATE_INACTIVE = 0,
  TASK_STATE_ALIVE,
  TASK_STATE_PEND,
}SYS_WATCHDOG_TASK_STATE_t;

typedef struct 
{
    SYS_WATCHDOG_TASK_STATE_t state;
    uint32_t count;
}SysWatchDogTaskElement_t;

void SysWatchdogInit(void);
void SysWatchDogChangeState(
                         SYS_WATCH_DOG_TASK_INDEX_t TaskIndex,
                         SYS_WATCHDOG_TASK_STATE_t TaskNewState
                         );
void SysWatchDogClearCounter(SYS_WATCH_DOG_TASK_INDEX_t taskIndex);
void SysWatchDogIncTasksCounter(void);
void SysWatchDogKick(void);

#endif /*__WATCHDOG_H*/