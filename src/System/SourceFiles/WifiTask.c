#include "WifiTask.h"
#include "WifiHTTPClient.h" 
#include "platform.h"
#include "jsmn.h"
#include "EventQ.h"
#include "WatchDog.h"
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC32XX.h>
#include <ti/sysbios/hal/Hwi.h>


#define SNTP_DATAGRAM_SIZE             48
#define SNTP_SERVER_PORT               123
#define SECOND_SINCE_1970              2208988800UL

#define MINIMUM_RSSI_IN_DB              -25
#define MAXIMUM_RSSI_IN_DB              -100

#define MINIMUM_RSI_NORMALIZE           0
#define MAXIMUM_RSI_NORMALIZE           75
#define JSON_STRING_LENGTH              128
#define PM_MAX_SLEEP_TIME_FOR_BEACON    1000

#define SL_MAX_JSON_TOKEN_NUM           30

#define MIN_BAT_FOR_RADIO_CALIB_IN_uV  3650        

#define MAX_NUMBER_OF_NETWORKS          30

#pragma data_alignment = 8
uint8_t WifiTaskStack[WIFI_TASK_STACK_SIZE];

uint8_t HTTPReqBuffer[HTTP_REQ_BUFFER_SIZE];
uint8_t HTTPResponceBuffer[HTTP_RESP_BUFFER_SIZE];

jsmntok_t js_tok[SL_MAX_JSON_TOKEN_NUM];
extern CloudInfo_t httpCloudInfo;

static WifiInfo_t wifiInfoObj;
static WifiConfig_t wifiConfigObj;
Mailbox_Handle wifiMailboxHandle;

void wifiInfoInit(void);
static void WifiTask(void);

static void WifiDisable(void);
static void WifiEnable();
static int32_t SetStationMode(void);
static void WifiConnect(void);
static void WifiAutoReconnect(void);

static int32_t ConnectToWifiAP(WifiConfig_t *ptrWifiConfigObj);
static void DisconnectFromWifiAP(void);
static int16_t EstablishConnectionWithAP(void);
static void UpdateNetworkConfig(void);
static void UpdateIpSettings(void);
static void WifiUpdateRxRssiStats(void);
static void GetNetworksInAir(void);
static void WifiUpdateTimeFromSNTPServer(void);
static void SlDateTimeUpdateFromRTC(SlDateTime_t *time);
static void UpdateWIFIRSSIStats(void);

static void LoadSettingsFromDataFlash(void);
static void SaveSettingsToDataFlash(void);
static void RemoveUserInformationFromFlash(void);
static void SaveGPSCoordinates(void);
static void SaveCloudParameter(void);

static int SaveCloudParametersToFileSystem(CloudInfo_t *ptrToCloudInfo);
static int SaveBatteryParametersToFileSystem(BatteryInfo_t *ptrToBatteryInfo);
static int SaveJobParameterToFileSystem(JobInfo_t *ptrToJobInfo);
static int SaveGPSCoordinatesToFileSystem(GPSFullInfo_t *ptrToGPSInfo);

static int32_t LoadBatteryParameters(BatteryInfo_t *ptrToBatteryInfo);
static int32_t LoadJobParameters(JobInfo_t *ptrToJobInfo);
static int32_t LoadCloudParameters(CloudInfo_t *ptrToCloudInfo);
static int32_t LoadGPSCoordinates(GPSFullInfo_t *ptrToGPSInfo);

static BOOLEAN CompareCloudParameters(CloudInfo_t *ptrToCloudInfo1,CloudInfo_t *ptrToCloudInfo2);
static BOOLEAN CompareBatteryParameters(BatteryInfo_t *ptrToBatteryInfo1,BatteryInfo_t *ptrToBatteryInfo2);
static BOOLEAN CompareJobParameters(JobInfo_t *ptrToJobInfo1,JobInfo_t *ptrToJobInfo2);
static BOOLEAN CompareGPSCoordinates(GPSFullInfo_t *ptrToGPSInfo1,GPSFullInfo_t *ptrToGPSInfo2);

static int32_t JParseGPSCoordinates(GPSFullInfo_t *ptrToGPSInfo,uint8_t js_data[],size_t len);
static int32_t JParseBatteryParameters(BatteryInfo_t *ptrToBatteryInfo,uint8_t js_data[],size_t len);
static int32_t JParseCloudParameters(CloudInfo_t *ptrToCloudInfo,uint8_t js_data[],size_t len);

static void InitiateRadioCalibration(void);

uint8_t const* const filePath[] ={"/sys/BatterySettings.json","/sys/JobSettings.json","/sys/CloudSettings.json","/sys/GPS.json"};

void WifiTaskInit(void)
{
    // create Wifi Task
    static Task_Struct WifiTaskStruct;
    static Mailbox_Struct WifiMailboxStruct;
    
    Task_Params WifiTaskParams;
    Error_Block WifiTaskEb;
    Error_init(&WifiTaskEb);
    Task_Params_init(&WifiTaskParams);
    WifiTaskParams.stackSize = WIFI_TASK_STACK_SIZE;
    WifiTaskParams.stack = &WifiTaskStack;
    WifiTaskParams.priority = WIFI_TASK_PRIORITY;
    Task_construct(&WifiTaskStruct,(Task_FuncPtr)WifiTask,&WifiTaskParams,&WifiTaskEb);
    
    
    Mailbox_Params WifiMailboxParams;
    Error_Block WifiMailboxErrorBlock;
    uint32_t WifiMailboxMsgSize=sizeof(WifiMsg_t);
    Mailbox_Params_init(&WifiMailboxParams);
    Mailbox_construct(&WifiMailboxStruct,WifiMailboxMsgSize,WIFI_MAILBOX_MSG_COUNT
                      ,&WifiMailboxParams, &WifiMailboxErrorBlock);
    wifiMailboxHandle=Mailbox_handle(&WifiMailboxStruct);
    
    //initialize wifi Info struct
    wifiInfoInit();
}

static void wifiInfoInit(void)
{
    memset(&wifiInfoObj,0,sizeof(WifiInfo_t));
    wifiInfoObj.wifiState = WIFI_DISABLE;
    wifiInfoObj.RSSIUpdateInterval = RSSI_UPDATE_INTERVAL >> 2;
    wifiInfoObj.ScanInterval = WIFI_SCAN_INTERVAL;
}

