#include "TimerTask.h"
#include "WatchDog.h"
#include "SysDef.h"
#include "EventQ.h"
#include <ti/drivers/Power.h>

#define MAX_PERIODIC_FUNCTION_ALLOWED        10u

static void DispatchPeriodicEvents(void);
static void MonitorShutDownTimeOut(void);
static void TimerTask(void);

static PtrPeriodicFunc timerArray[MAX_PERIODIC_FUNCTION_ALLOWED];

#pragma data_alignment = 8 
uint8_t timerTaskStack[TIMER_TASK_STACK_SIZE];

static BOOLEAN isDispatcherEnabled = false; 
static void TimerTask(void)
{
    while(true)
    {
#ifdef ENABLE_LPDS
        Power_enablePolicy();
#endif /* ENABLE_LPDS */
        
        TimerTaskDebugPrint("TIMER TASK IN SLEEP\r\n");      
        TimerTaskDebugFlush(); 
        
        Task_sleep(TIMER_PERIODIC_INTERVAL *CLOCK_TICKS_PER_SEC);

        if(true == isDispatcherEnabled)
        {
            
            MonitorShutDownTimeOut();
            TimerTaskDebugPrint("TIMER TASK PERIODIC EVT DISPATCH \r\n");      
            TimerTaskDebugFlush();             
            DispatchPeriodicEvents();
        }
        
        TimerTaskDebugPrint("TIMER TASK WD INC\r\n");      
        TimerTaskDebugFlush();
        
        SysWatchDogIncTasksCounter();
        
        TimerTaskDebugPrint("TIMER TASK WD KICK\r\n");      
        TimerTaskDebugFlush(); 
        
        SysWatchDogKick();
    }
}

void TimerTaskInit(void)
{
    static Task_Struct TimerTaskStruct;
    
    Task_Params TimerTaskParams;
    Error_Block TimerTaskEb;
    Error_init(&TimerTaskEb);
    Task_Params_init(&TimerTaskParams);
    TimerTaskParams.stackSize = TIMER_TASK_STACK_SIZE;
    TimerTaskParams.stack = timerTaskStack;
    TimerTaskParams.priority = TIMER_TASK_PRIORITY;
    Task_construct(&TimerTaskStruct,(Task_FuncPtr)TimerTask,&TimerTaskParams,&TimerTaskEb);
}

BOOLEAN RegisterPeriodicFunction(PtrPeriodicFunc ptrFunc)
{
    BOOLEAN isRegister = false;
    for(uint8_t index =0;index<MAX_PERIODIC_FUNCTION_ALLOWED; ++index)
    {
        if(NULL == timerArray[index])
        {
            timerArray[index]=ptrFunc;
            isRegister = true;
            break;
        }
    }
    return isRegister;
}

void startPeriodicEventDIspatcher(void)
{
    isDispatcherEnabled = true;
}
void stopPeriodicEventDIspatcher(void)
{
    isDispatcherEnabled = false;
}

void DispatchPeriodicEvents(void)
{
    
    for(uint8_t index =0;index<MAX_PERIODIC_FUNCTION_ALLOWED; ++index)
    {
        if(NULL != timerArray[index])
        {
            timerArray[index](TIMER_PERIODIC_INTERVAL);
        }
    }
}

static void MonitorShutDownTimeOut(void)
{
    uint32_t currentRTCStamp = 0u;
    uint16_t msOffset = 0u;
    PRCMRTCGet(&currentRTCStamp, &msOffset);
    if(MAX_INST_COMM_INTERVAL  < (currentRTCStamp - InstrumentInfo.lastSlaveCommunictionTimeStamp) )
    {
        TimerTaskDebugPrint("TIMER TASK Shutdown Message posted\r\n");      
        TimerTaskDebugFlush(); 
        PostDeepSleepEventToSysTask();
        InstrumentInfo.lastSlaveCommunictionTimeStamp = currentRTCStamp;
    }
}

BOOLEAN GetDispacherState(void)
{
    return isDispatcherEnabled;
}