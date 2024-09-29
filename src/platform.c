#include <ti/drivers/net/wifi/simplelink.h>

#define BIT_WIFI_IP_ACCUIRED             ((uint8_t)(0x01u<<0u))
#define BIT_WIFI_CONNECTED               ((uint8_t)(0x01u<<1u))

#define WIFI_LINK_UP                 ((uint8_t)(BIT_WIFI_IP_ACCUIRED|BIT_WIFI_CONNECTED))

#include "SysDef.h"
#include "WifiTask.h"

#define NWP_TASK_PRIORITY                   2u
#define NWP_TASK_STACK_SIZE                 2048u

#pragma data_alignment = 8
uint8_t NWPTaskStack[NWP_TASK_STACK_SIZE];

uint8_t wifiLinkStatus = 0x00u;

//Function is availble in SysTask.
void ResetInstrumentConnection(INET_STATUS_t inetStatus,SMART_BATTERY_STATUS_t sBatteryStatus);

void* sl_Task(void* pEntry);
void CreateNWPTask(void)
{
    static Task_Struct NWPTaskStruct;
    Error_Block NWPTaskEb;
    Error_init(&NWPTaskEb);
    
    Task_Params NWPTaskParams;
    Task_Params_init(&NWPTaskParams);
    NWPTaskParams.stackSize = NWP_TASK_STACK_SIZE;
    NWPTaskParams.stack = &NWPTaskStack;
    NWPTaskParams.priority = NWP_TASK_PRIORITY;
    Task_construct(&NWPTaskStruct,(Task_FuncPtr)sl_Task,&NWPTaskParams,&NWPTaskEb);
}
/*!
    \brief          SimpleLinkNetAppEventHandler

    This handler gets called whenever a Netapp event is reported
    by the host driver / NWP. Here user can implement he's own logic
    for any of these events. This handler is used by 'network_terminal'
    application to show case the following scenarios:

    1. Handling IPv4 / IPv6 IP address acquisition.
    2. Handling IPv4 / IPv6 IP address Dropping.

    \param          pNetAppEvent     -   pointer to Netapp event data.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 5.7

*/
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{

    WifiMsg_t wifiMsg={MSG_SENDER_NWP,DHCP_LEASE_MSG,NULL};
    
    switch(pNetAppEvent->Id)
    {
    case SL_NETAPP_EVENT_IPV4_ACQUIRED:
    case SL_NETAPP_EVENT_IPV6_ACQUIRED:
        wifiLinkStatus |= BIT_WIFI_IP_ACCUIRED;
        Mailbox_post(wifiMailboxHandle,&wifiMsg,BIOS_NO_WAIT);
        if(WIFI_LINK_UP == wifiLinkStatus)
        {
            SetWifiState(WIFI_CONNECTED);
        }
        break;
    case SL_NETAPP_EVENT_DHCPV4_LEASED:
        break;
        
    case SL_NETAPP_EVENT_DHCPV4_RELEASED:
        break;
        
    default:
        break;
    }
}
/*!
    \brief          SimpleLinkFatalErrorEventHandler

    This handler gets called whenever a socket event is reported
    by the NWP / Host driver. After this routine is called, the user's
    application must restart the device in order to recover.

    \param          slFatalErrorEvent    -   pointer to fatal error event.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 17.9.

*/
void SimpleLinkFatalErrorEventHandler(SlDeviceFatal_t *slFatalErrorEvent)
{
  WifiMsg_t wifiMsg={MSG_SENDER_NWP,FAULT_RECOVERY_MSG,NULL};
    
    switch(slFatalErrorEvent->Id)
    {
    case SL_DEVICE_EVENT_FATAL_DEVICE_ABORT:
        WiFiDebugPrint("[ERROR] - FATAL ERROR: Abort NWP event detected: "
                       "AbortType=%d, AbortData=0x%x\n\r",
                       slFatalErrorEvent->Data.DeviceAssert.Code,
                       slFatalErrorEvent->Data.DeviceAssert.Value);
        WiFiDebugFlush();
        break;
        
    case SL_DEVICE_EVENT_FATAL_DRIVER_ABORT:
        WiFiDebugPrint("[ERROR] - FATAL ERROR: Driver Abort detected. \n\r");
        WiFiDebugFlush();
        break;
        
    case SL_DEVICE_EVENT_FATAL_NO_CMD_ACK:
        WiFiDebugPrint("[ERROR] - FATAL ERROR: No Cmd Ack detected "
                       "[cmd opcode = 0x%x] \n\r",
                       slFatalErrorEvent->Data.NoCmdAck.Code);
        WiFiDebugFlush();
        
        break;
        
    case SL_DEVICE_EVENT_FATAL_SYNC_LOSS:
        WiFiDebugPrint("[ERROR] - FATAL ERROR: Sync loss detected \n\r");
        WiFiDebugFlush();
        break;
        
    case SL_DEVICE_EVENT_FATAL_CMD_TIMEOUT:
        WiFiDebugPrint("[ERROR] - FATAL ERROR: Async event timeout detected "
                       "[event opcode =0x%x]  \n\r",
                       slFatalErrorEvent->Data.CmdTimeout.Code);
        WiFiDebugFlush();
        break;
        
    default:
        WiFiDebugPrint("[ERROR] - FATAL ERROR: Unspecified error detected \n\r");
        WiFiDebugFlush();
        break;
    }
   Mailbox_post(wifiMailboxHandle,&wifiMsg,BIOS_NO_WAIT);
}
/*!
    \brief          SimpleLinkNetAppRequestMemFreeEventHandler

    This handler gets called whenever the NWP is done handling with
    the buffer used in a NetApp request. This allows the use of
    dynamic memory with these requests.

    \param          pNetAppRequest     -   Pointer to NetApp request structure.

    \param          pNetAppResponse    -   Pointer to NetApp request Response.

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 17.9.

    \return         void

*/
void SimpleLinkNetAppRequestMemFreeEventHandler(uint8_t *buffer)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkNetAppRequestEventHandler

    This handler gets called whenever a NetApp event is reported
    by the NWP / Host driver. User can write he's logic to handle
    the event here.

    \param          pNetAppRequest     -   Pointer to NetApp request structure.

    \param          pNetAppResponse    -   Pointer to NetApp request Response.

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 17.9.

    \return         void