static void WifiTask(void)
{
    WifiMsg_t msg;    
    
    //WifiEnable();
    WifiAutoReconnect();
    LoadSettingsFromDataFlash();
    //WifiDisable();
    //init Client Information Here after Cloud Parameter are loaded
    slHTTPCliStruct_t httpCli;
    HttpInit(&httpCli);
    
    while(1)
    {
        //Set the Watch dog state to inactive
        SysWatchDogChangeState(WATCH_DOG_WIFI_TASK_INDEX,TASK_STATE_INACTIVE);
        
        Mailbox_pend(wifiMailboxHandle, &msg, BIOS_WAIT_FOREVER);
        
        //Set the Watch dog state to active 
        SysWatchDogChangeState(WATCH_DOG_WIFI_TASK_INDEX,TASK_STATE_ALIVE);
        //Clear the watch dog counter for this task 
        SysWatchDogClearCounter(WATCH_DOG_WIFI_TASK_INDEX);
        
        WiFiDebugPrint("WIFI_TASK STARTED, Msg Recv: %d\r\n", msg.msgId);
        WiFiDebugFlush();
        
        
//        WiFiDebugPrint("SNTP NEXT UPDATE AFTER: %d\r\n"
//                       "RSSI NEXT UPDATE AFTER: %d\r\n",
//                       wifiInfoObj.SNTPTimeUpdateInterval,
//                       wifiInfoObj.RSSIUpdateInterval
//                       );
//        WiFiDebugFlush();
        switch(msg.msgId)
        {
        case WIFI_DISABLE_MSG:
            WifiDisable();
            WiFiDebugPrint("WIFI EVENT RECIVED\r\n");
            WiFiDebugFlush();
            break;
        case WIFI_ENABLE_MSG:
            WifiEnable();
            WiFiDebugPrint("WIFI EVENT RECIVED\r\n");
            WiFiDebugFlush();        
            break;
        case WIFI_AUTO_RECONNECT_MSG:
            WifiAutoReconnect();
            break;
        case DHCP_LEASE_MSG:
            WiFiDebugPrint("WIFI DHCP LEASE EVENT RECIVED\r\n");
            WiFiDebugFlush();
            UpdateNetworkConfig();
            DispatchConnectionTestMessageToWifiTask();
            break;
        case FAULT_RECOVERY_MSG:
            WiFiDebugPrint("WIFI FAULT RECOVERY EVENT RECIVED\r\n");
            WiFiDebugFlush();
            EstablishConnectionWithAP();
            break;
        case UPDATE_RSSI_MSG:
            
            WiFiDebugPrint("WIFI UPDATE RSSI EVENT RECIVED\r\n");
            WiFiDebugFlush();
            
            WifiUpdateRxRssiStats();
            break;
            
        case SCAN_NETWORKS_IN_AIR:
            if(GetWifiState() == WIFI_DISABLE)
            {
                WiFiScanDebugPrint("WiFi Disabled: Sacan Failed\r\n");
                WiFiScanDebugFlush();
            }
            else
            {
                GetNetworksInAir();
            }
            break;
        case UPDATE_TIME_MSG:
            
            WiFiDebugPrint("WIFI UPDATE TIME EVENT RECIVED\r\n");
            WiFiDebugFlush();
            
            WifiUpdateTimeFromSNTPServer();
            break;
        case ESTABLISH_CONNECTION_MSG:
            WiFiDebugPrint("WIFI CONNECTION EVENT RECIVED\r\n");
            WiFiDebugFlush();
            
            EstablishConnectionWithAP();
            break;
        case STATION_DISCONECTION_MSG:
            //ResetInstrumentConnection(INET_NOT_MONITORING_INSTRUMENT,NO_CONNECTION_AVALIBLE);
            WiFiDebugPrint("AP DISCONNECTION EVENT RECIVED\r\n");
            WiFiDebugFlush();
            break;
        case SEND_MSG_TO_INET:
            
            WiFiDebugPrint("WIFI INET EVENT RECIVED\r\n");
            WiFiDebugFlush();
            SndQueueEventsToINET(&httpCli,msg.msgInfo);
            break;
        case TEST_CONN_WITH_INET:
            WiFiDebugPrint("WIFI INET TEST EVENT RECIVED\r\n");
            WiFiDebugFlush();
            TestConnectionWithINET(&httpCli);
            break;
            
        case DELETE_FILE_IN_SFLASH:
            break;
            
        case VIEW_FILE_LIST_IN_SFLASH:
            break;
            
        case VIEW_FILE_IN_SFLASH:
            break;
            
        case OPEN_FIRMWARE_FILE:
            if(GetWifiState() == WIFI_DISABLE)
            {
                WifiEnable();
            }
            ReInitFirmwareUploadStateMachine();
            break;
            
        case COMMIT_FIRMWARE_DATA:
            //Run File Upload state Machine 
            GetFirmwareFromQueue();
            break;
            
        case CLOSE_FIRMWARE_FILE:
            
            if(GetParameterUpdateStatus())
            {                
                SaveSettingsToDataFlash();
            }
            
            PostRebootEvent();
            break;
            
        case COMMIT_PARAMETERS_TO_FLASH:
            
            WiFiDebugPrint("WIFI BATTERY SETTINGS COMMIT EVENT RECIVED\r\n");
            WiFiDebugFlush();

            if(GetWifiState() == WIFI_DISABLE)
            {
                WifiEnable();
                SaveSettingsToDataFlash();
                WifiDisable();
            }
            else
            {
                SaveSettingsToDataFlash();
            }
            break;
        case REMOVE_PARAMETERS_FROM_FLASH:
            
            WiFiDebugPrint("WIFI BATTERY SETTINGS REMOVE EVENT RECIVED\r\n");
            WiFiDebugFlush();
            
            if(GetWifiState() == WIFI_DISABLE)
            {
                WifiEnable();
                RemoveUserInformationFromFlash();
                WifiDisable();
            }
            else
            {
                RemoveUserInformationFromFlash();
            }
            break;
        case COMMIT_CLOUD_PARAMETER_TO_FLASH:
            WiFiDebugPrint("WIFI Cloud SETTINGS COMMIT EVENT RECIVED\r\n");
            WiFiDebugFlush();
            
            if(GetWifiState() == WIFI_DISABLE)
            {
                WifiEnable();
                SaveCloudParameter();
                WifiDisable();
            }
            else
            {
                SaveCloudParameter();
            }
            break;
        case COMMIT_GPS_COORDINATES_TO_FLASH:
            
            WiFiDebugPrint("WIFI GPS SETTINGS EVENT RECIVED\r\n");
            WiFiDebugFlush();
            
            if(GetWifiState() == WIFI_DISABLE)
            {
                WifiEnable();
                SaveGPSCoordinates();
                WifiDisable();
            }
            else
            {
                SaveGPSCoordinates();
            }
            break;
        default : 
            WiFiDebugPrint("Unknown Msg Recived\r\n");
            WiFiDebugFlush();
            break;
        }
        
        WiFiDebugPrint("WIFI_TASK END\r\n");
        WiFiDebugFlush();
  }
}
//------------------------------------------------------------------------------
//   static int16_t EstablishConnectionWithAP(void)
//
//   Author:  Dilawar Ali
//   Date:    2018/02/13
//
//!  This function Switch WiFi Working State from station mode to Access Point mode and back
//
//------------------------------------------------------------------------------
static int16_t EstablishConnectionWithAP(void)
{
    int16_t ret = -1;
    WifiDisable();
    WifiEnable(true);
    //Enable Station mode
    ret = (int16_t)SetStationMode();
    WifiConnect();
    
    return ret;
}
//------------------------------------------------------------------------------
//   static void WifiEnable(void)
//
//   Author:  Ammar Younas
//   Date:    2017/01/17
//   Enable CC3100 Wifi Module
//
//------------------------------------------------------------------------------
static void WifiEnable(void)
{
    int32_t time_out=0;
    int32_t status=0;
    uint16_t len = 0;
    DateTimeInfo_t SystimeInfo;
    SlDateTime_t wifiTime;
    GetCurrentTimeAndDate(&SystimeInfo);
    //Set Date and time for INET Certificate verification
    wifiTime.tm_day = SystimeInfo.date.day;
    wifiTime.tm_mon = SystimeInfo.date.month;
    wifiTime.tm_year = SystimeInfo.date.year;
    wifiTime.tm_sec = SystimeInfo.time.seconds;
    wifiTime.tm_min =  SystimeInfo.time.minutes;
    wifiTime.tm_hour = SystimeInfo.time.hours;
    
    InitiateRadioCalibration();
    do
    {
        // Enable CC3220
        status = sl_Start(0,0,0);
        if(0 > status )
        {
            //if cc3100 not enabled stop cc3100
            sl_Stop(0);
        }
        //Retry to Enable untitl retries time out
    }while((status < 0) && (++time_out < WIFI_RECONNECTION_ATTEMPTS));

    //check if cc3100 is enable
    if(time_out < WIFI_RECONNECTION_ATTEMPTS)
    {
        //get MAC Adress Array size
        uint16_t macAddressLen = (sizeof(wifiInfoObj.wifiMacAdress)/sizeof(wifiInfoObj.wifiMacAdress[0]));
        //Set Local variable to get cc3100 Version information
        uint8_t pConfigOpt = SL_DEVICE_GENERAL_VERSION;
        //Get Variable size to store version information
        uint16_t pConfigLen = sizeof(wifiInfoObj.wifiDevInfo.devVersion);
        //Get Version information in Global structure i.e slState.slFullVersion
        sl_DeviceGet(SL_DEVICE_GENERAL,&pConfigOpt,&pConfigLen,(_u8 *)(&wifiInfoObj.wifiDevInfo.devVersion));
        //set device info update flag to true
        wifiInfoObj.wifiDevInfo.isUpdated=true;
        //Get Version information in Global structure i.e slState.slMacAdress
        sl_NetCfgGet(SL_NETCFG_MAC_ADDRESS_GET,NULL,&macAddressLen,(_u8 *)wifiInfoObj.wifiMacAdress);
        
        
        len = snprintf(
                       (char *)InstrumentInfo.wirelessInfo.WirelessMAC,MAC_ADRESS_LENGTH_PLUS_END_OF_FRAME,
                       "%02X:%02X:%02X:%02X:%02X:%02X",
                       wifiInfoObj.wifiMacAdress[0],
                       wifiInfoObj.wifiMacAdress[1],
                       wifiInfoObj.wifiMacAdress[2],
                       wifiInfoObj.wifiMacAdress[3],
                       wifiInfoObj.wifiMacAdress[4],
                       wifiInfoObj.wifiMacAdress[5]
                      );
        InstrumentInfo.wirelessInfo.WirelessMAC[len] = NULL;
        
        
        // Load current Date time to cc3100
        status = sl_DeviceSet(SL_DEVICE_GENERAL, SL_DEVICE_GENERAL_DATE_TIME,  sizeof(SlDateTime_t),(unsigned char *)(&wifiTime));//#needUpdate set current time not hard coded
        //check if date time succesfully loadded to cc3100
        if(0 > status)
        {
            WiFiDebugPrint("Failed to configure cc3100 Date and time .\n");
            WiFiDebugFlush();
        }
        SetWifiState(WIFI_ENABLE);

    }
    return;
}
static void WifiAutoReconnect(void)
{
    WifiDisable();
    WifiEnable();
    sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION,
                     SL_WLAN_CONNECTION_POLICY(1, 1, 0, 0), NULL, 0);
}

static void WifiConnect(void)
{
    int time_out = WIFI_RECONNECTION_ATTEMPTS;
    int status = -1;


    //Upload wifi Related Certificates from DataFlash
    //UploadWifiCerts();
    
    sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION,
                     SL_WLAN_CONNECTION_POLICY(1, 1, 0, 0), NULL, 0);
    
    while( ( status!=0 ) && ( time_out!=0 ) )
    {
        //Disconnect from Acess Point if Already connected
        DisconnectFromWifiAP();
        
        Hwi_disable();
        
        SLSetApParam();
        
        Hwi_enable();
        //Try to connect with Acess Point
        //SysWatchDogClearCounter(WATCH_DOG_WIFI_TASK_IDX);
        status = ConnectToWifiAP(&wifiConfigObj);
        --time_out;
    }

    if( time_out==0 )
    {
        //Set cc3100 state to error
        wifiInfoObj.wifiState=WIFI_DISCONNECTED;
        //SlWifiSetErrorStatus(true);

        //SlWifiDisconnect();

        WiFiDebugPrint("Reconnection Timeout\n");
        WiFiDebugFlush();
    }

}

void SLSetApParam(void)
{
    WiFiDebugPrint("SSID:%.*s\r\n",NETWORK_ID_LENGTH,InstrumentInfo.networkInfo.NetworkID);
    WiFiDebugFlush();
    
    WiFiDebugPrint("PASSWORD:%.*s\r\n",NETWORK_PASSWORD_LENGTH,InstrumentInfo.networkInfo.NetworkPassword);
    WiFiDebugFlush();
    
    WiFiDebugPrint("ENCRYPTION:%d\r\n",InstrumentInfo.networkInfo.NetworkEncryptionType);
    WiFiDebugFlush();
    ResetNetworkFlags();
    if(NULL != InstrumentInfo.networkInfo.NetworkID[0])
    {
        memcpy(wifiConfigObj.ssid,InstrumentInfo.networkInfo.NetworkID,NETWORK_ID_LENGTH); 
        wifiConfigObj.ssidLen = strlen((char const *)wifiConfigObj.ssid);
        wifiConfigObj.wifiSecParam.Type = InstrumentInfo.networkInfo.NetworkEncryptionType;//SL_WLAN_SEC_TYPE_WPA_WPA2;
        //Get Acess Point Key from flash i.e Pre Shared Key(PSK) for Personal and User Key for Enterprise
        memcpy(wifiConfigObj.key,InstrumentInfo.networkInfo.NetworkPassword,NETWORK_PASSWORD_LENGTH);   
        wifiConfigObj.keyLen = strlen((char const *)wifiConfigObj.key);
        wifiConfigObj.wifiSecParam.Key = (_i8 *)wifiConfigObj.key;
        //Get Acess Point Key string size from flash
        wifiConfigObj.wifiSecParam.KeyLen = wifiConfigObj.keyLen;
        
        if(InstrumentInfo.networkInfo.NetworkType == NETWORK_TYPE_STATIC)
        {
            wifiConfigObj.wifiDhcpCfg = WIFI_DHCP_DISABLE;
            
            inet_aton(InstrumentInfo.networkInfo.IPAddress,(uint32_t *)&(wifiConfigObj.wifiNetIpv4Cfg.Ip));
            inet_aton(InstrumentInfo.networkInfo.NetworkMask,(uint32_t *)&(wifiConfigObj.wifiNetIpv4Cfg.IpMask));
            inet_aton(InstrumentInfo.networkInfo.Gateway,(uint32_t *)&(wifiConfigObj.wifiNetIpv4Cfg.IpGateway));
            inet_aton(InstrumentInfo.networkInfo.DNS,(uint32_t *)&(wifiConfigObj.wifiNetIpv4Cfg.IpDnsServer));
        }
        else
        {
            wifiConfigObj.wifiDhcpCfg = WIFI_DHCP_ENABLE;
        }
    }
    else
    {
        WiFiDebugPrint("Invalid Parameter Recived\r\n");
        WiFiDebugFlush();
    }
}

