#include "WatchDog.h"
#include "SysDef.h"
#include "PowerManagement.h"


#define WATCH_DOG_BASE_RESOLUTION_15_S       1
#define WATCH_DOG_SCALE_FACTOR_ONE_MINUTE    WATCH_DOG_BASE_RESOLUTION_15_S*4
#define WATCH_DOG_SCALE_FACTOR_TWO_MINUTE    WATCH_DOG_SCALE_FACTOR_ONE_MINUTE*2

#define WATCH_DOG_WIFI_MAX_THRESHOLD         WATCH_DOG_SCALE_FACTOR_ONE_MINUTE 
#define WATCH_DOG_SYS_TASK_MAX_THRESHOLD     WATCH_DOG_SCALE_FACTOR_ONE_MINUTE 

static BOOLEAN SysWatchDogGetStatus();
void SysAbort(char * str);
void SysExit(int error);

static const uint32_t sysWatchDogMax[WATCH_DOG_LAST_TASK_INDEX]={
                                                                  WATCH_DOG_WIFI_MAX_THRESHOLD,
                                                                  WATCH_DOG_SYS_TASK_MAX_THRESHOLD
                                                                 };

SysWatchDogTaskElement_t sysWatchDogTaskArray[WATCH_DOG_LAST_TASK_INDEX];

static BOOLEAN isInFault = false; 

void SysWatchdogInit(void)
{
    GPIO_setConfig(Board_WATCH_DOG_KICK_PIN,GPIO_CFG_OUT_LOW);
}

void SysWatchDogChangeState(
                         SYS_WATCH_DOG_TASK_INDEX_t TaskIndex,
                         SYS_WATCHDOG_TASK_STATE_t TaskNewState
                         )
{
    sysWatchDogTaskArray[TaskIndex].state = TaskNewState;
}

static BOOLEAN SysWatchDogGetStatus(void)
{
    for(uint8_t LoopCounter = 0u; LoopCounter < WATCH_DOG_LAST_TASK_INDEX; LoopCounter++)
    {
        if(sysWatchDogTaskArray[LoopCounter].count > sysWatchDogMax[LoopCounter])
        {
            WatchDogDebugPrint("WatchDog Reset Will Occur Due To Task :%d\r\n",LoopCounter);      
            WatchDogDebugFlush();
            
            isInFault = true;
            break;
        }
    }
    
#ifdef MANFACTURING_TEST_CODE
    if (TRUE == InstrumentInfo.isWDTTestRunnig)
    {
      //Put system in fault for intended reset
      isInFault = true;
    }
#endif /*MANFACTURING_TEST_CODE*/ 
    
  return isInFault;
}

void SysWatchDogKick(void)
{
    if(false == isInFault)
    {
        WatchDogDebugPrint("WatchDog Test Passed\r\n");      
        WatchDogDebugFlush();

        if(false == SysWatchDogGetStatus())
        { 
            GPIO_write(Board_WATCH_DOG_KICK_PIN,1);
            Task_sleep(1);
            GPIO_write(Board_WATCH_DOG_KICK_PIN,0);
        }
    }
    else
    {
        WatchDogDebugPrint("WatchDog Test Failed\r\n");      
        WatchDogDebugFlush();
    }
}

void SysWatchDogIncTasksCounter(void)
{
  
  for(uint8_t LoopCounter = 0u; LoopCounter < WATCH_DOG_LAST_TASK_INDEX; LoopCounter++)
  {
    if(TASK_STATE_ALIVE == sysWatchDogTaskArray[LoopCounter].state)
    {
      ++(sysWatchDogTaskArray[LoopCounter].count);
    }
  }
}

void SysWatchDogClearCounter(SYS_WATCH_DOG_TASK_INDEX_t taskIndex)
{
  sysWatchDogTaskArray[taskIndex].count = 0;
}

void SysAbort(char * str)
{
  SysExit(0);
}

void SysExit(int error)
{
  isInFault = true;
  while(1)
  {
      SystemReboot();
  }
}