*/
void SimpleLinkNetAppRequestEventHandler(SlNetAppRequest_t *pNetAppRequest, SlNetAppResponse_t *pNetAppResponse)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkHttpServerEventHandler

    This handler gets called whenever a HTTP event is reported
    by the NWP internal HTTP server.

    \param          pHttpEvent       -   pointer to http event data.

    \param          pHttpEvent       -   pointer to http response.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) chapter 9.

*/
void SimpleLinkHttpServerEventHandler(SlNetAppHttpServerEvent_t *pHttpEvent,
                                      SlNetAppHttpServerResponse_t *pHttpResponse)
{
    /* Unused in this application */
}

/*!
    \brief          SimpleLinkWlanEventHandler

    This handler gets called whenever a WLAN event is reported
    by the host driver / NWP. Here user can implement he's own logic
    for any of these events. This handler is used by 'network_terminal'
    application to show case the following scenarios:

    1. Handling connection / Disconnection.
    2. Handling Addition of station / removal.
    3. RX filter match handler.
    4. P2P connection establishment.

    \param          pWlanEvent       -   pointer to Wlan event data.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) sections 4.3.4, 4.4.5 and 4.5.5.

    \sa             cmdWlanConnectCallback, cmdEnableFilterCallback, cmdWlanDisconnectCallback,
                    cmdP2PModecallback.

*/
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{

    WifiMsg_t wifiMsg={MSG_SENDER_NWP,DHCP_LEASE_MSG,NULL};
    switch(pWlanEvent->Id)
    {
    case SL_WLAN_EVENT_CONNECT:
        wifiLinkStatus |= BIT_WIFI_CONNECTED;
        Mailbox_post(wifiMailboxHandle,&wifiMsg,BIOS_NO_WAIT);
        if(WIFI_CONNECTED == wifiLinkStatus)
        {
            SetWifiState(WIFI_CONNECTED);
        }
        break;

    case SL_WLAN_EVENT_DISCONNECT:
      
        //wifiMsg.msgId = STATION_DISCONECTION_MSG;
        //Mailbox_post(wifiMailboxHandle,&wifiMsg,BIOS_NO_WAIT);
        ResetInstrumentConnection(INET_NOT_MONITORING_INSTRUMENT,NO_CONNECTION_AVALIBLE);
        if(WIFI_CONNECTED == wifiLinkStatus)
        {
          SetWifiState(WIFI_DISCONNECTED);
          WiFiDebugPrint("AP DisConnected:%d\r\n",pWlanEvent->Data.Disconnect.ReasonCode);
          WiFiDebugFlush();
        } 
        break;

    case SL_WLAN_EVENT_STA_ADDED:
        break;

    case SL_WLAN_EVENT_STA_REMOVED:
        break;

    default:
        break;
    }
}
/*!
    \brief          SimpleLinkGeneralEventHandler

    This handler gets called whenever a general error is reported
    by the NWP / Host driver. Since these errors are not fatal,
    application can handle them.

    \param          pDevEvent    -   pointer to device error event.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 17.9.

*/
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    /* Most of the general errors are not FATAL. are to be handled            */
    /* appropriately by the application.                                      */
    WiFiDebugPrint("[GENERAL EVENT] - ID=[%d] Sender=[%d]\r\n",
                   pDevEvent->Data.Error.Code,
                   pDevEvent->Data.Error.Source);
}

/*!
    \brief          SimpleLinkSockEventHandler

    This handler gets called whenever a socket event is reported
    by the NWP / Host driver.

    \param          SlSockEvent_t    -   pointer to socket event data.

    \return         void

    \note           For more information, please refer to: user.h in the porting
                    folder of the host driver and the  CC3120/CC3220 NWP programmer's
                    guide (SWRU455) section 7.6.

*/
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    /* This application doesn't work w/ socket - Events are not expected      */
    switch(pSock->Event)
    {
    case SL_SOCKET_TX_FAILED_EVENT:
        switch(pSock->SocketAsyncEvent.SockTxFailData.Status)
        {
        case SL_ERROR_BSD_ECLOSE:
            break;
        default:
            break;
        }
        break;
        
    default:
        break;
    }
}

BOOLEAN isWifiLinkUp(void)
{
    
    return (wifiLinkStatus == WIFI_LINK_UP? true : false);
}
void ResetWiFiLinkState(void)
{
    wifiLinkStatus = 0;
}