//------------------------------------------------------------------------------
//   static void SlWifiDisable(void)
//
//   Author:  Ammar Younas
//   Date:    2017/01/17
//   Enable CC3220 Wifi Module
//
//------------------------------------------------------------------------------
static void WifiDisable(void)
{
    //Inet is not accepting the instrument
    ResetInstrumentConnection(INET_NOT_MONITORING_INSTRUMENT,NO_CONNECTION_AVALIBLE);
    sl_WlanPolicySet(SL_WLAN_POLICY_CONNECTION,
                     SL_WLAN_CONNECTION_POLICY(0, 0, 0, 0), NULL, 0);
    
    SetWifiState(WIFI_DISABLE);
    sl_Stop(0XFF);
    
}
//------------------------------------------------------------------------------
//  int SetStationMode(void) 
//
//   Author:  Ammar Younas
//   Date:    2017/04/5
//  Put CC3100 To Station Mode
//
//------------------------------------------------------------------------------
static int32_t SetStationMode(void) 
{
    int16_t retVal = 0;
    uint32_t intervalInSeconds = 0;
    uint16_t loopTimeOut = WIFI_MODE_TIME_OUT;
    SlWlanPmPolicyParams_t PmPolicyParams;
    
    do
    {
        sl_WlanSetMode(ROLE_STA);
        sl_Stop(0xFF);
        retVal = sl_Start(NULL, NULL, NULL);
    }while( (ROLE_STA != retVal) && (--loopTimeOut > 0) );
    
    if(ROLE_STA == retVal)
    {
        memset(&PmPolicyParams,0,sizeof(SlWlanPmPolicyParams_t));
        //max sleep time in mSec
        PmPolicyParams.MaxSleepTimeMs = PM_MAX_SLEEP_TIME_FOR_BEACON;
        sl_WlanPolicySet(SL_WLAN_POLICY_PM , SL_WLAN_LONG_SLEEP_INTERVAL_POLICY, (_u8*)&PmPolicyParams,sizeof(PmPolicyParams));
        
        if( 0 == retVal )
        {
            //Disable station Scan 
            retVal = sl_WlanPolicySet(SL_WLAN_POLICY_SCAN,SL_WLAN_DISABLE_SCAN,(_u8 *)&intervalInSeconds,sizeof(intervalInSeconds));
            /* Enable DHCP client */
            sl_NetCfgSet(SL_NETCFG_IPV4_STA_ADDR_MODE,SL_NETCFG_ADDR_DHCP,0,0);
            
        }
        else
        {
            WiFiDebugPrint("WiFi: Failed To Set Connection Policy\r\n");
            WiFiDebugFlush();
        }
    }
    else
    {
        WiFiDebugPrint("WiFi: Unable To Configure WiFI in Station Mode\r\n");
        WiFiDebugFlush();
    }
    return retVal;    
}

static int32_t ConnectToWifiAP(WifiConfig_t *ptrWifiConfigObj)
{
    _u8 pValues = 1;
    int status=0;
    
    //Check DHCP flag
    if(WIFI_DHCP_ENABLE == ptrWifiConfigObj->wifiDhcpCfg)
    {
        //Set mode to DHCP
        status=sl_NetCfgSet(SL_NETCFG_IPV4_STA_ADDR_MODE,SL_NETCFG_ADDR_DHCP,0,0);
        status = sl_Stop(~(0u));
        status = sl_Start(NULL,NULL,NULL);
    }
    //Check Static IP
    if(WIFI_DHCP_DISABLE == ptrWifiConfigObj->wifiDhcpCfg)
    {
        //set Mode to static
        status = sl_NetCfgSet(SL_NETCFG_IPV4_STA_ADDR_MODE,SL_NETCFG_ADDR_STATIC,sizeof(SlNetCfgIpV4Args_t),(_u8 *)&ptrWifiConfigObj->wifiNetIpv4Cfg);
        status = sl_Stop(~(0u));
        status = sl_Start(NULL,NULL,NULL);
        //Set IP acuired to true for static mode
    }
    
    if( 0 == status )
    {
        // Delete All Profiles
        //sl_WlanProfileDel(SL_WLAN_DEL_ALL_PROFILES);
        status = sl_WlanProfileAdd((_i8 const *)ptrWifiConfigObj->ssid,ptrWifiConfigObj->ssidLen
                                 ,NULL,&ptrWifiConfigObj->wifiSecParam,0,1,0);
        if(0 <= status)
        {
            //To Disable Certificate Verfification set pValues=0,Enable Certificate Verification Set pValues to 1
            status=sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,SL_WLAN_GENERAL_PARAM_DISABLE_ENT_SERVER_AUTH,sizeof(pValues),(_u8 *)&pValues);
            if(0 == status)
            {
                ResetWiFiLinkState();
                //Restart Network Processor NWP for configuration to be effective
                sl_Stop(0);
                status=sl_Start(0,0,0);
                //Connect To Wireless Acess Point
                if(0 <= status)
                {
                    status=sl_WlanConnect((_i8 const *)ptrWifiConfigObj->ssid,ptrWifiConfigObj->ssidLen, 0, &ptrWifiConfigObj->wifiSecParam,
                                          (  ptrWifiConfigObj->wifiSecExtParam.EapMethod==0) ? (0) :(&ptrWifiConfigObj->wifiSecExtParam));
                    if(0 == status)
                    {
                        WiFiDebugPrint("WiFi: Connection To AP In Progress\r\n");
                        WiFiDebugFlush();
                    }
                    else
                    {
                        WiFiDebugPrint("WiFi: Connect Command Failed\r\n");
                        WiFiDebugFlush();
                    }
                }
                else
                {
                    WiFiDebugPrint("WiFi:Failed To start Network Processor\r\n");
                    WiFiDebugFlush();
                }
            }
            else
            {
                WiFiDebugPrint("WiFi: Failed To Set Enterprise Protection Settings\r\n");
                WiFiDebugFlush();
            }
        }
        else
        {
            WiFiDebugPrint("WiFi: Saving Profile To Flash failed\r\n");
            WiFiDebugFlush();
        }
    }
    
    
    return status;
}

//------------------------------------------------------------------------------
//   static void DisconnectFromWifiAP(void)
//
//   Author:  Ammar Younas
//   Date:    2017/01/17
//   DisConnect NWP from AP
//
//------------------------------------------------------------------------------
static void DisconnectFromWifiAP(void)
{
    // Remove stored AP Configuration File
    sl_WlanProfileDel(SL_WLAN_DEL_ALL_PROFILES);
    //Disconnect from router
    sl_WlanDisconnect();
    //Set Wifi state to Diconnected
    wifiInfoObj.wifiState = WIFI_DISCONNECTED;
}

static void UpdateNetworkConfig(void)
{
    uint8_t SSID[32] = {0};
    uint16_t len = 0;
    uint16_t dhcpIsOn = 0;
    SlWlanSecParams_t secParams;
    
    //Read SSID from Profile
    len = NETWORK_ID_LENGTH_PLUS_END_OF_FRAME;
    sl_WlanProfileGet(0,(signed char *)SSID,(signed short *)&len, NULL, &secParams,NULL,NULL);
    len = (len < NETWORK_ID_LENGTH ? len : NETWORK_ID_LENGTH);    
    if (
        ( SSID[0] !=  NULL ) &&
        ( len != 0) &&
        ( false == GetNetworkConfigInProgress() ) 
        )
    {
        //Copy SSID to Instrument Info
        memcpy(InstrumentInfo.networkInfo.NetworkID,SSID,len);
        InstrumentInfo.networkInfo.NetworkID[len] = NULL;
        InstrumentInfo.networkInfo.NetworkEncryptionType = (SMART_BATTERY_ENCRYPTION_TYPE_t)secParams.Type;
        
        //Get IP information from CC3220
        len = sizeof(wifiInfoObj.wifiNetCfgCurrentlyAssigned);
        memset(&wifiInfoObj.wifiNetCfgCurrentlyAssigned,0,sizeof(SlNetCfgIpV4Args_t));
        sl_NetCfgGet(SL_NETCFG_IPV4_STA_ADDR_MODE,&dhcpIsOn,&len,(_u8 *)&wifiInfoObj.wifiNetCfgCurrentlyAssigned);
        
        if(wifiInfoObj.wifiNetCfgCurrentlyAssigned.Ip != 0)
        {
            InstrumentInfo.wirelessInfo.WirelessRSSI = 1;
            wifiInfoObj.wifiState=WIFI_CONNECTED;
            //start updating Rx Tx stats
            sl_WlanRxStatStart();
            //Run SNTP and Get
            WifiUpdateTimeFromSNTPServer();
            if( SL_NETCFG_ADDR_DHCP == dhcpIsOn )
            {
                InstrumentInfo.networkInfo.NetworkType = 1;
                Swi_disable();
                UpdateIpSettings();
                Swi_enable();
                WiFiDebugPrint("Network Configuration accuired from DHCP server \r\n");
            }
            else
            {
                InstrumentInfo.networkInfo.NetworkType = 0;
                Swi_disable();
                UpdateIpSettings();
                Swi_enable();
                WiFiDebugPrint("\nNetwork Configuration accuired statically \r\n");
            }
            
            WiFiDebugPrint("MAC Address: %02x:",wifiInfoObj.wifiMacAdress[0]);
            WiFiDebugPrint("%02x:",wifiInfoObj.wifiMacAdress[1]);
            WiFiDebugPrint("%02x:",wifiInfoObj.wifiMacAdress[2]);
            WiFiDebugPrint("%02x:",wifiInfoObj.wifiMacAdress[3]);
            WiFiDebugPrint("%02x:",wifiInfoObj.wifiMacAdress[4]);
            WiFiDebugPrint("%02x\r\n",wifiInfoObj.wifiMacAdress[5]);
            WiFiDebugFlush();
            
            WiFiDebugPrint("IP Address: %d.", SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.Ip, 3));
            WiFiDebugPrint("%d.", SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.Ip, 2));
            WiFiDebugPrint("%d.", SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.Ip, 1));
            WiFiDebugPrint("%d\r\n", SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.Ip, 0));
            
            
            WiFiDebugPrint("SubnetMask: %d.", SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpMask,3));
            WiFiDebugPrint("%d.",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpMask,2));
            WiFiDebugPrint("%d.",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpMask,1));
            WiFiDebugPrint("%d\r\n",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpMask,0));
            
            WiFiDebugFlush();
            
            WiFiDebugPrint("Gateway: %d.",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpGateway,3));
            WiFiDebugPrint("%d.",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpGateway,2));
            WiFiDebugPrint("%d.",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpGateway,1));
            WiFiDebugPrint("%d\r\n",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpGateway,0));
            
            WiFiDebugPrint("DNSServer: %d.",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpDnsServer,3));
            WiFiDebugPrint("%d.",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpDnsServer,2));
            WiFiDebugPrint("%d.",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpDnsServer,1));
            WiFiDebugPrint("%d\r\n",SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpDnsServer,0));
            
        }
        else
        {
            WiFiDebugPrint("DHCP IP Lease Failed\r\n");
            WiFiDebugFlush();
        }
    }
    else
    {
      WiFiDebugPrint("\n**Update Network Credentials Failed || SSID = %.*s || Len = %d || Net Config Flag = %d \r\n\n ",32, SSID, len , GetNetworkConfigInProgress());
      WiFiDebugFlush();
    }

}

