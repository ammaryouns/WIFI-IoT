#ifndef __TIMER_TASK_H_
#define __TIMER_TASK_H_

#include "SysDef.h"

#define TIMER_TASK_PRIORITY             3
#define TIMER_TASK_STACK_SIZE           512u

#define CLOCK_TICKS_PER_SEC             1000
#define TIMER_PERIODIC_INTERVAL         15

#define TimerTaskDebugPrint(format,...)      SYSTEM_PRINT(ENABLE_TIMER_TASK_DEBUG_LOG, format, ##__VA_ARGS__)
#define TimerTaskDebugFlush()                SYSTEM_FLUSH(ENABLE_TIMER_TASK_DEBUG_LOG)

typedef void(*PtrPeriodicFunc)(uint16_t);

void startPeriodicEventDIspatcher(void);
void stopPeriodicEventDIspatcher(void);
BOOLEAN RegisterPeriodicFunction(PtrPeriodicFunc ptrFunc);
void TimerTaskInit(void);
BOOLEAN GetDispacherState(void);

#endif /* __TIMER_TASK_H_ */