#include <ti/drivers/UART.h>
#include <ti/devices/cc32xx/inc/hw_memmap.h>
#include <ti/drivers/Watchdog.h>
#include "SysDef.h"
#include "SysTask.h"
#include "WifiTask.h"
#include "PowerManagement.h"
#include "TimerTask.h"
#include <WifiHTTPClient.h>
#include "GPS.h"
#include "WatchDog.h"
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC32XX.h>
#include "ADC_Channel.h"
#include "EventQ.h"

#ifdef MANFACTURING_TEST_CODE  
#include "ManufacturingTestCodes.h"
#endif /*MANFACTURING_TEST_CODE*/

#define CRITICAL_LOW_BATTERY_VOLTAGE            3350            //. Voltage value is given in mV
#define LOW_BATTERY_VOLTAGE                     3500            //. Voltage value is given in mV

#pragma data_alignment = 8
uint8_t sysTaskStack[SYS_TASK_STACK_SIZE];

Mailbox_Handle sysMailboxHandle;

extern uint32_t check0;
extern uint32_t check1;
extern uint32_t gdeltaTick;
extern uint32_t gconstraintMask;

static void SysTask(void);
static void DispatchMessageToCommunicationTask(void);
void ParseGPSData(UART_Handle handle, void *GPSData, size_t dataLen);

static void InstrumentLowBatteryState(void);
static void InstrumentRunningState(void);

static void SysTask(void)
{   
    StartupDebugPrint("\r\n*************SYSTEM EXECUTION STARTED***********************\r\n");
    StartupDebugFlush();
    GPSInit();
    InitializeIntrumentData();
    (void)ConfigureCSPin();
    (void)SPISlaveConfigure();
    (void)InitializeADC();
    //Create Timer Task for periodic event
    TimerTaskInit();
    //Register peridoic functions
    RegisterPeriodicFunction(&PeriodicCommEvtDisspatch);
    RegisterPeriodicFunction(&PeriodicADCEevent);
    RegisterPeriodicFunction(PostTimeUpdateMessageToWIFI);
    RegisterPeriodicFunction(PostRSSIUpdateMessageToWIFI);
    
#ifdef WIFI_SCAN_AP_TEST_ENABLE
    RegisterPeriodicFunction(DispatchScanMessageToWifiTask);
#endif /*WIFI_SCAN_AP_TEST_ENABLE*/
    
    //Wait for some time to ensure drivers initializtion
    Task_sleep(10);
    //Shutdown the GNSS module at startup
    //ShutDownGNSSystem();
    //Try to get the ADC value after the driver has been initialized
    UpdateBatteryVoltageInfo();

    while(1)
    {
        if( InstrumentInfo.vBatMon > CRITICAL_LOW_BATTERY_VOLTAGE )
        {
            SysDebugPrint("\r\n*************SYSTEM EXECUTION NORMAL***********************\r\n");
            SysDebugFlush();
            //Start Periodic Event Dispatch
            startPeriodicEventDIspatcher();
            InstrumentRunningState();
        }
        else
        {
            SysDebugPrint("\r\n*************SYSTEM EXECUTION LOW BATTERY***********************\r\n");
            SysDebugFlush();
            //Stop Priodic Event Dispatcher
            stopPeriodicEventDIspatcher();
            InstrumentLowBatteryState();
        }
    }
}