static void UpdateIpSettings(void)
{
    uint16_t len = 0;
    
    len = snprintf(
                   (char *)InstrumentInfo.networkInfo.IPAddress,NETWORK_IP_LENGTH_PLUS_END_OF_FRAME,
                   "%d.%d.%d.%d",
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.Ip, 3),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.Ip, 2),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.Ip, 1),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.Ip, 0)
                       );
    InstrumentInfo.networkInfo.IPAddress[len] = NULL;
    
    len = snprintf(
                   (char *)InstrumentInfo.networkInfo.Gateway,NETWORK_IP_LENGTH_PLUS_END_OF_FRAME,
                   "%d.%d.%d.%d",
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpGateway, 3),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpGateway, 2),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpGateway, 1),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpGateway, 0)
                       );
    InstrumentInfo.networkInfo.Gateway[len] = NULL;
    
    len = snprintf(
                   (char *)InstrumentInfo.networkInfo.DNS,NETWORK_IP_LENGTH_PLUS_END_OF_FRAME,
                   "%d.%d.%d.%d",
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpDnsServer, 3),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpDnsServer, 2),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpDnsServer, 1),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpDnsServer, 0)
                       );
    InstrumentInfo.networkInfo.DNS[len] = NULL;
    
    len = snprintf(
                   (char *)InstrumentInfo.networkInfo.NetworkMask,NETWORK_IP_LENGTH_PLUS_END_OF_FRAME,
                   "%d.%d.%d.%d",
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpMask, 3),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpMask, 2),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpMask, 1),
                   SL_IPV4_BYTE(wifiInfoObj.wifiNetCfgCurrentlyAssigned.IpMask, 0)
                       );
    InstrumentInfo.networkInfo.NetworkMask[len] = NULL;
}

static void WifiUpdateRxRssiStats(void)
{
    int32_t status = -1;
    SlWlanGetRxStatResponse_t RxStat;
    status = sl_WlanRxStatGet(&RxStat, 0);
    
    if(status == 0)
    {
        //lock
        Task_disable();
        //Copy to global variable
        //Copy data to global variable
        wifiInfoObj.wifiRxRssiInfo.rssiRxStat.ReceivedValidPacketsNumber           +=   RxStat.ReceivedValidPacketsNumber;
        wifiInfoObj.wifiRxRssiInfo.rssiRxStat.ReceivedFcsErrorPacketsNumber        +=   RxStat.ReceivedFcsErrorPacketsNumber;
        wifiInfoObj.wifiRxRssiInfo.rssiRxStat.ReceivedAddressMismatchPacketsNumber +=   RxStat.ReceivedAddressMismatchPacketsNumber;
        wifiInfoObj.wifiRxRssiInfo.rssiRxStat.AvarageDataCtrlRssi                   =   RxStat.AvarageDataCtrlRssi;
        wifiInfoObj.wifiRxRssiInfo.rssiRxStat.AvarageMgMntRssi                      =   RxStat.AvarageMgMntRssi;
        wifiInfoObj.wifiRxRssiInfo.rssiRxStat.GetTimeStamp                          =   RxStat.GetTimeStamp;
        wifiInfoObj.wifiRxRssiInfo.rssiRxStat.StartTimeStamp                        =   RxStat.StartTimeStamp;
        
        memcpy(&wifiInfoObj.wifiRxRssiInfo.rssiRxStat.RateHistogram,&RxStat.RateHistogram,SL_WLAN_NUM_OF_RATE_INDEXES);
        memcpy(&wifiInfoObj.wifiRxRssiInfo.rssiRxStat.RssiHistogram,&RxStat.RssiHistogram,SL_WLAN_SIZE_OF_RSSI_HISTOGRAM);
        
        Task_enable();
        wifiInfoObj.wifiRxRssiInfo.isUpdated=true;
        if(0 != wifiInfoObj.wifiRxRssiInfo.rssiRxStat.AvarageDataCtrlRssi)
        {
            UpdateWIFIRSSIStats();
        }
    }
}

static void GetNetworksInAir(void)
{
    int16_t retVal = -1;
    int16_t loopCounter = 0;
    uint32_t intervalInSeconds = 15;
    SlWlanNetworkEntry_t netEntries[MAX_NUMBER_OF_NETWORKS];
    //Create a local buffer for formatting the output scan data
    //uint8_t LocalBuffer[sizeof(netEntries[0])] = {0};
    uint8_t LocalBuffer[SL_WLAN_SSID_MAX_LENGTH] = {0};
    memset(netEntries,0,sizeof(netEntries) );
        
    sl_WlanPolicySet(SL_WLAN_POLICY_SCAN,SL_WLAN_SCAN_POLICY(1,1), (_u8 *)&intervalInSeconds,sizeof(intervalInSeconds)); 
    Task_sleep(1000);    
    
    do{
      
      retVal = sl_WlanGetNetworkList(0,(uint8_t)(sizeof(netEntries)/sizeof(netEntries[0])),netEntries);
      
    }while((4 > ++loopCounter) &&( 0 >= retVal ));
      
    if (0 < retVal)
    {
        WiFiScanDebugPrint("************************************************************************************\r\n");
        WiFiScanDebugFlush();
        WiFiScanDebugPrint("SSID:                                  BSSID:                  CHANNEL:    RSSI:    \r\n");
        WiFiScanDebugFlush();    
        WiFiScanDebugPrint("************************************************************************************\r\n");
        WiFiScanDebugFlush();
        
        for(uint8_t i = 0; ( (i< ( sizeof(netEntries)/sizeof(netEntries[0]) ) ) && ( 0 != netEntries[i].SsidLen ) ) ; ++i )
        {
            memset(LocalBuffer,' ',sizeof(LocalBuffer));
            memcpy(LocalBuffer,netEntries[i].Ssid, netEntries[i].SsidLen);
            
            WiFiScanDebugPrint("%.*s ",sizeof(LocalBuffer),LocalBuffer);
            WiFiScanDebugFlush();
            
            WiFiScanDebugPrint("%02x:%02x:%02x:%02x:%02x:%02x                %02d       %02d \r\n",
                               //netEntries[i].SsidLen,
                               //netEntries[i].Ssid,
                               netEntries[i].Bssid[0],
                               netEntries[i].Bssid[1],
                               netEntries[i].Bssid[2],
                               netEntries[i].Bssid[3],
                               netEntries[i].Bssid[4],
                               netEntries[i].Bssid[5],
                               netEntries[i].Channel,
                               netEntries[i].Rssi);
            WiFiScanDebugFlush();
        }
        WiFiScanDebugPrint("************************************************************************************\r\n\n");
        WiFiScanDebugFlush();
    }
    else if (0 == retVal)
    {
        WiFiScanDebugPrint("No AP detected\r\n");
        WiFiScanDebugFlush();
    }
    else
    {
      WiFiScanDebugPrint("WiFi Error: Scan Failed : Err %d\r\n",retVal);
        WiFiScanDebugFlush();
    }
    
    sl_WlanPolicySet(SL_WLAN_POLICY_SCAN,SL_WLAN_DISABLE_SCAN,(_u8 *)&intervalInSeconds,sizeof(intervalInSeconds));
}

