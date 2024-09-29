#ifndef __WIFI_H
#define __WIFI_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus*/
  
#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/net/wifi/device.h>
  
#include "SysDef.h"
#include "SysTask.h"
#include "FirmwareUpload.h"
  
#define WIFI_TASK_PRIORITY              1u
#define WIFI_TASK_STACK_SIZE            4096u

#define HTTP_REQ_BUFFER_SIZE            1024u
#define HTTP_RESP_BUFFER_SIZE           1024u
#define WIFI_MAILBOX_MSG_COUNT          15

#define FILE_MAX_CHUNK_SIZE             256    
    
#define WIFI_RECONNECTION_ATTEMPTS      3u
#define WIFI_MODE_TIME_OUT              5u
    
#define SNTP_SERVER                  "pool.ntp.org"
    
#define SNTP_TIME_UPDATE_PERIOD         7200

#define RSSI_UPDATE_INTERVAL            15
#define WIFI_SCAN_INTERVAL              15
  
#define MAX_SSID_LEN_ALLOWED            32
#define WIFI_SEC_TYPE                   SL_WLAN_SEC_TYPE_WPA_WPA2

#define WIRELESS_MAC_ADDRESS_SIZE       7u
    
#define WiFiDebugPrint(format,...)      SYSTEM_PRINT(ENABLE_WIFI_DEBUG_LOG, format, ##__VA_ARGS__)
#define WiFiDebugFlush()                SYSTEM_FLUSH(ENABLE_WIFI_DEBUG_LOG)
    
#define WiFiScanDebugPrint(format,...)  SYSTEM_PRINT(ENABLE_WIFI_SCAN_DEBUG_LOG, format, ##__VA_ARGS__)
#define WiFiScanDebugFlush()            SYSTEM_FLUSH(ENABLE_WIFI_SCAN_DEBUG_LOG)

typedef enum
{
    WIFI_DISABLE_MSG=0,
    WIFI_ENABLE_MSG,
    WIFI_AUTO_RECONNECT_MSG,
    DHCP_LEASE_MSG,
    FAULT_RECOVERY_MSG,
    UPDATE_TIME_MSG,
    UPDATE_RSSI_MSG,
    ESTABLISH_CONNECTION_MSG,
    STATION_DISCONECTION_MSG,
    SEND_MSG_TO_INET,
    TEST_CONN_WITH_INET,
    VIEW_FILE_LIST_IN_SFLASH,
    VIEW_FILE_IN_SFLASH,
    DELETE_FILE_IN_SFLASH,
    OPEN_FIRMWARE_FILE,
    COMMIT_FIRMWARE_DATA,
    CLOSE_FIRMWARE_FILE,
    COMMIT_PARAMETERS_TO_FLASH,
    REMOVE_PARAMETERS_FROM_FLASH,
    COMMIT_CLOUD_PARAMETER_TO_FLASH,
    COMMIT_GPS_COORDINATES_TO_FLASH,
    SCAN_NETWORKS_IN_AIR
        
}WIFI_MSG_ID_t;

typedef struct
{
    MsjSenderInfo_t msgSender;
    WIFI_MSG_ID_t msgId;
    uint16_t msgInfo;
    void * ptrData;
}WifiMsg_t;

typedef enum
{
  WIFI_DISABLE=0,
  WIFI_ENABLE,
  WIFI_DISCONNECTED,
  WIFI_CONNECTED,
  WIFI_LOW_POWER,
}WIFI_STATE_t;

typedef enum FLASH_FILE_NAME
{
    BATTERY_SETTINGS_FILE = 0,
    JOB_SETTINGS_FILE,
    CLOUD_SETTING_FILE,
    GPS_COORDINATES_FILE,
    LAST_SETTING_FILE
}FLASH_FILE_NAME_t;

typedef struct WifiRxRssiInfo
{
    SlWlanGetRxStatResponse_t rssiRxStat;
    bool isUpdated;
}WifiRxRssiInfo_t;

typedef struct WifiDevVersionInfo
{
    SlDeviceVersion_t devVersion;
    bool isUpdated;
}WifiDevVersionInfo_t;

typedef enum SL_DHCP_CFG
{
    WIFI_DHCP_ENABLE=0,
    WIFI_DHCP_DISABLE
}WIFI_DHCP_CFG_t;

typedef struct WifiConfig
{
    uint8_t ssid[MAX_SSID_LEN_ALLOWED];
    uint8_t ssidLen;
    uint8_t key[MAX_SSID_LEN_ALLOWED];
    uint8_t keyLen;
    SlWlanSecParams_t wifiSecParam;
    SlWlanSecParamsExt_t wifiSecExtParam;
    SlNetCfgIpV4Args_t wifiNetIpv4Cfg;
    WIFI_DHCP_CFG_t wifiDhcpCfg;
}WifiConfig_t;

typedef struct
{
    WIFI_STATE_t                wifiState;
    WifiRxRssiInfo_t            wifiRxRssiInfo;
    SlNetCfgIpV4Args_t          wifiNetCfgCurrentlyAssigned;
    WifiDevVersionInfo_t        wifiDevInfo;
    uint8_t                     wifiMacAdress[WIRELESS_MAC_ADDRESS_SIZE];
    int32_t                     SNTPTimeUpdateInterval;
    int32_t                     RSSIUpdateInterval;
    int32_t                     ScanInterval;
}WifiInfo_t;

extern Mailbox_Handle wifiMailboxHandle;

void WifiTaskInit(void);
void SLSetApParam(void);
void SetWifiState(WIFI_STATE_t wifiState);
WIFI_STATE_t GetWifiState(void);

void PostConnectMessageToWIFI(void);

void PostOpenFileMessageToWIFI(void);
void PostCloseFileMessageToWIFI(void);
void PostWriteFileToWIFI(void);
void PostGetFileInfoToWIFI(void);
void PostDelFileInfoToWIFI(void);
void PostGetFileListToWIFI(void);
void CommitParametersToFlash(void);
void CommitCloudParametersToFlash(void);
void CommitGPSSettingFileToWIFIFlash(void);
void RemoveUserInfoFromFlash(void);

void PostRSSIUpdateMessageToWIFI(uint16_t timeElapsed);
void PostTimeUpdateMessageToWIFI(uint16_t imeElapsed);
void PostAutoReconnectMsgToWifiTask(void);
void PostDisableMsgToWifiTask(void);
int inet_aton(uint8_t *cp, uint32_t *ap);
void DispatchConnectionTestMessageToWifiTask(void);
void DispatchScanMessageToWifiTask(uint16_t timeElapsed);

#ifdef __cplusplus
}
#endif /* __cplusplus*/

#endif /* #ifndef __WIFI_H */