static void InstrumentLowBatteryState(void)
{
    SysMsg_t msg;
    
    while(InstrumentInfo.vBatMon < LOW_BATTERY_VOLTAGE)
    {
        //Try to get the ADC value after the driver has been initialized
        UpdateBatteryVoltageInfo();
        
        SysWatchDogChangeState(WATCH_DOG_SYS_TASK_INDEX,TASK_STATE_INACTIVE);
        
        Mailbox_pend(sysMailboxHandle, &msg, TIMER_PERIODIC_INTERVAL*CLOCK_TICKS_PER_SEC);
        
        //Set the Watch dog state to active 
        SysWatchDogChangeState(WATCH_DOG_SYS_TASK_INDEX,TASK_STATE_ALIVE);
        //Clear the watch dog counter for this task 
        SysWatchDogClearCounter(WATCH_DOG_SYS_TASK_INDEX);
    }
}
static void InstrumentRunningState(void)
{
    SysMsg_t msg;
    //Create Wi-FiTask for periodic event
    WifiTaskInit();
    
    while(1)
    {
        
        //Set the Watch dog state to inactive
        SysWatchDogChangeState(WATCH_DOG_SYS_TASK_INDEX,TASK_STATE_INACTIVE);
        
        Mailbox_pend(sysMailboxHandle, &msg, BIOS_WAIT_FOREVER);
        
        //Set the Watch dog state to active 
        SysWatchDogChangeState(WATCH_DOG_SYS_TASK_INDEX,TASK_STATE_ALIVE);
        //Clear the watch dog counter for this task 
        SysWatchDogClearCounter(WATCH_DOG_SYS_TASK_INDEX);
        
        
        //        SysDebugPrint("Chek0: %d\r\n"
        //                      "Chek1: %d\r\n"
        //                      "DeltaTick: %d\r\n"
        //                      "Constraint  :0x%08x\r\n"
        //                      ,check0,check1,gdeltaTick,gconstraintMask);
        //        SysDebugFlush();
        
        switch(msg.msgId)
        {
        case VPRO_SLAVE_EVENT_RECEVIED:
            SysDebugPrint("SYSTASK VPRO EVENT RECIVE\r\n");
            SysDebugFlush();
            VproSlaveStateMachine(msg.msgInfo);
            break;
            
        case EVENT_MANAGEMENT_EVENT_RECEVIED:
            break;
            
        case POWER_MANAGEMENT_EVENT_RECEVIED:
            SysDebugPrint("SYSTASK POWER MANAGMENT EVENT RECIVE, Event Type: %d\r\n ",msg.msgInfo);
            SysDebugFlush();
            PowerManagementStateMachine(msg.msgInfo);
            break;
            
        case DISPATCH_QUEUE_EVENT_TO_COMM_TASK:
            SysDebugPrint("SYSTASK QUEUE CHECK EVENT RECIVE\r\n");
            SysDebugFlush();
            DispatchMessageToCommunicationTask();
            break;
            
        case GPS_EVENT:
            SysDebugPrint("SYSTASK GPS EVENT RECIVE\r\n");
            SysDebugFlush();
            GPSStateMachine(msg.msgInfo);
            break;
        case READ_BATTERY_LEVEL_ADC_EVENT:
            SysDebugPrint("SYSTASK ADC EVENT RECIVE\r\n");
            SysDebugFlush();
            UpdateBatteryVoltageInfo();
            break;
            
#ifdef MANFACTURING_TEST_CODE  
            
        case MANUFACTURING_TEST_REQ:
            SysDebugPrint("MANFACTURING TEST EVENT RECIVED\r\n");
            SysDebugFlush();
            MfgTestRun(msg.msgInfo);
            break;
            
#endif /*MANFACTURING_TEST_CODE*/  
            
        case EMPTY_EVENT_REQ:
            SysDebugPrint("SYSTASK EMPTY EVENT RECIVE\r\n");
            SysDebugFlush();
            break;
            
        default: 
            break;
        }
        
        //Reset the msgId and will be again populated from mail box
        msg.msgId = EMPTY_EVENT_REQ;
    }
}

void DispatchMessageToCommunicationTask(void)
{
    uint8_t EventsInQuene = 0u;
    WifiMsg_t msg;
    if(false == WifiGetCommErrorStatus())
    {
        EventsInQuene = GET_TOTAL_ELEMENTS_IN_COMM_QUEUE();
        if( EventsInQuene != 0u)
        {
            msg.msgSender.sender = MSG_SENDER_SYS_TASK;
            msg.msgId = SEND_MSG_TO_INET;
            msg.msgInfo = EventsInQuene;
            msg.ptrData = NULL;
            Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
        }
    }
    else
    {
        msg.msgSender.sender = MSG_SENDER_SYS_TASK;
        msg.msgId = TEST_CONN_WITH_INET;
        Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
    }
}

void SysTaskInit(void)
{
    static Task_Struct SysTaskStruct;
    static Mailbox_Struct sysMailboxStruct;
    
    Task_Params SysTaskParams;
    Error_Block SysTaskEb;
    Error_init(&SysTaskEb);
    Task_Params_init(&SysTaskParams);
    SysTaskParams.stackSize = SYS_TASK_STACK_SIZE;
    SysTaskParams.stack = &sysTaskStack;
    SysTaskParams.priority = SYS_TASK_PRIORITY;
    Task_construct(&SysTaskStruct,(Task_FuncPtr)SysTask,&SysTaskParams,&SysTaskEb); 
    
    Swi_Params swiParams;
    Swi_Params_init(&swiParams);
    swiParams.arg0 = 1;
    swiParams.arg1 = 0;
    swiParams.priority = 2;
    swiParams.trigger = 0;
    Swi_construct(&swiSPISlaveStruct, (Swi_FuncPtr)swiSPISlaveFxn, &swiParams, NULL);
    swiSPISlaveHandle = Swi_handle(&swiSPISlaveStruct);
    
    Semaphore_Params semParams;
    Semaphore_Params_init(&semParams);
    Semaphore_construct(&semSPISlaveStruct, 0, &semParams);
    semSPISlaveHandle = Semaphore_handle(&semSPISlaveStruct);
    
    Mailbox_Params sysMailboxParams;
    Error_Block sysMailboxErrorBlock;
    uint32_t sysMailboxMsgSize=sizeof(SysMsg_t);
    Mailbox_Params_init(&sysMailboxParams);
    Mailbox_construct(&sysMailboxStruct,sysMailboxMsgSize,SYS_MAILBOX_MSG_COUNT
                      ,&sysMailboxParams, &sysMailboxErrorBlock);
    sysMailboxHandle=Mailbox_handle(&sysMailboxStruct);
    
}