static void WifiUpdateTimeFromSNTPServer(void)
{
    uint8_t sntpDatagram[SNTP_DATAGRAM_SIZE];
    int8_t sntpServer[]=SNTP_SERVER;
    uint32_t sntpServerLen=(sizeof(sntpServer)-1);
    static uint32_t sntpServerIp=0;
    int16_t sntpSockId=0;
    SlSockAddr_t sntpSockAddr;
    SlSockAddrIn_t  sntpLocalSockAddr;
    SlSocklen_t sntpLocalSockAddrLen=sizeof(SlSockAddrIn_t);
    struct SlTimeval_t timeVal;
    _u32   elapsedSec=0;
    SlDateTime_t time;

    // Seconds
    timeVal.tv_sec =  SOCKET_SND_RCV_TIMEOUT;
    // Microseconds. 10000 microseconds resolution
    timeVal.tv_usec = 0;
    //socket to listen responce server
    sntpLocalSockAddr.sin_family = SL_AF_INET;
    sntpLocalSockAddr.sin_port = 0;
    sntpLocalSockAddr.sin_addr.s_addr = 0;
    //Socket to send request to server
    sntpSockAddr.sa_family=SL_AF_INET;
    sntpSockAddr.sa_data[0]=0;
    sntpSockAddr.sa_data[1]=SNTP_SERVER_PORT;

    int retVal = 0;

    if(WIFI_CONNECTED == wifiInfoObj.wifiState)
    {
        //Clear the watch dog counter for this task 
        SysWatchDogClearCounter(WATCH_DOG_WIFI_TASK_INDEX);
        
        //resolve hostname to ip address only once
        if(0 == sntpServerIp)
        {
            
            //Set the Watch dog state to inactive
            SysWatchDogChangeState(WATCH_DOG_WIFI_TASK_INDEX,TASK_STATE_INACTIVE);
            
            retVal=sl_NetAppDnsGetHostByName(sntpServer,sntpServerLen,(unsigned long *)&sntpServerIp,SL_AF_INET);
            
            //Set the Watch dog state to active 
            SysWatchDogChangeState(WATCH_DOG_WIFI_TASK_INDEX,TASK_STATE_ALIVE);    
            
        }
        if(0 <= retVal)
        {
            //load IP address in byte format
            sntpSockAddr.sa_data[2] = (_u8)((sntpServerIp >> 24) & 0xff);
            sntpSockAddr.sa_data[3] = (_u8)((sntpServerIp >> 16) & 0xff);
            sntpSockAddr.sa_data[4] = (_u8)((sntpServerIp >> 8) & 0xff);
            sntpSockAddr.sa_data[5] = (_u8) (sntpServerIp & 0xff);
            // Filling Buffer with zero for next UDp request
            memset(sntpDatagram,0,sizeof(sntpDatagram));
            // Setting Mode bits in SNTP Datagram
            sntpDatagram[0]='\x1b';
            // Open BSD socket for UDP Communication
            sntpSockId = sl_Socket(SL_AF_INET, SL_SOCK_DGRAM, SL_IPPROTO_UDP);
            if(0 <= sntpSockId)
            {
                // Enable receive timeout
                sl_SetSockOpt(sntpSockId,SL_SOL_SOCKET,SL_SO_RCVTIMEO, (_u8 *)&timeVal, sizeof(timeVal));
                //Sending SNTP request To Server
                retVal=sl_SendTo(sntpSockId,sntpDatagram,sizeof(sntpDatagram),0,
                                 &sntpSockAddr,sizeof(sntpSockAddr));
                if(0 <=retVal)
                {
                    sl_Bind(sntpSockId,(SlSockAddr_t *)&sntpLocalSockAddr,(_i16)sntpLocalSockAddrLen);
                    //Recive responce Packet
                    retVal=sl_RecvFrom(sntpSockId,sntpDatagram,sizeof(sntpDatagram),0,
                                       (SlSockAddr_t *)&sntpLocalSockAddr,&sntpLocalSockAddrLen);
                    
                    WiFiDebugPrint("sl_Recv Data: %.*s", sizeof(sntpDatagram) , sntpDatagram);
                    WiFiDebugFlush();
                    
                    if(0 <=retVal)
                    {
                        //Verify that that packet is recived from valid server
                        if( 4 ==(sntpDatagram[0] & 0x7) )
                        {
                            WiFiDebugPrint("Server Responce Recived: Sock_id:%d\r\n",sntpSockId);
                            WiFiDebugFlush();
                            //get elapse second since 19000
                            elapsedSec = sntpDatagram[40];
                            elapsedSec <<= 8;
                            elapsedSec += sntpDatagram[41];
                            elapsedSec <<= 8;
                            elapsedSec += sntpDatagram[42];
                            elapsedSec <<= 8;
                            elapsedSec += sntpDatagram[43];
                            //get elapse second since 1970
                            elapsedSec -= SECOND_SINCE_1970;
                            WiFiDebugPrint("Elapsed Second Since 1970:%u\r\n",elapsedSec);
                            WiFiDebugFlush();
                            
                            Task_disable();
                            //Update RTC Time
                            UpdateRTCTimeFromSNTP(elapsedSec);
                            //Update All Time Stamps To RTC
                            UpdateAllTimeStamps(elapsedSec);
                            Task_enable();
                            
                            //Get update information from RTC
                            SlDateTimeUpdateFromRTC(&time);
                            //Update Simplink CC3100 Date Time
                            retVal = sl_DeviceSet(SL_DEVICE_GENERAL, SL_DEVICE_GENERAL_DATE_TIME,  sizeof(SlDateTime_t),(unsigned char *)(&time));

                            if(0 > retVal)
                            {
                                WiFiDebugPrint("\n Failed To Update Simplink DATE and Time \r\n");
                                WiFiDebugFlush();

                            }
                            else
                            {
                              wifiInfoObj.SNTPTimeUpdateInterval = SNTP_TIME_UPDATE_PERIOD;
                            }
                        }
                        else
                        {
                            WiFiDebugPrint("Not A Server Responce\r\n");
                            WiFiDebugFlush();
                        }
                    }
                    else
                    {
                        WiFiDebugPrint("CC3100 SNTP Failed To Get Time Update\r\n");
                        WiFiDebugFlush();
                    }
                }
                else
                {
                    WiFiDebugPrint("CC3100 SNTP Failed To Send Time Update Request error code:%d\r\n");
                    WiFiDebugFlush();
                }
            }
            else
            {
                WiFiDebugPrint("CC3100 SNTP Socket Failed\r\n");
                WiFiDebugFlush();
            }
        }
        else
        {
            WiFiDebugPrint("CC3100 Failed to Resolve SNTP Hostname\r\n");
            WiFiDebugFlush();
        }
        //close opend socket
        sl_Close(sntpSockId);
    }

    if(0 > retVal)
    {
        // In case of failure retry After 10 seconds
        wifiInfoObj.SNTPTimeUpdateInterval = 10;
    }

}
static void SlDateTimeUpdateFromRTC(SlDateTime_t *time)
{
    DateTimeInfo_t cTime;
    GetCurrentTimeAndDate(&cTime);
    time->tm_year = cTime.date.year;
    time->tm_mon = cTime.date.month;
    time->tm_day = cTime.date.day;
    time->tm_hour = cTime.time.hours;
    time->tm_min = cTime.time.minutes;
    time->tm_sec = cTime.time.seconds;
}
void SetWifiState(WIFI_STATE_t wifiState)
{
    wifiInfoObj.wifiState = wifiState;
}
WIFI_STATE_t GetWifiState(void)
{
     return wifiInfoObj.wifiState;
}

void PostConnectMessageToWIFI()
{
  WifiMsg_t msg;
  msg.msgSender.sender = MSG_SENDER_SYS_TASK;
  msg.msgId = ESTABLISH_CONNECTION_MSG;
  msg.msgInfo = 0;
  msg.ptrData = NULL;
  
  Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
  
}

void PostOpenFileMessageToWIFI(void)
{
  WifiMsg_t msg;
  msg.msgSender.sender = MSG_SENDER_SYS_TASK;
  msg.msgId = OPEN_FIRMWARE_FILE;
  msg.msgInfo = 0;
  msg.ptrData = NULL;
  
  Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}
void PostCloseFileMessageToWIFI(void)
{
  WifiMsg_t msg;
  msg.msgSender.sender = MSG_SENDER_SYS_TASK;
  msg.msgId = CLOSE_FIRMWARE_FILE;
  msg.msgInfo = 0;
  msg.ptrData = NULL;
  
  Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}
void PostWriteFileToWIFI(void)
{
  WifiMsg_t msg;
  msg.msgSender.sender = MSG_SENDER_SYS_TASK;
  msg.msgId = COMMIT_FIRMWARE_DATA;
  msg.msgInfo = 0;
  msg.ptrData = NULL;
  
  Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}
void PostGetFileInfoToWIFI(void)
{
  WifiMsg_t msg;
  msg.msgSender.sender = MSG_SENDER_SYS_TASK;
  msg.msgId = VIEW_FILE_IN_SFLASH;
  msg.msgInfo = 0;
  msg.ptrData = NULL;
  
  Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}
void CommitParametersToFlash(void)
{
  WifiMsg_t msg;
  msg.msgSender.sender = MSG_SENDER_SYS_TASK;
  msg.msgId = COMMIT_PARAMETERS_TO_FLASH;
  msg.msgInfo = 0;
  msg.ptrData = NULL;
  if(GetParameterUpdateStatus())
  {
      Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
  }
  SetParameterUpdateStatus(false);
}

void RemoveUserInfoFromFlash(void)
{
    WifiMsg_t msg;
    msg.msgSender.sender = MSG_SENDER_SYS_TASK;
    msg.msgId = REMOVE_PARAMETERS_FROM_FLASH;
    msg.msgInfo = 0;
    msg.ptrData = NULL;
    
    Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}

void CommitCloudParametersToFlash(void)
{
    WifiMsg_t msg;
    msg.msgSender.sender = MSG_SENDER_SYS_TASK;
    msg.msgId = COMMIT_CLOUD_PARAMETER_TO_FLASH;
    msg.msgInfo = 0;
    msg.ptrData = NULL;
    
    Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}

void CommitGPSSettingFileToWIFIFlash(void)
{
  WifiMsg_t msg;
  msg.msgSender.sender = MSG_SENDER_SYS_TASK;
  msg.msgId = COMMIT_GPS_COORDINATES_TO_FLASH;
  msg.msgInfo = 0;
  msg.ptrData = NULL;
  
  Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}

void PostDelFileInfoToWIFI(void)
{
  WifiMsg_t msg;
  msg.msgSender.sender = MSG_SENDER_SYS_TASK;
  msg.msgId = DELETE_FILE_IN_SFLASH;
  msg.msgInfo = 0;
  msg.ptrData = NULL;
  
  Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}
void PostGetFileListToWIFI(void)
{
  WifiMsg_t msg;
  msg.msgSender.sender = MSG_SENDER_SYS_TASK;
  msg.msgId = VIEW_FILE_LIST_IN_SFLASH;
  msg.msgInfo = 0;
  msg.ptrData = NULL;
  
  Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}
void PostTimeUpdateMessageToWIFI(uint16_t timeElapsed)
{
  WifiMsg_t msg;
  msg.msgSender.sender = MSG_SENDER_TIMER;

  wifiInfoObj.SNTPTimeUpdateInterval -= timeElapsed;
  if(wifiInfoObj.SNTPTimeUpdateInterval <= 0)
  {
    if(GetWifiState() == WIFI_CONNECTED)
    {
      msg.msgId = UPDATE_TIME_MSG;
      Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
    }
   wifiInfoObj.SNTPTimeUpdateInterval = SNTP_TIME_UPDATE_PERIOD;
  }
}

void PostRSSIUpdateMessageToWIFI(uint16_t timeElapsed)
{
    WifiMsg_t msg;
    msg.msgSender.sender = MSG_SENDER_TIMER;
    
    wifiInfoObj.RSSIUpdateInterval -= timeElapsed;
    if(wifiInfoObj.RSSIUpdateInterval <= 0)
    {
        if(GetWifiState() == WIFI_CONNECTED)
        {
            msg.msgId = UPDATE_RSSI_MSG;
            Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
        }
        wifiInfoObj.RSSIUpdateInterval = RSSI_UPDATE_INTERVAL;
    }
}

void DispatchScanMessageToWifiTask(uint16_t timeElapsed)
{
    WifiMsg_t msg;
    msg.msgSender.sender = MSG_SENDER_TIMER;
    
    wifiInfoObj.ScanInterval -= timeElapsed;
    if(wifiInfoObj.ScanInterval <= 0)
    {        
        msg.msgSender.sender = MSG_SENDER_TIMER;
        msg.msgId = SCAN_NETWORKS_IN_AIR;
        Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
        
        wifiInfoObj.ScanInterval = WIFI_SCAN_INTERVAL;
    }
}

void UpdateWIFIRSSIStats(void)
{
    uint8_t percentageRSSI = 0;
    //normalize to 0 to 75
    int8_t rssi = (int8_t)(wifiInfoObj.wifiRxRssiInfo.rssiRxStat.AvarageDataCtrlRssi + 100);
    rssi = ((rssi > 75) ? 75 : rssi);
    
    if( (GetWifiState() == WIFI_CONNECTED) && (MAXIMUM_RSI_NORMALIZE >= rssi) && (MINIMUM_RSI_NORMALIZE <= rssi ) )
    {
        percentageRSSI = (uint8_t)((rssi/(float)MAXIMUM_RSI_NORMALIZE ) * 100);
    }
    
    InstrumentInfo.wirelessInfo.WirelessRSSI = percentageRSSI;
}

static void LoadSettingsFromDataFlash(void)
{
    LoadBatteryParameters(&InstrumentInfo.batteryInfo);
    LoadJobParameters(&InstrumentInfo.jobInfo);
    
    LoadCloudParameters(&InstrumentInfo.cloudInfo);
    UpdateHTTPHostInfo(&InstrumentInfo.cloudInfo);
        
    LoadGPSCoordinates(&GPSLastKnownInfoObj.gpsLocationInfo);
    GPSLastKnownInfoObj.gpsLocationInfo.isLockAcquired = false;
    //Convert GPS Coordinates acceptable to INET
    Task_disable();
    GpsDataConversion( &GPSLastKnownInfoObj.gpsLocationInfo,&GPSLastKnownInfoObj.gpsConvertedInfo);
    Task_enable();
}

static void SaveSettingsToDataFlash(void)
{
    BatteryInfo_t batteryInfoLocal;
    JobInfo_t jobInfoLocal;    
    
    memset(&batteryInfoLocal,0,sizeof(BatteryInfo_t));
    memset(&jobInfoLocal,0,sizeof(JobInfo_t));
    
    LoadBatteryParameters(&batteryInfoLocal);
    if(!CompareBatteryParameters(&batteryInfoLocal,&InstrumentInfo.batteryInfo))
    {
        SaveBatteryParametersToFileSystem(&InstrumentInfo.batteryInfo);
    }
    
    LoadJobParameters(&jobInfoLocal);
    if(!CompareJobParameters(&jobInfoLocal,&InstrumentInfo.jobInfo))
    {
        SaveJobParameterToFileSystem(&InstrumentInfo.jobInfo);
    }

}
static void RemoveUserInformationFromFlash(void)
{
    //Remove Netowork Credentials
    sl_WlanProfileDel(SL_WLAN_DEL_ALL_PROFILES);
    //Remove credentials from Instrument Info structure
    memset(&InstrumentInfo.networkInfo,0,sizeof(NetworkInfo_t));
    
    sl_FsDel(filePath[GPS_COORDINATES_FILE],0);
}
static void SaveGPSCoordinates(void)
{
    GPSFullInfo_t gpsLocationInfoLocal;
    memset(&gpsLocationInfoLocal,0,sizeof(GPSFullInfo_t));
    
    LoadGPSCoordinates(&gpsLocationInfoLocal);
    if(!CompareGPSCoordinates(&gpsLocationInfoLocal,&GPSLastKnownInfoObj.gpsLocationInfo))
    {
        SaveGPSCoordinatesToFileSystem(&GPSLastKnownInfoObj.gpsLocationInfo);
    }
}

static void SaveCloudParameter(void)
{
    CloudInfo_t cloudInfoLocal;
    memset(&cloudInfoLocal,0,sizeof(CloudInfo_t));
    
    LoadCloudParameters(&cloudInfoLocal);
    if(!CompareCloudParameters(&cloudInfoLocal,&InstrumentInfo.cloudInfo))
    {
        SaveCloudParametersToFileSystem(&InstrumentInfo.cloudInfo);
    }
}

static int SaveBatteryParametersToFileSystem(BatteryInfo_t *ptrToBatteryInfo)
{

    uint8_t JSON_ARRAY[JSON_STRING_LENGTH] = {0};
    
    int32_t retVal = -1;
    uint32_t MasterToken = 0;
    int32_t fileHandle = -1;
    uint32_t chunkLen;
    uint32_t remainingLen = 0;
    uint32_t movingOffset = 0;
    
    remainingLen = 
        snprintf((char *)JSON_ARRAY,1000,"{\"B\":[{\"s\":\"%s\",\"p\":\"%s\",\"m\":\"%s\",\"h\":\"%d\"}]}",
                 ptrToBatteryInfo->SerialNumber,
                 ptrToBatteryInfo->PartNumber,
                 ptrToBatteryInfo->ManufacturingDate,
                 ptrToBatteryInfo->HardwareVersion
                     );
    
    chunkLen = FIND_MIN(FILE_MAX_CHUNK_SIZE, remainingLen);
    
    fileHandle = sl_FsOpen(filePath[BATTERY_SETTINGS_FILE],SL_FS_CREATE|SL_FS_OVERWRITE | SL_FS_CREATE_FAILSAFE|SL_FS_CREATE_MAX_SIZE( remainingLen ) ,(unsigned long*)&MasterToken);
    if(0 < fileHandle)
    {
        
        do
        {
            retVal = sl_FsWrite(fileHandle, movingOffset, (_u8 *)&(JSON_ARRAY[movingOffset]),chunkLen);
            if (retVal > 0)
            {
                WiFiDebugPrint("%.*s", chunkLen,&JSON_ARRAY[movingOffset]);
                WiFiDebugFlush();
            }
            else
            {
                WiFiDebugPrint("File Writting Failed %s\r\n",filePath[BATTERY_SETTINGS_FILE]);
                WiFiDebugFlush();
            }
            remainingLen -= chunkLen;
            movingOffset += chunkLen;
            chunkLen = FIND_MIN(FILE_MAX_CHUNK_SIZE, remainingLen);
            
        }while ( (0 < chunkLen) && (0 < retVal) );
        
        sl_FsClose( fileHandle,0,0,0);
    }
    else
    {
        WiFiDebugPrint("File Opening Failed %s\r\n",filePath[BATTERY_SETTINGS_FILE]);
        WiFiDebugFlush();
    }
    return retVal;  
}

static int SaveJobParameterToFileSystem(JobInfo_t *ptrToJobInfo)
{

    uint8_t JSON_ARRAY[JSON_STRING_LENGTH] = {0};
    
    int32_t retVal = -1;
    uint32_t MasterToken = 0;
    int32_t fileHandle = -1;
    uint32_t chunkLen;
    uint32_t remainingLen = 0;
    uint32_t movingOffset = 0;
    
    remainingLen = 
        snprintf((char *)JSON_ARRAY,1000,"{\"J\":[{\"t\":\"%s\",\"j\":\"%s\"}]}",
                 ptrToJobInfo->TechniciansInitials,
                 ptrToJobInfo->JobNumber
                );
    
    chunkLen = FIND_MIN(FILE_MAX_CHUNK_SIZE, remainingLen);
    
    fileHandle = sl_FsOpen(filePath[JOB_SETTINGS_FILE],SL_FS_CREATE|SL_FS_OVERWRITE | SL_FS_CREATE_FAILSAFE|SL_FS_CREATE_MAX_SIZE( remainingLen ) ,(unsigned long*)&MasterToken);
    if(0 < fileHandle)
    {
        
        do
        {
            retVal = sl_FsWrite(fileHandle, movingOffset, (_u8 *)&(JSON_ARRAY[movingOffset]),chunkLen);
            if (retVal > 0)
            {
                WiFiDebugPrint("%.*s", chunkLen,&JSON_ARRAY[movingOffset]);
                WiFiDebugFlush();
            }
            else
            {
                WiFiDebugPrint("File Writting Failed %s\r\n",filePath[JOB_SETTINGS_FILE]);
                WiFiDebugFlush();
            }
            remainingLen -= chunkLen;
            movingOffset += chunkLen;
            chunkLen = FIND_MIN(FILE_MAX_CHUNK_SIZE, remainingLen);
            
        }while ( (0 < chunkLen) && (0 < retVal) );
        
        sl_FsClose( fileHandle,0,0,0);
    }
    else
    {
        WiFiDebugPrint("File Opening Failed %s\r\n",filePath[JOB_SETTINGS_FILE]);
        WiFiDebugFlush();
    }
    return retVal;  
}

static int SaveCloudParametersToFileSystem(CloudInfo_t *ptrCloudInfo)
{

    uint8_t JSON_ARRAY[JSON_STRING_LENGTH] = {0};
    
    int32_t retVal = -1;
    uint32_t MasterToken = 0;
    int32_t fileHandle = -1;
    uint32_t chunkLen;
    uint32_t remainingLen = 0;
    uint32_t movingOffset = 0;
    
    remainingLen = 
        snprintf((char *)JSON_ARRAY,1000,"{\"C\":[{\"u\":\"%s\",\"p\":\"%s\",\"s\":\"%s\"}]}",
                 ptrCloudInfo->CloudUserID,
                 ptrCloudInfo->CloudUserPassword,
                 ptrCloudInfo->CloudURL
                     );
    
    chunkLen = FIND_MIN(FILE_MAX_CHUNK_SIZE, remainingLen);
    
    fileHandle = sl_FsOpen(filePath[CLOUD_SETTING_FILE],SL_FS_CREATE|SL_FS_OVERWRITE | SL_FS_CREATE_FAILSAFE|SL_FS_CREATE_MAX_SIZE( remainingLen ) ,(unsigned long*)&MasterToken);
    if(0 < fileHandle)
    {
        
        do
        {
            retVal = sl_FsWrite(fileHandle, movingOffset, (_u8 *)&(JSON_ARRAY[movingOffset]),chunkLen);
            if (retVal > 0)
            {
                WiFiDebugPrint("%.*s", chunkLen,&JSON_ARRAY[movingOffset]);
                WiFiDebugFlush();
            }
            else
            {
                WiFiDebugPrint("File Writting Failed %s\r\n",filePath[CLOUD_SETTING_FILE]);
                WiFiDebugFlush();
            }
            remainingLen -= chunkLen;
            movingOffset += chunkLen;
            chunkLen = FIND_MIN(FILE_MAX_CHUNK_SIZE, remainingLen);
            
        }while ( (0 < chunkLen) && (0 < retVal) );
        
        sl_FsClose( fileHandle,0,0,0);
    }
    else
    {
        WiFiDebugPrint("File Opening Failed %s\r\n",filePath[CLOUD_SETTING_FILE]);
        WiFiDebugFlush();
    }
    return retVal;  
}


static int SaveGPSCoordinatesToFileSystem(GPSFullInfo_t *ptrToGPSInfo)
{

    uint8_t JSON_ARRAY[JSON_STRING_LENGTH] = {0};
    
    int32_t retVal=-1;
    _u32 MasterToken = 0;
    int32_t fileHandle=-1;
    uint32_t chunkLen;
    uint32_t remainingLen=0;
    uint32_t movingOffset=0;
    
    remainingLen = snprintf( (char *)JSON_ARRAY,1000,"{\"G\":[{\"lat\":%f,\"lat_dir\":\"%d\",\"lng\":\"%f\",\"lng_dir\":\"%d\",\"acc\":%f}]}",
                ptrToGPSInfo->gpsInfo.latitude,
                ptrToGPSInfo->latitudeDir,
                ptrToGPSInfo->gpsInfo.longitude,
                ptrToGPSInfo->longitudeDir,
                ptrToGPSInfo->gpsInfo.horizantalDilution
                );
    
    chunkLen = FIND_MIN(FILE_MAX_CHUNK_SIZE, remainingLen);
    
    fileHandle = sl_FsOpen(filePath[GPS_COORDINATES_FILE],SL_FS_CREATE|SL_FS_OVERWRITE | SL_FS_CREATE_FAILSAFE|SL_FS_CREATE_MAX_SIZE( remainingLen ) ,&MasterToken);
    if(0 < fileHandle)
    {  
        do
        {
            retVal = sl_FsWrite(fileHandle, movingOffset, (_u8 *)&(JSON_ARRAY[movingOffset]),chunkLen);
            if (retVal > 0)
            {
                WiFiDebugPrint("%.*s", chunkLen,&JSON_ARRAY[movingOffset]);
                //WiFiDebugPrint("%d bytes of img are transferd to CC3220 Flash...\n",(movingOffset+chunkLen) - 32);
                WiFiDebugFlush();
            }
            else
            {
                WiFiDebugPrint("File Writting Failed %s\r\n",filePath[GPS_COORDINATES_FILE]);
                WiFiDebugFlush();
            }
            remainingLen -= chunkLen;
            movingOffset += chunkLen;
            chunkLen = FIND_MIN(FILE_MAX_CHUNK_SIZE, remainingLen);
            
        }while ( (0 < chunkLen) && (0 < retVal) );  
        
        sl_FsClose( fileHandle,0,0,0);
    }
    else
    {
        WiFiDebugPrint("File Opening Failed %s",filePath[GPS_COORDINATES_FILE]);
        WiFiDebugFlush();
    }
    
    return retVal;
    
}