void PeriodicCommEvtDisspatch(uint16_t timeElapsed)
{
    static int8_t timeCounter = COM_EVENT_DISPATCH_INTERVAL;
    timeCounter -= timeElapsed;
    if(timeCounter <= 0)
    {
        SendEvntToCommTask();
        timeCounter = COM_EVENT_DISPATCH_INTERVAL;
    }
}

void SendEvntToCommTask(void)
{
    SysMsg_t msg;
    if(GetWifiState() == WIFI_CONNECTED)
    {
        msg.msgId = DISPATCH_QUEUE_EVENT_TO_COMM_TASK;
        Mailbox_post(sysMailboxHandle, &msg, BIOS_NO_WAIT);
    }
    //Update the connection status of AP & INET
    UpdateConnectionStaus();
}

void PeriodicADCEevent(uint16_t timeElapsed)
{
    SysMsg_t msg;
    static int8_t timeCounter = ADC_EVENT_DISPATCH_INTERVAL;
    timeCounter -= timeElapsed;
    if(timeCounter <= 0)
    {
        //Set the specific state machine to which to post the event 
        msg.msgId = READ_BATTERY_LEVEL_ADC_EVENT;
        Mailbox_post(sysMailboxHandle, &msg, BIOS_NO_WAIT);
        timeCounter = ADC_EVENT_DISPATCH_INTERVAL;
    }
}

void PostRebootEvent(void)
{
    SysMsg_t SysTaskEvent;
    //Set the specific state machine to which to post the event 
    SysTaskEvent.msgId = POWER_MANAGEMENT_EVENT_RECEVIED;
    //Set the mailbox type to be posted
    SysTaskEvent.msgInfo = POWER_REBOOT_EVENT;   
    //Post the mailbox to Systask
    Mailbox_post(sysMailboxHandle, &SysTaskEvent, BIOS_NO_WAIT);
}

void EnteringDeepSleep(void)
{
    //SysDebugPrint("SYSTEM ENTERING SLEEP\r\n");
    //SysDebugFlush();
}

void LeavingDeepSleep(void)
{
    Board_initGeneral();
    ConfigureCSPin();
    //SysDebugPrint("SYSTEM AWAKE\r\n");
    //SysDebugFlush();
}

void PostDeepSleepEventToSysTask(void)
{
    SysMsg_t SysTaskEvent;
    
    SysTaskEvent.msgId = POWER_MANAGEMENT_EVENT_RECEVIED;
    SysTaskEvent.msgInfo = POWER_DEEP_SLEEP_EVENT;   
    //Post the mailbox to Systask
    Mailbox_post(sysMailboxHandle, &SysTaskEvent, BIOS_NO_WAIT);
}

void PostWakeUpEventToSysTask(void)
{
    SysMsg_t SysTaskEvent;
    
    SysTaskEvent.msgId = POWER_MANAGEMENT_EVENT_RECEVIED;
    SysTaskEvent.msgInfo = POWER_WAKE_UP_EVENT;   
    //Post the mailbox to Systask
    Mailbox_post(sysMailboxHandle, &SysTaskEvent, BIOS_NO_WAIT);
}


void UpdateAllTimeStamps(uint32_t LatestTimeStampSince1970)
{
    InstrumentInfo.lastSlaveCommunictionTimeStamp = LatestTimeStampSince1970;
}

void ResetInstrumentConnection(INET_STATUS_t inetStatus,SMART_BATTERY_STATUS_t sBatteryStatus)
{
    InstrumentInfo.InetStatus = inetStatus;
    InstrumentInfo.SmartBatteryStatus = sBatteryStatus;
    
    if(RUNNING_NORMAL != sBatteryStatus)
    {
        InstrumentInfo.wirelessInfo.WirelessRSSI = 0;
    }
}

#ifdef MANFACTURING_TEST_CODE
//==============================================================================
//
//  void PostMfgTestEvent(uint16_t EventType)
//
//   Author:   Tayyab Tahir
//   Date:     2019/01/30
//
//!  This function is used to post message to mailbox.
//
//==============================================================================
void PostMfgTestEvent(uint16_t EventType)
{
    SysMsg_t SysTaskEvent;
    //Set the specific state machine to which to post the event 
    SysTaskEvent.msgId = MANUFACTURING_TEST_REQ;
    //Set the mailbox type to be posted
    SysTaskEvent.msgInfo = EventType; 
    //Post the mailbox to Systask
    Mailbox_post(sysMailboxHandle, &SysTaskEvent, BIOS_NO_WAIT);
}

#endif /*MANFACTURING_TEST_CODE*/