static int32_t LoadBatteryParameters(BatteryInfo_t *ptrToBatteryInfo)
{
    uint8_t JSON_ARRAY[JSON_STRING_LENGTH] = {0};
    int32_t retVal = -1;
    int32_t fileHandle = -1;
    uint32_t MasterToken = 0;
    
    fileHandle = sl_FsOpen(filePath[BATTERY_SETTINGS_FILE],SL_FS_READ,(unsigned long*)&MasterToken);
    if(0 < fileHandle)
    {
        retVal = sl_FsRead(fileHandle, 0, (_u8 *)JSON_ARRAY,sizeof(JSON_ARRAY));
        if(retVal > 0 )
        {
            WiFiDebugPrint("%.*s",retVal,JSON_ARRAY);
            WiFiDebugFlush();

            retVal = JParseBatteryParameters(ptrToBatteryInfo,JSON_ARRAY,retVal);
            
            if(-1 != retVal)
            {
                WiFiDebugPrint("JSON PARSING SUCESS\r\n");
                WiFiDebugFlush();
            }
            else
            {
                WiFiDebugPrint("JSON PARSING FAILED\r\n");
                WiFiDebugFlush();
            }
        }
        else
        {
            WiFiDebugPrint("File Read Failed%s\r\n",filePath[BATTERY_SETTINGS_FILE]);
            WiFiDebugFlush();
        }
        sl_FsClose( fileHandle,0,0,0);
    }
    else
    {
        WiFiDebugPrint("Fail opening File%s\r\n",filePath[BATTERY_SETTINGS_FILE]);
        WiFiDebugFlush();
        retVal = -1;
    }
    
    return retVal;  
}

static int32_t LoadJobParameters(JobInfo_t *ptrToJobInfo)
{
    uint8_t JSON_ARRAY[JSON_STRING_LENGTH] = {0};
    int32_t retVal = -1;
    int32_t fileHandle = -1;
    uint32_t MasterToken = 0;
    
    fileHandle = sl_FsOpen(filePath[JOB_SETTINGS_FILE],SL_FS_READ,(unsigned long*)&MasterToken);
    if(0 < fileHandle)
    {
        retVal = sl_FsRead(fileHandle, 0, (_u8 *)JSON_ARRAY,sizeof(JSON_ARRAY));
        if(retVal > 0 )
        {
            WiFiDebugPrint("%.*s",retVal,JSON_ARRAY);
            WiFiDebugFlush();
            
            retVal = JParseJobParameters(ptrToJobInfo,JSON_ARRAY,retVal);
            
            if(-1 != retVal)
            {
                WiFiDebugPrint("JSON PARSING SUCESS\r\n");
                WiFiDebugFlush();
            }
            else
            {
                WiFiDebugPrint("JSON PARSING FAILED For Cloud Parameters Local will be used\r\n");
                WiFiDebugFlush();
                
            }
        }
        else
        {
            WiFiDebugPrint("File Read Failed%s\r\n",filePath[JOB_SETTINGS_FILE]);
            WiFiDebugFlush();
        }
        sl_FsClose( fileHandle,0,0,0);
    }
    else
    {
        WiFiDebugPrint("Fail opening File%s\r\n",filePath[BATTERY_SETTINGS_FILE]);
        WiFiDebugFlush();
        retVal = -1;
    }
    
    return retVal; 
}

static int32_t LoadCloudParameters(CloudInfo_t *ptrToCloudInfo)
{
    uint8_t JSON_ARRAY[JSON_STRING_LENGTH] = {0};
    int32_t retVal = -1;
    int32_t fileHandle = -1;
    uint32_t MasterToken = 0;
    
    fileHandle = sl_FsOpen(filePath[CLOUD_SETTING_FILE],SL_FS_READ,(unsigned long*)&MasterToken);
    if(0 < fileHandle)
    {
        retVal = sl_FsRead(fileHandle, 0, (_u8 *)JSON_ARRAY,sizeof(JSON_ARRAY));
        if(retVal > 0 )
        {
            WiFiDebugPrint("%.*s",retVal,JSON_ARRAY);
            WiFiDebugFlush();
            
            retVal = JParseCloudParameters(ptrToCloudInfo,JSON_ARRAY,retVal);
            
            if(-1 != retVal)
            {
                WiFiDebugPrint("JSON PARSING SUCESS\r\n");
                WiFiDebugFlush();
            }
            else
            {                
                WiFiDebugPrint("JSON PARSING FAILED\r\n");
                WiFiDebugFlush();
                retVal = -1;
            }
        }
        else
        {
            WiFiDebugPrint("File Read Failed%s\r\n",filePath[CLOUD_SETTING_FILE]);
            WiFiDebugFlush();
            retVal = -1;
        }
        sl_FsClose( fileHandle,0,0,0);
    }
    else
    {
        WiFiDebugPrint("Fail opening File%s\r\n",filePath[CLOUD_SETTING_FILE]);
        WiFiDebugFlush();
        retVal = -1;
    }
    
    if( -1 == retVal )
    {
        // Load Default Parameters IN that Case
        memcpy(ptrToCloudInfo->CloudURL,CLOUD_URL,sizeof(CLOUD_URL));
        memcpy(ptrToCloudInfo->CloudUserID,CLOUD_USER_ID,sizeof(CLOUD_USER_ID));
        memcpy(ptrToCloudInfo->CloudUserPassword,CLOUD_USER_PWD,sizeof(CLOUD_USER_PWD));
    }
    
    return retVal; 
}

static int32_t LoadGPSCoordinates(GPSFullInfo_t *ptrToGPSInfo)
{
    uint8_t JSON_ARRAY[JSON_STRING_LENGTH] = {0};
    int32_t retVal = -1;
    int32_t fileHandle = -1;
    uint32_t MasterToken = 0;
   
    fileHandle = sl_FsOpen(filePath[GPS_COORDINATES_FILE],SL_FS_READ,(unsigned long *)&MasterToken);
    if(0 < fileHandle)
    {
        retVal = sl_FsRead(fileHandle, 0, (_u8 *)JSON_ARRAY,sizeof(JSON_ARRAY));
        if(retVal > 0)
        {
            WiFiDebugPrint("%.*s",retVal,JSON_ARRAY);
            WiFiDebugFlush();
            retVal = JParseGPSCoordinates(ptrToGPSInfo,JSON_ARRAY,retVal);
            if(-1 != retVal)
            {
                WiFiDebugPrint("JSON PARSING Passed\r\n");
                WiFiDebugFlush();
            }
            else
            {
                WiFiDebugPrint("JSON PARSING SUCESS\r\n");
                WiFiDebugFlush();
            }
        }
        else
        {
            WiFiDebugPrint("File Read Failed%s\r\n",filePath[GPS_COORDINATES_FILE]);
            WiFiDebugFlush();
        }
        sl_FsClose( fileHandle,0,0,0);
    }
    else
    {
        WiFiDebugPrint("File Opening Failed %s",filePath[GPS_COORDINATES_FILE]);
        WiFiDebugFlush();
    }
    
    return retVal; 
}

static int32_t JParseBatteryParameters(BatteryInfo_t *ptrToBatteryInfo,uint8_t js_data[],size_t len)
{
    jsmn_parser js;
    jsmn_init(&js);
    uint32_t size = 0;
    int32_t status = -1;
        
    int numOfToken=(sizeof(js_tok)/sizeof(js_tok[0]));
    
    status=jsmn_parse(&js,(const char *)js_data,len,js_tok,numOfToken);
    
    if(status < 0)
    {
        WiFiDebugPrint("json parsing error\n");
        WiFiDebugFlush();
    }
    else
    {
        for ( uint32_t loopCounter = 1; loopCounter < status; ++loopCounter )
        {
            size = (js_tok[loopCounter].end-js_tok[loopCounter].start);
            
            if ( (0 == strncmp((char*)&js_data[js_tok[loopCounter].start],"s",size)) && (size !=0))
            {
                size = (js_tok[loopCounter + 1].end-js_tok[loopCounter + 1].start);
                size = (size +1) > SERIAL_NUMBER_LENGTH_PLUS_END_OF_FRAME ? SERIAL_NUMBER_LENGTH_PLUS_END_OF_FRAME : (size +1) ;
                snprintf((char*)(ptrToBatteryInfo->SerialNumber), size,"%s", (char *)&js_data[js_tok[loopCounter + 1].start]);
            }
            else if (( 0 == strncmp((char*)&js_data[js_tok[loopCounter].start],"p",size)) && (size != 0))
            {
                size=(js_tok[loopCounter + 1].end-js_tok[loopCounter + 1].start);
                size = (size +1) > PART_NUMBER_LENGTH_PLUS_END_OF_FRAME ? PART_NUMBER_LENGTH_PLUS_END_OF_FRAME : (size +1);
                snprintf((char*)(ptrToBatteryInfo->PartNumber), size,"%s", (char *)&js_data[js_tok[loopCounter + 1].start]);
            }
            else if (( 0 == strncmp((char*)&js_data[js_tok[loopCounter].start],"m",size)) && (size != 0))
            {
                size=(js_tok[loopCounter + 1].end-js_tok[loopCounter + 1].start);
                size = (size +1) > DATE_LENGTH_PLUS_END_OF_FRAME ? DATE_LENGTH_PLUS_END_OF_FRAME : (size +1);
                snprintf((char*)(ptrToBatteryInfo->ManufacturingDate), size,"%s", (char *)&js_data[js_tok[loopCounter + 1].start]);
            }
            else if (( 0 == strncmp((char*)&js_data[js_tok[loopCounter].start],"h",size)) && (size != 0))
            {
                
                js_data[js_tok[loopCounter + 1].end] = NULL;
                ptrToBatteryInfo->HardwareVersion = (int)strtol( (char *)&js_data[js_tok[loopCounter + 1].start], NULL, 0);
            }
            else
            {
                WiFiDebugPrint("Unknown Json recived\r\n");
                WiFiDebugFlush();
            }
        }
    }
    return status;
}

static int32_t JParseJobParameters(JobInfo_t *ptrToJobInfo,uint8_t js_data[],size_t len)
{
    jsmn_parser js;
    jsmn_init(&js);
    uint32_t size = 0;
    int32_t status = -1;
        
    int numOfToken=(sizeof(js_tok)/sizeof(js_tok[0]));
    
    status=jsmn_parse(&js,(const char *)js_data,len,js_tok,numOfToken);
    
    if(status < 0)
    {
        WiFiDebugPrint("json parsing error\n");
        WiFiDebugFlush();
    }
    else
    {
        for ( uint32_t loopCounter = 1; loopCounter < status; ++loopCounter )
        {
            size = (js_tok[loopCounter].end-js_tok[loopCounter].start);
            
            
            if (( 0 == strncmp((char*)&js_data[js_tok[loopCounter].start],"t",size)) && (size != 0))
            {
                size=(js_tok[loopCounter + 1].end-js_tok[loopCounter + 1].start);
                size = (size +1) > TECH_INITIALS_LENGTH_PLUS_END_OF_FRAME ? TECH_INITIALS_LENGTH_PLUS_END_OF_FRAME : (size +1);
                snprintf((char*)(ptrToJobInfo->TechniciansInitials), size,"%s", (char *)&js_data[js_tok[loopCounter + 1].start]);
            }
            else if (( 0 == strncmp((char*)&js_data[js_tok[loopCounter].start],"j",size)) && (size != 0))
            {
                size=(js_tok[loopCounter + 1].end-js_tok[loopCounter + 1].start);
                size = (size +1) > JOB_NUMBER_LENGTH_PLUS_END_OF_FRAME ? JOB_NUMBER_LENGTH_PLUS_END_OF_FRAME : (size +1);
                snprintf((char*)(ptrToJobInfo->JobNumber), size,"%s", (char *)&js_data[js_tok[loopCounter + 1].start]);
            }
            else
            {
                WiFiDebugPrint("Unknown Json recived\r\n");
                WiFiDebugFlush();
            }
        }
    }
    return status;
}

static int32_t JParseCloudParameters(CloudInfo_t *ptrToCloudInfo,uint8_t js_data[],size_t len)
{
    jsmn_parser js;
    jsmn_init(&js);
    uint32_t size = 0;
    int32_t status = -1;
        
    int numOfToken=(sizeof(js_tok)/sizeof(js_tok[0]));
    
    status=jsmn_parse(&js,(const char *)js_data,len,js_tok,numOfToken);
    
    if(status < 0)
    {
        WiFiDebugPrint("json parsing error Default Cloud Param are loadded\n");
        WiFiDebugFlush();
               
    }
    else
    {
        for ( uint32_t loopCounter = 1; loopCounter < status; ++loopCounter )
        {
            size = (js_tok[loopCounter].end-js_tok[loopCounter].start);
            
            
            if (( 0 == strncmp((char*)&js_data[js_tok[loopCounter].start],"u",size)) && (size != 0))
            {
                size=(js_tok[loopCounter + 1].end-js_tok[loopCounter + 1].start);
                size = (size +1) > CLOUD_USER_ID_LENGTH_PLUS_END_OF_FRAME ? CLOUD_USER_ID_LENGTH_PLUS_END_OF_FRAME : (size +1);
                snprintf((char*)(ptrToCloudInfo->CloudUserID), size,"%s", (char *)&js_data[js_tok[loopCounter + 1].start]);
            }
            else if (( 0 == strncmp((char*)&js_data[js_tok[loopCounter].start],"p",size)) && (size != 0))
            {
                size=(js_tok[loopCounter + 1].end-js_tok[loopCounter + 1].start);
                size = (size +1) > CLOUD_USER_PASS_LENGTH_PLUS_END_OF_FRAME ? CLOUD_USER_PASS_LENGTH_PLUS_END_OF_FRAME : (size +1);
                snprintf((char*)(ptrToCloudInfo->CloudUserPassword), size,"%s", (char *)&js_data[js_tok[loopCounter + 1].start]);
            }
            else if (( 0 == strncmp((char*)&js_data[js_tok[loopCounter].start],"s",size)) && (size != 0))
            {
                size=(js_tok[loopCounter + 1].end-js_tok[loopCounter + 1].start);
                size = (size +1) > CLOUD_URL_LENGTH_PLUS_END_OF_FRAME ? CLOUD_URL_LENGTH_PLUS_END_OF_FRAME : (size +1);
                snprintf((char*)(ptrToCloudInfo->CloudURL), size,"%s", (char *)&js_data[js_tok[loopCounter + 1].start]);
            }
            
            else
            {
                WiFiDebugPrint("Unknown Json recived\r\n");
                WiFiDebugFlush();
            }
        }
    }
    return status;
}

static int32_t JParseGPSCoordinates(GPSFullInfo_t *ptrToGPSInfo,uint8_t js_data[],size_t len)
{
    
    jsmn_parser js;
    jsmn_init(&js);
    uint32_t size = 0;
    int32_t status = -1;
    uint32_t loopCounter = 0;
    
    int32_t numOfToken=(sizeof(js_tok)/sizeof(js_tok[0]));
    
    status = jsmn_parse(&js,(char*)js_data,len,js_tok,numOfToken);  
    
    if(status < 0)
    {
        WiFiDebugPrint("json parsing error\n");
        WiFiDebugFlush();
    }
    else
    {
        for ( loopCounter = 1; loopCounter < status; ++loopCounter )
        {
            size = (js_tok[loopCounter].end-js_tok[loopCounter].start);
            
            if ( (0 == strncmp( (char*)&js_data[js_tok[loopCounter].start],"lat",size)) && (size !=0) )
            {
                js_data[js_tok[loopCounter + 1].end] = NULL;
                ptrToGPSInfo->gpsInfo.latitude = (float32_t) strtof( (char*) &js_data[js_tok[loopCounter + 1].start], NULL);                
            }
            else if (( 0 == strncmp( (char*)&js_data[js_tok[loopCounter].start],"lat_dir",size)) && (size != 0) )
            {
                js_data[js_tok[loopCounter + 1].end] = NULL;
                ptrToGPSInfo->latitudeDir = (uint8_t)strtol( (char*)&js_data[js_tok[loopCounter + 1].start],NULL,0);
            }
            else if (( 0 == strncmp( (char*)&js_data[js_tok[loopCounter].start],"lng",size)) && (size != 0) )
            {
                js_data[js_tok[loopCounter + 1].end] = NULL;
                 ptrToGPSInfo->gpsInfo.longitude = (float32_t)strtof( (char*)&js_data[js_tok[loopCounter + 1].start], NULL);
            }
            else if (( 0 == strncmp( (char*)&js_data[js_tok[loopCounter].start],"lng_dir",size)) && (size != 0) )
            {
                js_data[js_tok[loopCounter + 1].end] = NULL;
                ptrToGPSInfo->longitudeDir = (uint8_t)strtol( (char*)&js_data[js_tok[loopCounter + 1].start],NULL,0);
            }
            else if (( 0 == strncmp( (char*)&js_data[js_tok[loopCounter].start],"acc",size)) && (size != 0) )
            {
                js_data[js_tok[loopCounter + 1].end] = NULL;
                ptrToGPSInfo->gpsInfo.horizantalDilution = (float32_t)strtof( (char*)&js_data[js_tok[loopCounter + 1].start], NULL);
            }
            else
            {
                WiFiDebugPrint("Unknown Json recived\r\n");
                WiFiDebugFlush();
            }
        }
    }
    return status;
    
}

static BOOLEAN CompareBatteryParameters(BatteryInfo_t *ptrToBatteryInfo1,BatteryInfo_t *ptrToBatteryInfo2)
{
    BOOLEAN isEqual = false;
    
    if( 0 == memcmp( ptrToBatteryInfo1, ptrToBatteryInfo2, sizeof(BatteryInfo_t) ) )
    {
        isEqual = true;
    }
    
    return isEqual;
}

static BOOLEAN CompareJobParameters(JobInfo_t *ptrToJobInfo1,JobInfo_t *ptrToJobInfo2)
{
    BOOLEAN isEqual = false;
    
    if( 0 == memcmp( ptrToJobInfo1, ptrToJobInfo2, sizeof(JobInfo_t) ) )
    {
        isEqual = true;
    }
    
    return isEqual;
}

static BOOLEAN CompareCloudParameters(CloudInfo_t *ptrToCloudInfo1,CloudInfo_t *ptrToCloudInfo2)
{
    BOOLEAN isEqual = false;
    
    if( 0 == memcmp( ptrToCloudInfo1, ptrToCloudInfo2, sizeof(CloudInfo_t) ) )
    {
        isEqual = true;
    }
    
    return isEqual;
}

static BOOLEAN CompareGPSCoordinates(GPSFullInfo_t *ptrToGPSInfo1,GPSFullInfo_t *ptrToGPSInfo2)
{
    BOOLEAN isEqual = false;
    ptrToGPSInfo1->isLockAcquired = ptrToGPSInfo2->isLockAcquired; 
    if( 0 == memcmp( ptrToGPSInfo1, ptrToGPSInfo2, sizeof(GPSFullInfo_t) ) )
    {
        isEqual = true;
    }
    
    return isEqual;
}

void PostAutoReconnectMsgToWifiTask(void)
{
    WifiMsg_t msg;
    msg.msgSender.sender = MSG_SENDER_SYS_TASK;
    msg.msgId = WIFI_AUTO_RECONNECT_MSG;
    Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}
void PostDisableMsgToWifiTask(void)
{
    WifiMsg_t msg;
    msg.msgSender.sender = MSG_SENDER_SYS_TASK;
    msg.msgId = WIFI_DISABLE_MSG;
    Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
}

int inet_aton(uint8_t *cp, uint32_t *ap)
{
    int dots = 0;
    uint32_t acc = 0, addr = 0;

    do {
	register char cc = *cp;

	switch (cc) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    acc = acc * 10 + (cc - '0');
	    break;

	case '.':
	    if (++dots > 3) {
		return 0;
	    }
	    /* Fall through */

	case '\0':
	    if (acc > 255) {
		return 0;
	    }
	    addr = addr << 8 | acc;
	    acc = 0;
	    break;

	default:
	    return 0;
	}
    } while (*cp++) ;

    /* Normalize the address */
    if (dots < 3) {
	addr <<= 8 * (3 - dots) ;
    }

    /* Store it if requested */
    if (ap) {
	*ap = addr;
    }

    return 1;    
}

void DispatchConnectionTestMessageToWifiTask(void)
{
  WifiMsg_t msg;
  //Only dispatch the connection test event if connected to AP
  if( WIFI_CONNECTED == GetWifiState())
  {
    msg.msgSender.sender = MSG_SENDER_SYS_TASK;
    msg.msgId = TEST_CONN_WITH_INET;
    Mailbox_post(wifiMailboxHandle, &msg, BIOS_NO_WAIT);
  }
}

static void InitiateRadioCalibration(void)
{
    uint8_t stapower = 0;
    
    if( MIN_BAT_FOR_RADIO_CALIB_IN_uV <= InstrumentInfo.vBatMon )
    {
        sl_Start(0,0,0);
        sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, SL_WLAN_GENERAL_PARAM_OPT_STA_TX_POWER,1,(_u8 *)&stapower);
        sl_Stop(0x05);
        stapower = 1;
        sl_Start(0,0,0);
        sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, SL_WLAN_GENERAL_PARAM_OPT_STA_TX_POWER,1,(_u8 *)&stapower);
        sl_Stop(0x05);
    }
}