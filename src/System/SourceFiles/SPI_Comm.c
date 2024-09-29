//==============================================================================
//
//  SPI_Communication.c
//
//  Copyright (C) 2018 by Industrial Scientific.
//
//  This document and all information contained within are confidential and
//  proprietary property of Industrial Scientific Corporation. All rights
//  reserved. It is not to be reproduced or reused without the prior approval
//  of Industrial Scientific Corporation.
//
//==============================================================================
//  FILE INFORMATION
//==============================================================================
//
//  Source:        
//
//  Project:       
//
//  Author:        
//
//  Date:          
//
//  Revision:      1.0
//
//==============================================================================
//  FILE DESCRIPTION
//==============================================================================
//
//! \file
//! This module takes care of all the communication/messages between smart battery
//! board and VPRO
//
//==============================================================================
//  REVISION HISTORY
//==============================================================================
//  Revision: 1.0  
//      
//
//==============================================================================
//  INCLUDES
//==============================================================================
#include <stdint.h>
#include "Board.h"
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include "SPI_Comm.h"
#include "SysDef.h"
#include "SysTask.h"
#include "WifiTask.h"
#include "EventQ.h"
#include "GPS.h"
#include "ExtCommunication.h"
#include "WifiHTTPClient.h"
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include "FirmwareUpload.h"
#include "TimerTask.h"

#ifdef MANFACTURING_TEST_CODE  
#include "ManufacturingTestCodes.h"
#endif /*MANFACTURING_TEST_CODE*/
//==============================================================================
//	CONSTANTS, TYPEDEFS AND MACROS 
//==============================================================================


//==============================================================================
//	LOCAL DATA STRUCTURE DEFINITION
//==============================================================================
#define NETWORK_ID_BIT_FIELD_INDEX                              0
#define NETWORK_PWD_BIT_FIELD_INDEX                             1
#define NETWORK_ENC_BIT_FIELD_INDEX                             2
#define NETWORK_TYPE_BIT_FIELD_INDEX                            3

#define NETWORK_IP_BIT_FIELD_INDEX                              4
#define NETWORK_MASK_BIT_FIELD_INDEX                            5
#define NETWORK_GW_BIT_FIELD_INDEX                              6
#define NETWORK_DNS_BIT_FIELD_INDEX                             7

#define STATUS_MSG_SSID_LEN                                     16

#define NETWORK_CONFIG_VALID                               (\
                                                           ( 0X01u << NETWORK_ID_BIT_FIELD_INDEX)|\
                                                           ( 0X01u << NETWORK_ENC_BIT_FIELD_INDEX)|\
                                                           ( 0X01u << NETWORK_TYPE_BIT_FIELD_INDEX)\
                                                           )


#define NETWORK_CONFIG_DHCP_OPEN                           (\
                                                           ( 0X01u << NETWORK_ID_BIT_FIELD_INDEX)|\
                                                           ( 0X01u << NETWORK_ENC_BIT_FIELD_INDEX)|\
                                                           ( 0X01u << NETWORK_TYPE_BIT_FIELD_INDEX)\
                                                           )

#define NETWORK_CONFIG_DHCP                                (\
                                                           (NETWORK_CONFIG_DHCP_OPEN)|\
                                                           ( 0X01u << NETWORK_PWD_BIT_FIELD_INDEX)\
                                                           )
                                                     
#define NETWORK_CONFIG_STATIC_OPEN                         (\
                                                           (NETWORK_CONFIG_DHCP_OPEN)|\
                                                           ( 0X01u << NETWORK_IP_BIT_FIELD_INDEX)|\
                                                           ( 0X01u << NETWORK_MASK_BIT_FIELD_INDEX)|\
                                                           ( 0X01u << NETWORK_DNS_BIT_FIELD_INDEX)|\
                                                           ( 0X01u << NETWORK_GW_BIT_FIELD_INDEX)\
                                                           )
                                                               
#define NETWORK_CONFIG_STATIC                              (\
                                                           (NETWORK_CONFIG_STATIC_OPEN)|\
                                                           ( 0X01u << NETWORK_PWD_BIT_FIELD_INDEX)\
                                                           )

typedef union NetworkBitField
{
    struct
    {
        uint8_t isNetworkIdRcvd         : 1;
        uint8_t isNetworkPwdRcvd        : 1;
        uint8_t isNetworkEncRcvd        : 1;
        uint8_t isNetworkTypeRcvd       : 1;
        uint8_t isNetworkIpRcvd         : 1;
        uint8_t isNetworkMaskRcvd       : 1;
        uint8_t isNetworkGwRcvd         : 1;
        uint8_t isNetworkDnsRcvd        : 1;
    };
    uint8_t networkFlagsVector;
}NetworkConfigBitField_t;

typedef union CloudConfigBitField
{
    struct
    {
        uint8_t isCloudUserIDRcvd        : 1;
        uint8_t isCloudUserPasswordRcvd  : 1;
        uint8_t isCloudURLRcvd           : 1;
    };
    uint8_t cloudConfigFlagsVector;
}CloudConfigBitField_t;

typedef struct ConfigFlags
{
    NetworkConfigBitField_t networkConfigFlags;
    CloudConfigBitField_t cloudConfigFlags;
    
    BOOLEAN isCloudParameterChanged;
    BOOLEAN isStatusMessageSent;
    BOOLEAN isNetworkConfigInProgress;
    BOOLEAN isParameterChanged;
    BOOLEAN isQuickStatusMessageSent;
    BOOLEAN isRadioConfigMessageSent;
}ConfigFlags_t;


//*****************Smart Battery Firmware Upgrade Structure************//
typedef struct 
{
  int32_t CurrentSequenceNumber;
  int32_t PreviousSequenceNumber;
  int32_t RequestedPacketSequenceNumber;
  int32_t TotalNumberOfPackets;
  int32_t TotalNumberOfPacketsResent;
  int32_t RemainingPackets;
  int32_t FileCRC;
  WIRELESS_DEVICE_TYPE_t SmartBatteryType;
  FIRMWARE_FILE_TYPEt FileType;
  ACK_NACK_t AckNackByte;
  FW_PACKET_ERROR_TYPE_t ErrorType;
  uint32_t CurrentPacketLength;
  unsigned long TotalTimeTakenForFwDownload;
}FirmwareInfo_t;
//*****************Smart Battery Firmware Upgrade Structure************//

static int FWPackets_Received = 0;
FirmwareInfo_t FW;
static ConfigFlags_t configFlags = {
                                        .networkConfigFlags = {
                                                                .networkFlagsVector = 0
                                                              },
                                        .cloudConfigFlags = {
                                                                .cloudConfigFlagsVector = 0
                                                            },
                                        .isStatusMessageSent = false,
                                        .isNetworkConfigInProgress = false,
                                        .isParameterChanged = false,
                                        .isQuickStatusMessageSent = false,
                                        .isRadioConfigMessageSent = true,
                                        .isCloudParameterChanged = false
                                    };
//==============================================================================
//	GLOBAL DATA DECLARATIONS
//==============================================================================

InstInfo_t InstrumentInfo = {
                              .wirelessInfo = {
                                                .devType = WIRELESS_DEVICE_WIFI
                                              },
                              .InetStatus = INET_NOT_MONITORING_INSTRUMENT,
                              
#ifdef MANFACTURING_TEST_CODE
                              .isWDTTestRunnig =FALSE,
#endif /*MANFACTURING_TEST_CODE*/
                            };

Remote_Instrument_t RemoteUnit = {
                                  .InternalInstrumentState = 0x01<<SHUTDOWN,
                                  };

Swi_Struct swiSPISlaveStruct;
Swi_Handle swiSPISlaveHandle;
Semaphore_Struct semSPISlaveStruct;
Semaphore_Handle semSPISlaveHandle;

//==============================================================================
//	LOCAL DATA DECLARATIONS
//==============================================================================

//SPI Related Paramters 
static SPI_Handle      spiVproSlaveHandle    = NULL;
static SPI_Params      spiVproSlaveParam;
static SPI_Transaction transaction;
static BOOLEAN         isSlaveConfigured     = FALSE;

//Variable to hold Vpro state machine states 
uint8_t SlaveStateMachineCurrentState = VPRO_SLAVE_INITIALIZATION_STATE;
uint8_t SlaveStateMachinePreviousState = VPRO_SLAVE_INITIALIZATION_STATE;

//Variables to hold the data received from the SPI PEripheral of VPRO
uint8_t slaveRxBuffer[SPI_SLAVE_BUFFER_LENGTH];
uint8_t slaveTxBuffer[SPI_SLAVE_BUFFER_LENGTH];
uint8_t tempDataBuffer[10];


unsigned int MasterRequest = NO_MESSAGE_LEFT;
static int CopyOfMasterRequest = NO_MESSAGE_LEFT;

static unsigned char MasterRequestedRadioParameter = NO_PARAMETER;

uint8_t SlaveStateMachineHandler = VPRO_SLAVE_INITIALIZATION_STATE;
SYS_TASK_VPRO_STATE_MACHINE_STATES_t SlaveRequest;
static uint_least8_t CSInterruptIndex;

//==============================================================================
//	LOCAL FUNCTION PROTOTYPES
//==============================================================================
//.These Functions handels the incoming data from the master device 
static void ProcessInstrumentDataMessage(uint8_t *IncomingBuffer, uint8_t StartingIndex);
static void ProcessCannedMessage(uint8_t *IncomingBuffer, uint8_t StartingIndex);
static void ProcessProximityAlarmMessage(uint8_t *IncomingBuffer, uint8_t StartingIndex);
static void ProcessSetRadioConfigureMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex);
static void ProcessGetRadioConfigureMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex);
static void ProcessWassupMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex);
static void ProcessWassupAgainMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex);
static void ProcessStartFirmwareMessage(uint8_t *IncomingBuffer , uint8_t StartingIndex);
static void ProcessFirmwareMessage(uint8_t *IncomingBuffer , uint8_t StartingIndex);
static void ProcessCommitFirmwareMessage(uint8_t *IncomingBuffer , uint8_t StartingIndex);

#ifdef MANFACTURING_TEST_CODE
static void ProcessManufacturingTestMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex);
#endif /*MANFACTURING_TEST_CODE*/

//.These Functions handels the outgoing messages to master device 
static uint8_t BuildDataStatusMessage(uint8_t *OutgoingBuffer);
static uint8_t BuildQuickStatusMessage(uint8_t *OutgoingBuffer);
static uint8_t BuildRadioConfigureMessage(uint8_t *OutgoingBuffer);
static uint8_t BuildNoRemainingMesage(uint8_t *OutgoingBuffer);
static uint8_t BuildCustomINETMessage(uint8_t *OutgoingBuffer);
static uint8_t BuildFirmwareAckNackMessage(uint8_t *OutgoingBuffer);
//.Miscellaneous functions
static void  IsMessageAvalibleForMasterDevice(void);
static void  ResendFailedChecksumMessage(void);
static uint16_t CalculateMessageChecksum(uint8_t *buffer,unsigned short startIndex,unsigned short endIndex);
static void UpdateIntrumentData(void);
static void SPITransferCompleteFxn(SPI_Handle handle, SPI_Transaction *transaction);
//static void ChipSelectInterrupt(uint_least8_t index);
static void CreateInstrumentEvent(void);

static uint8_t BuildShutdownAckMesage(uint8_t *OutgoingBuffer);

static void SetNetworkConfigInProgress(BOOLEAN isInProgress);
BOOLEAN GetNetworkConfigInProgress(void);
//==============================================================================
//	LOCAL AND GLOBAL FUNCTIONS IMPLEMENTATION
//==============================================================================

//==============================================================================
//
//  void VproSlaveStateMachine (SYS_TASK_STATE_MACHINE_STATES_t EventType)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/26
//
//!  This function handels the state machine for VPRO slave  
//
//==============================================================================
void VproSlaveStateMachine (uint16_t EventType)
{  
  switch(EventType)
  {      
  case VPRO_SLAVE_DATA_RECV_START:
    break;
    
  case VPRO_SLAVE_DATA_RECV_STOP:
    break;
    
  case VPRO_SLAVE_CREATE_EVENT:
    EventLogDebugPrint("\n *******Cloud Message Received = %d *******\n\n", InstrumentInfo.lastSlaveCommunictionTimeStamp);
    EventLogDebugFlush();
    //Create events if master changes any state 
    CreateInstrumentEvent();
    //Update the InstrumentInfo structure 
    UpdateIntrumentData();
    break;
    
  default:
    //Do nothing 
    break;
  }
}

//==============================================================================
//
//  static void SPITransferCompleteFxn(SPI_Handle handle, SPI_Transaction *transaction)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/26
//
//!  This is the callback function for SPI Prorocol
//
//==============================================================================
static void SPITransferCompleteFxn(SPI_Handle handle, SPI_Transaction *transaction)
{
    
}

//==============================================================================
//
//  int32_t SPISlaveConfigure(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/26
//
//!  This function handels the slave initialization 
//
//==============================================================================
int32_t SPISlaveConfigure(void)
{
  int32_t retVal = 0 ;
  
  if( (false == isSlaveConfigured) )
  {
    //SPI Control Parameters
    SPI_Params_init(&spiVproSlaveParam);
    spiVproSlaveParam.transferMode          = SPI_MODE_CALLBACK;
    spiVproSlaveParam.transferCallbackFxn   = SPITransferCompleteFxn;
    spiVproSlaveParam.bitRate               = VPRO_SPI_BIT_RATE;
    spiVproSlaveParam.frameFormat           = SPI_POL0_PHA0;
    spiVproSlaveParam.mode                  = SPI_SLAVE;
    spiVproSlaveParam.dataSize              = VPRO_SPI_DATA_TRANSFER_SIZE;
    
    spiVproSlaveHandle = SPI_open(Board_SPI_SLAVE, &spiVproSlaveParam);
  }
  //SPI_transferCancel(spiVproSlaveHandle);
  return retVal;
}

//==============================================================================
//
//  void ChipSelectInterrupt(uint_least8_t index)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/09
//
//!  This function handels the GPIO iterrupt enabled earlier  
//
//==============================================================================
void ChipSelectInterrupt(uint_least8_t index)
{
  //Create variable to store pin status 
  uint8_t pinStatus = 0;
   
  //Read pin status of the master signal through CS line 
  pinStatus = GPIO_read(CORNELL_INSTRUMENT_SLAVE_SPI);
  
  //Store the Index, Interrupt will be cleared later in the SWI function
  CSInterruptIndex = index;
  
  //Check the CS pin status
  if (PIN_STATUS_HIGH == pinStatus)
  {
      if (spiVproSlaveHandle != NULL)
      {
          //Cancel the transfer SPI call
          SPI_transferCancel(spiVproSlaveHandle);
      }
      //Set the request type to be posted
      SlaveRequest = VPRO_SLAVE_DATA_RECV_STOP;  

  }
  else if(PIN_STATUS_LOW == pinStatus)
  {
        //Set the request type to be posted
    SlaveRequest = VPRO_SLAVE_DATA_RECV_START;
    
  }
  else
  {
    //Do Nothing
  }
  //Post the SWI to handle SPI activity
  Swi_post(swiSPISlaveHandle);
}

//==============================================================================
//
//  Void swiSPISlaveFxn(UArg arg0, UArg arg1)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/18
//
//!  This function handels the SWI posted in CS GPIO iterrupt 
//
//==============================================================================
Void swiSPISlaveFxn(UArg arg0, UArg arg1)
{
  UInt key;
  BOOLEAN isInstDataUpdateReq = TRUE;
  uint16_t msOffset = 0u;
  
  PRCMRTCGet(&InstrumentInfo.lastSlaveCommunictionTimeStamp, &msOffset);  
  
  key = Swi_disable();  
  
  switch(SlaveRequest)
  {      
  case VPRO_SLAVE_DATA_RECV_START:
    //Set the transaction length to be the length of buffer always 
    transaction.count = SPI_SLAVE_BUFFER_LENGTH;
    transaction.txBuf = (void *) slaveTxBuffer;
    transaction.rxBuf = (void *) slaveRxBuffer;
    //Transfer the data to SPI handle 
    SPI_transfer(spiVproSlaveHandle, &transaction);
    break;
    
  case VPRO_SLAVE_DATA_RECV_STOP:
    //Process the data received from the master device 
    isInstDataUpdateReq = ProcessIncomingmessage(slaveRxBuffer,0);
    //Reset the slave RX buffer incase of noise it shouldnt be processed again.
    memset(slaveRxBuffer, 0x00, SPI_SLAVE_BUFFER_LENGTH);
    
    if(isInstDataUpdateReq == TRUE)
    {
      //Post Mailbox to systeask for Creating Event      
      SysMsg_t SysTaskEvent;
      //Set the specific state machine to which to post the event 
      SysTaskEvent.msgId = VPRO_SLAVE_EVENT_RECEVIED;
      //Set the mailbox type to be posted
      SysTaskEvent.msgInfo = VPRO_SLAVE_CREATE_EVENT; 
      //Post the mailbox to Systask
      Mailbox_post(sysMailboxHandle, &SysTaskEvent, BIOS_NO_WAIT);
    }
    //Prepare the outgoing buffer to respond master 
    (void)ManageOutgoingMessage();
    break;
    
  default:
    //Do nothing 
    break;
  }
  
  if(0x03 == CSInterruptIndex)
  {
    //Dont try to clear interrupt when woken from LPDS 
    //Index 0x03 is only set when woken from LPDS
  }
  else
  {
    // Clear the GPIO interrupt
    GPIO_clearInt(CSInterruptIndex);
  }
  Swi_restore(key);
}

//==============================================================================
//
//  uint8_t ConfigureCSPin(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/09
//
//!  This function configures the CS pin of SPI1 as GPIO interrupt enabled pin 
//
//==============================================================================
uint8_t ConfigureCSPin(void)
{
  //Variable to hold the return statu value 
  uint8_t Status = 0u;
  //Configure CS of SPI 1 as input and interrupt enabled on both edges 
  GPIO_setConfig(CORNELL_INSTRUMENT_SLAVE_SPI,GPIO_CFG_IN_PU|GPIO_CFG_IN_INT_BOTH_EDGES);
  //Configure the callback function in case of the GPIO interrupt
  GPIO_setCallback(CORNELL_INSTRUMENT_SLAVE_SPI,ChipSelectInterrupt);
  //Enable the interrupt for the specific GPIO
  GPIO_enableInt(CORNELL_INSTRUMENT_SLAVE_SPI);
  //Return the status to the caller function 
  return Status;
}


//==============================================================================
//
//  void InitializeIntrumentData(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/03
//
//!  This function handels the Instrument Information initialization 
//
//==============================================================================
void InitializeIntrumentData(void)
{
  uint16_t msOffset = 0u;
  memset(&InstrumentInfo,0,sizeof(InstInfo_t));
  PRCMRTCGet(&InstrumentInfo.lastSlaveCommunictionTimeStamp, &msOffset);  
  
#ifdef MANFACTURING_TEST_CODE  
  memset(&ManufacturingTestsState,0,sizeof(ManufacturingTestsState));
#endif /*MANFACTURING_TEST_CODE*/
  //Initialize the InstrumentInfo structure 
  InstrumentInfo.MessgaeVersion = MSG_VERSION_ONE;
  InstrumentInfo.InetStatus = INET_NOT_MONITORING_INSTRUMENT;
  InstrumentInfo.SmartBatteryStatus = NO_CONNECTION_AVALIBLE;
  InstrumentInfo.wirelessInfo.devType = WIRELESS_DEVICE_WIFI;
}


//==============================================================================
//
//  void UpdateIntrumentData(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/05/03
//
//!  This function updates the Instrument Information
//
//==============================================================================
void UpdateIntrumentData(void)
{  
  //Update the connection status of AP & INET
  UpdateConnectionStaus();
}

//==============================================================================
//
//  BOOLEAN ProcessIncomingmessage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function handeles data extraction from incoming message according to 
//!  their type.
//
//==============================================================================
BOOLEAN ProcessIncomingmessage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
{
  uint8_t MessageType  = IncomingBuffer[StartingIndex+2];
  BOOLEAN isInstDataUpdateReq = FALSE;
  EventLogDebugPrint("\nMessage Received at : %ul\n",InstrumentInfo.lastSlaveCommunictionTimeStamp);
  EventLogDebugFlush(); 
  switch (MessageType)
  {
  case COMMAND_SEND_DATA_TO_WIFI:   
    ProcessInstrumentDataMessage(IncomingBuffer , StartingIndex);
    if( CHECKBIT ((RemoteUnit.InstrumentState),SHUTDOWN) )
    {
        //if Message is Shutdown set Master Req to Shutdown ACK Message
        MasterRequest = RESPONCE_STATUS_ACK_MESSAGE;
    }
    isInstDataUpdateReq = TRUE;
    break;
    
  case COMMAND_SEND_CANNED_MSG_TO_CLOUD:
    ProcessCannedMessage(IncomingBuffer , StartingIndex);
    break;
    
  case COMMAND_SEND_PROXIMITY_ALARM_TO_CLOUD:
    ProcessProximityAlarmMessage(IncomingBuffer , StartingIndex);
    break;
    
  case COMMAND_SEND_RADIO_CONFIGURE_MESSAGE:
    ProcessSetRadioConfigureMesssage(IncomingBuffer , StartingIndex);
    break;
    
  case COMMAND_GET_RADIO_CONFIGURE_MESSAGE:
    ProcessGetRadioConfigureMesssage(IncomingBuffer , StartingIndex);
    MasterRequest = RESPOND_RADIO_CONFIGURATION;
    break;
    
  case COMMAND_SEND_WASSUP_MESSAGE:
    ProcessWassupMesssage(IncomingBuffer , StartingIndex);
    break;
    
  case COMMAND_SEND_WASSUP_AGAIN_MESSAGE:
    ProcessWassupAgainMesssage(IncomingBuffer , StartingIndex);
    break;
    
  case COMMAND_START_RECEIVE_FIRMWARE_MESSAGE:
    ProcessStartFirmwareMessage(IncomingBuffer , StartingIndex);
    MasterRequest = RENSPOND_FW_ACK_NACK_MESSAGE;
    break;
    
  case COMMAND_RECEIVE_FIRMWARE_MESSAGE:
    ProcessFirmwareMessage(IncomingBuffer , StartingIndex);
    MasterRequest = RENSPOND_FW_ACK_NACK_MESSAGE;
    break;
    
  case COMMAND_COMMIT_FIRMWARE_MESSAGE:
    ProcessCommitFirmwareMessage(IncomingBuffer , StartingIndex);
    MasterRequest = RENSPOND_FW_ACK_NACK_MESSAGE;
    break;
    
#ifdef MANFACTURING_TEST_CODE
  case COMMAND_MANUFACTURING_TEST_MESSAGE:
    ProcessManufacturingTestMesssage(IncomingBuffer , StartingIndex);
    PostMfgTestEvent(ManufacturingTestsState.ManufacturingTestFlagsVector);
    break;
#endif /*MANFACTURING_TEST_CODE*/
    
  default:
    //Do Nothing
    break;
  }
  return isInstDataUpdateReq;
}

//==============================================================================
//
//  static void ProcessInstrumentDataMessage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function extracts the instrument informnation from the incoming message.
//
//==============================================================================
static void ProcessInstrumentDataMessage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
{
  uint16_t CalculatedChecksum = 0u;
  uint16_t ReceivedChecksum = 0u;
  uint8_t Index = 0u;
  uint8_t LoopCounter = 0u;
  uint8_t TempData = 0u;
  uint8_t SensorCount = 0u;
  
  // Extract message byte for checksum calculation
  TempData= IncomingBuffer[LENGTH_BYTE];
  
  //Received checksum is present a byte before end framing characters
  ReceivedChecksum = IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData+1];
  ReceivedChecksum = (ReceivedChecksum<<8) | (IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData]);
  
  //Calculate checksum excluding the start and end framing characters 
  CalculatedChecksum = CalculateMessageChecksum(IncomingBuffer, (unsigned short)(StartingIndex+2), (unsigned short)(StartingIndex+4+TempData));
  
  if (ReceivedChecksum == CalculatedChecksum)
  {
    // Byte 0-1 Start framing Character of the message ..Ignore
    Index ++;
    Index ++;
    
    // Byte 2 Messgae Type ..Ignore
    Index ++;
    
    // Byte 3 Version of message 
    Index ++;
    
    // Byte 4 Instrument Type
    RemoteUnit.InstrumentType = (INSTRUMENT_TYPE_t)IncomingBuffer[Index++];
    
    // Byte 5 Payload Length 
    TempData = IncomingBuffer[Index++];             //53 + (5 * n) where n is number of sensors with gas reading 
    SensorCount = (TempData - I_AM_OK_MESSAGE_SIZE)/5;
    
    if (SensorCount > MAX_SENSOR_SUPPORTED)
    {
      EventLogDebugPrint("\n************Faulty Sensor Count received: %d*********\r\n",SensorCount);
      EventLogDebugFlush(); 
      SensorCount = MAX_SENSOR_SUPPORTED;
    }
    //RemoteUnit.SensorsInfo.numberOfSensors = SensorCount;
    
    // Byte 6-21 Instrument Serial Number
    for (LoopCounter = 0; LoopCounter < SERIAL_NUMBER_LENGTH ;LoopCounter++)
    {
      if( 0x20 != IncomingBuffer[Index])
      {
        RemoteUnit.SerialNumber[LoopCounter] = IncomingBuffer[Index];
      }
      else
      {
        RemoteUnit.SerialNumber[LoopCounter] = '\0';
      }
      
      Index++;
    }
    
    // Byte 22 Message Counter
    RemoteUnit.MessageCounter = IncomingBuffer[Index++];
    
    // Byte 23 Instrument Status
    RemoteUnit.InstrumentState = IncomingBuffer[Index++];
    // Byte 24-39 Username 
    for (LoopCounter = 0; LoopCounter < USER_SITE_NAME_LENGTH ;LoopCounter++)
    {
      RemoteUnit.CurrentWorkerInfo.UserName[LoopCounter] = IncomingBuffer[Index++];
    }
    
    // Byte 40-55 Sitename 
    for (LoopCounter = 0; LoopCounter < USER_SITE_NAME_LENGTH ;LoopCounter++)
    {
      RemoteUnit.CurrentWorkerInfo.SiteName[LoopCounter] = IncomingBuffer[Index++];
    }
    
    // Byte 56-90 Sensor Data (5 Bytes per sensor )
    for (LoopCounter = 0; LoopCounter < SensorCount ;LoopCounter++)
    {
      //Byte 1: Containing Sensor Type            
      RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorType = (SENSOR_TYPES_t) IncomingBuffer[Index++];
      //Byte 2: Containing MeasuringUnit
      RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorMeasuringUnits = (GAS_MEASUREMENT_UNITS_t) IncomingBuffer[Index++];
      //Byte 3: Containing Sensor Status(High Nibble) & Measuring Resolution(Low Nibble)
      RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorStatus = (SENSOR_STATUS_t) HINIBBLE_WORD8(IncomingBuffer[Index]); 
      RemoteUnit.SensorsInfo.sensorArray[LoopCounter].DecimalPlaces = LONIBBLE_WORD8(IncomingBuffer[Index++]);
      //Byte 4: Containing Sensor Reading (High Byte)
      RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorReadingHigh = IncomingBuffer[Index++];
      //Byte 5: Containing Sensor Reading (Low Byte)
      RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorReadingLow = IncomingBuffer[Index++];
    }
    if (SensorCount == 0)
    {
      //Reset the sensors status's and sensor readings if detailed message isnt received 
      for (LoopCounter = 0; LoopCounter < MAX_SENSOR_SUPPORTED ;LoopCounter++)
      {
        //Byte 3: Containing Sensor Status(High Nibble) & Measuring Resolution(Low Nibble)
        RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorStatus = SENSOR_NORMAL; 
        
        if (RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorType == SENSOR_GAS_TYPE_O2)
        {
          //If sensor type is oxygen then set the normal reading value i-e 209
          RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorReadingHigh = 0;
          RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorReadingLow = 209;
        }
        else
        {
          //If sensor type is other then oxygen then set the normal reading value i-e 0
          RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorReadingHigh = 0;
          RemoteUnit.SensorsInfo.sensorArray[LoopCounter].SensorReadingLow = 0;
        }
      }
    }
    else
    {
      //Populate sensor data only when Vpro sends the detailed message 
      RemoteUnit.SensorsInfo.numberOfSensors = SensorCount;
    }
    
    // Byte 91-92 Checksum 
    Index ++;
    Index ++;
    
    // Byte 93-94 End framing Character of the message 
    Index ++;
    Index ++;
    
  }
  else
  {
    //Checksum failed no parsing needed
    EventLogDebugPrint("\r\n *******Checksum Failed*******\r\n");
    EventLogDebugFlush();
  }
}


//==============================================================================
//
//  static void ProcessCannedMessage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function extracts the custom message sent by the master device from 
//!  incoming message.
//
//==============================================================================
static void ProcessCannedMessage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
{    
  uint16_t CalculatedChecksum = 0u;
  uint16_t ReceivedChecksum = 0u;
  uint8_t Index = 0u;
  uint8_t LoopCounter = 0u;
  uint8_t TempData = 0u;
  
  // Extract message byte for checksum calculation
  TempData= IncomingBuffer[LENGTH_BYTE];
  
  //Received checksum is present a byte before end framing characters
  ReceivedChecksum = IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData+1];
  ReceivedChecksum = (ReceivedChecksum<<8) | (IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData]);
  
  //Calculate checksum excluding the start and end framing characters 
  CalculatedChecksum = CalculateMessageChecksum(IncomingBuffer, (unsigned short)(StartingIndex+2), (unsigned short)(StartingIndex+4+TempData));
  
  if (ReceivedChecksum == CalculatedChecksum)
  {
    // Byte 0-1 Start framing Character of the message ..//Ignore
    Index ++;
    Index ++;
    
    // Byte 2 Messgae Type ..Ignore
    Index ++;
    
    // Byte 3 Version of message ..//Ignore
    Index ++;
    
    // Byte 4 Instrument Type
    RemoteUnit.InstrumentType = (INSTRUMENT_TYPE_t)IncomingBuffer[Index++];
    
    // Byte 5 Length of payload 
    TempData = IncomingBuffer[Index++];
    
    // Byte 6-21 Instrument serial number
    for (LoopCounter = 0; LoopCounter < SERIAL_NUMBER_LENGTH ;LoopCounter++)
    {
      if( 0x20 != IncomingBuffer[Index])
      {
        RemoteUnit.SerialNumber[LoopCounter] = IncomingBuffer[Index];
      }
      else
      {
        RemoteUnit.SerialNumber[LoopCounter] = '\0';
      }
      Index++;
    }
    
    // Byte 22 Messgae counter
    RemoteUnit.MessageCounter = IncomingBuffer[Index++];
    
    // Byte 23-70 Message
    for (LoopCounter = 0; LoopCounter < TempData ;LoopCounter++)
    {
      RemoteUnit.UserCustomMessage[LoopCounter] = IncomingBuffer[Index++];
    }
    
    // Byte 71-72 Checksum ..//Ignore
    Index ++;
    Index ++;
    
    // Byte 73-74 End framing Character of the message ..//Ignore
    Index ++;
    Index ++;
  }
  else
  {
    //Checksum failed no parsing needed
    EventLogDebugPrint("\r\n *******Checksum Failed*******\r\n");
    EventLogDebugFlush();
  }
  
}
uint8_t Val = 0;

//==============================================================================
//
//  static void ProcessSetRadioConfigureMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function extracts the Radio Configuarations sent by the master device from 
//!  incoming message. 
//
//==============================================================================
static void ProcessSetRadioConfigureMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
{
  uint16_t CalculatedChecksum = 0u;
  uint16_t ReceivedChecksum = 0u;
  uint8_t Index = 0u;
  uint8_t LoopCounter = 0u;
  uint8_t ParameterType = 0u;
  uint8_t TempData = 0u;
  BOOLEAN isConfigValid = FALSE;
  
  // Extract message byte for checksum calculation
  TempData= IncomingBuffer[LENGTH_BYTE];
  
  //Received checksum is present a byte before end framing characters
  ReceivedChecksum = IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData+1];
  ReceivedChecksum = (ReceivedChecksum<<8) | (IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData]);
  
  //Calculate checksum excluding the start and end framing characters 
  CalculatedChecksum = CalculateMessageChecksum(IncomingBuffer, (unsigned short)(StartingIndex+2), (unsigned short)(StartingIndex+4+TempData));
  
  if (ReceivedChecksum == CalculatedChecksum)
  {
    // Byte 0-1 Start framing Character of the message ..//Ignore
    Index ++;
    Index ++;
    
    // Byte 2 Messgae Type ..Ignore
    Index ++;
    
    // Byte 3 Version of message ..//Ignore
    Index ++;
    
    // Byte 4 Parameter to be set
    ParameterType = IncomingBuffer[Index++];
    
    //Byte 5 Length of message 
    TempData = IncomingBuffer[Index++];
    
    // Byte 6- Onward parameter data
    switch (ParameterType)
    {
    case SERIAL_NUMBER:
        SetParameterUpdateStatus(true);
        TempData = (TempData > SERIAL_NUMBER_LENGTH)? SERIAL_NUMBER_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < TempData ;LoopCounter++)
        {
            // Read 16 Byte serial number
            InstrumentInfo.batteryInfo.SerialNumber[LoopCounter] = IncomingBuffer[Index++];
        }
        InstrumentInfo.batteryInfo.SerialNumber[LoopCounter] = NULL;
        break;
        
    case MANUFACTURING_DATE:
        SetParameterUpdateStatus(true);
        TempData = (TempData > DATE_LENGTH)? DATE_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < TempData ;LoopCounter++)
        {
            // Read 4 Byte Mfg date
            InstrumentInfo.batteryInfo.ManufacturingDate[LoopCounter] = IncomingBuffer[Index++];
        }
        InstrumentInfo.batteryInfo.ManufacturingDate[LoopCounter] = NULL;
        break;
        
    case PART_NUMBER:
        SetParameterUpdateStatus(true);
        TempData = (TempData > PART_NUMBER_LENGTH)? PART_NUMBER_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < TempData ;LoopCounter++)
        {
            // Read 16 Byte Part number
            InstrumentInfo.batteryInfo.PartNumber[LoopCounter] = IncomingBuffer[Index++];
        }
        InstrumentInfo.batteryInfo.PartNumber[LoopCounter] = NULL;
        break;
        
    case TECHNICIAN_INITIALS:
        SetParameterUpdateStatus(true);
        TempData = (TempData > TECH_INITIALS_LENGTH)? TECH_INITIALS_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < TempData ;LoopCounter++)
        {
            // Read 4 Byte Technician initials 
            InstrumentInfo.jobInfo.TechniciansInitials[LoopCounter] = IncomingBuffer[Index++];
        }
        InstrumentInfo.jobInfo.TechniciansInitials[LoopCounter] = NULL;
        break;
        
    case JOB_NUMBER:
        SetParameterUpdateStatus(true);
        TempData = (TempData > JOB_NUMBER_LENGTH)? JOB_NUMBER_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < TempData ;LoopCounter++)
        {
            // Read 8 Byte Job number
            InstrumentInfo.jobInfo.JobNumber[LoopCounter] = IncomingBuffer[Index++];
        }
        
        InstrumentInfo.jobInfo.JobNumber[LoopCounter] = NULL;
        break;
        
    case HARDWARE_VERSION:
        SetParameterUpdateStatus(true);
        // Read 1 Byte Hardware version
        InstrumentInfo.batteryInfo.HardwareVersion = IncomingBuffer[Index++];
        break;
        
    case NETWORK_ID:
        TempData = (TempData > NETWORK_ID_LENGTH)? NETWORK_ID_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < TempData ;LoopCounter++)
        {
            // Read 20 Byte Network ID
            InstrumentInfo.networkInfo.NetworkID[LoopCounter] = IncomingBuffer[Index++];
        }
        InstrumentInfo.networkInfo.NetworkID[LoopCounter] = NULL;
        
        SetNetworkConfigInProgress(true);
        //Sanity Check
        ResetNetworkFlags();
        if (InstrumentInfo.networkInfo.NetworkID[0] != NULL)
        {
            //set flag indicating that SSID is received from VPRO
            configFlags.networkConfigFlags.isNetworkIdRcvd = 1;
        }
        break;
        
    case NETWORK_PASSWORD:
        TempData = (TempData > NETWORK_PASSWORD_LENGTH)? NETWORK_PASSWORD_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < TempData ;LoopCounter++)
        {
            // Read 32 Byte Network PAssword
            InstrumentInfo.networkInfo.NetworkPassword[LoopCounter] = IncomingBuffer[Index++];
        }
        InstrumentInfo.networkInfo.NetworkPassword[LoopCounter] = NULL;
        
        configFlags.networkConfigFlags.isNetworkPwdRcvd = 1;
        break;
        
    case NETWORK_ENCRYPTION_TYPE:
        // Read 1 Byte Network Encryption Type
        InstrumentInfo.networkInfo.NetworkEncryptionType = (SMART_BATTERY_ENCRYPTION_TYPE_t)IncomingBuffer[Index++];
        //set flag indicating that Encryption type is received from VPRO
        configFlags.networkConfigFlags.isNetworkEncRcvd = 1;
        break;
        
    case NETWORK_TYPE:  
        
        InstrumentInfo.networkInfo.NetworkType = IncomingBuffer[Index++]; 
        configFlags.networkConfigFlags.isNetworkTypeRcvd = 1;
        break;
        
    case Network_Mask:
        TempData = (TempData > NETWORK_IP_LENGTH)? NETWORK_IP_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < NETWORK_IP_LENGTH ;LoopCounter++)
        {
            
            InstrumentInfo.networkInfo.NetworkMask[LoopCounter] = IncomingBuffer[Index++];
        }
        InstrumentInfo.networkInfo.NetworkMask[LoopCounter] = NULL;
        
        configFlags.networkConfigFlags.isNetworkMaskRcvd = 1;
        break;
        
    case IP_Address:
        TempData = (TempData > NETWORK_IP_LENGTH)? NETWORK_IP_LENGTH :TempData;
        
        for (LoopCounter = 0; LoopCounter < NETWORK_IP_LENGTH ;LoopCounter++)
        {
            InstrumentInfo.networkInfo.IPAddress[LoopCounter] = IncomingBuffer[Index++];
        }
        InstrumentInfo.networkInfo.IPAddress[LoopCounter] = NULL;
        configFlags.networkConfigFlags.isNetworkIpRcvd = 1;
        break;
        
    case DNS:
        TempData = (TempData > NETWORK_IP_LENGTH)? NETWORK_IP_LENGTH :TempData;
        
        for (LoopCounter = 0; LoopCounter < NETWORK_IP_LENGTH ;LoopCounter++)
        {
            InstrumentInfo.networkInfo.DNS[LoopCounter] = IncomingBuffer[Index++];
        }
        
        InstrumentInfo.networkInfo.DNS[LoopCounter] = NULL;
        configFlags.networkConfigFlags.isNetworkDnsRcvd = 1;
        break;
        
    case GATEWAY:
        TempData = (TempData > NETWORK_IP_LENGTH)? NETWORK_IP_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < NETWORK_IP_LENGTH ;LoopCounter++)
        {
            InstrumentInfo.networkInfo.Gateway[LoopCounter] = IncomingBuffer[Index++];
        }
        InstrumentInfo.networkInfo.Gateway[LoopCounter] = NULL;
        configFlags.networkConfigFlags.isNetworkGwRcvd = 1;
        break;
        
    case COMMIT_PARAM:
        if(1 == IncomingBuffer[Index])
        {
            CommitParametersToFlash();
        }
        if(0 == IncomingBuffer[Index])
        {
            RemoveUserInfoFromFlash();
        }
        break;

    case INSTRUMENT_MSG_INTERVAL:
        memcpy(&(InstrumentInfo.batteryInfo.InstrumentSPICommunicationInterval),&IncomingBuffer[Index],2);
        Index += 2;
        break;
        
    case FW_VERSION:   
        break;
        
    case FW_BUILD:
        break;
        
    case CLOUD_USER_NAME_PARAM:
        
        TempData = (TempData > CLOUD_USER_ID_LENGTH)? CLOUD_USER_ID_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < CLOUD_USER_ID_LENGTH ;LoopCounter++)
        {
            InstrumentInfo.cloudInfo.CloudUserID[LoopCounter] =  IncomingBuffer[Index++];
        }
        InstrumentInfo.cloudInfo.CloudUserID[LoopCounter] = NULL;
        configFlags.cloudConfigFlags.isCloudUserIDRcvd = 1;
        break;
        
    case CLOUD_USER_PASS_PARAM:
        
        TempData = (TempData > CLOUD_USER_PASS_LENGTH)? CLOUD_USER_PASS_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < CLOUD_USER_PASS_LENGTH ;LoopCounter++)
        {
            InstrumentInfo.cloudInfo.CloudUserPassword[LoopCounter] =  IncomingBuffer[Index++];
        }
        InstrumentInfo.cloudInfo.CloudUserPassword[LoopCounter] = NULL;
        configFlags.cloudConfigFlags.isCloudUserPasswordRcvd = 1;
        break;
        
    case CLOUD_URL_PARAM:
        
        TempData = (TempData > CLOUD_URL_LENGTH)? CLOUD_URL_LENGTH :TempData;
        for (LoopCounter = 0; LoopCounter < CLOUD_URL_LENGTH ;LoopCounter++)
        {
            InstrumentInfo.cloudInfo.CloudURL[LoopCounter] =  IncomingBuffer[Index++];
        }
        InstrumentInfo.cloudInfo.CloudURL[LoopCounter] = NULL;
        configFlags.cloudConfigFlags.isCloudURLRcvd = 1;
        break;
        
    default:
        break;
        
    }
    
    // Byte  Checksum 
    Index ++;
    Index ++;
    
    // Byte  End framing Character of the message 
    Index ++;
    Index ++;
    
  }
  else
  {
    //Checksum failed no parsing needed
    EventLogDebugPrint("\r\n *******Checksum Failed*******\r\n");
    EventLogDebugFlush();
  }
  
  //If both fields are received then post the connect message to WIFI
  if( 
     (1 == configFlags.networkConfigFlags.isNetworkIdRcvd)&&
     ( 1 == configFlags.networkConfigFlags.isNetworkEncRcvd)&&
     ( 1 == configFlags.networkConfigFlags.isNetworkTypeRcvd)
    )
  {
      
      if(NETWORK_TYPE_DHCP == InstrumentInfo.networkInfo.NetworkType)
      {
          if(ENCRYPTION_TYPE_OPEN == InstrumentInfo.networkInfo.NetworkEncryptionType)
          {
              if(
                 (NETWORK_CONFIG_DHCP_OPEN == configFlags.networkConfigFlags.networkFlagsVector) ||
                 (NETWORK_CONFIG_DHCP == configFlags.networkConfigFlags.networkFlagsVector)
                 )
              {
                  isConfigValid = true;
                  EventLogDebugPrint("\r\n *******Received SSID & PASSWORD RCVD WITH DHCP OPEN*******\r\n");
                  EventLogDebugFlush();
              }
          }
          else
          {
            if(NETWORK_CONFIG_DHCP == configFlags.networkConfigFlags.networkFlagsVector)
            {
                isConfigValid = true;
            }
            EventLogDebugPrint("\r\n *******Received SSID & PASSWORD RCVD WITH DHCP*******\r\n");
            EventLogDebugFlush();     
          }
      }
      
      if(NETWORK_TYPE_STATIC == InstrumentInfo.networkInfo.NetworkType)
      {
          if(ENCRYPTION_TYPE_OPEN == InstrumentInfo.networkInfo.NetworkEncryptionType)
          {
              if(
                 (NETWORK_CONFIG_STATIC_OPEN == configFlags.networkConfigFlags.networkFlagsVector) ||
                 (NETWORK_CONFIG_STATIC == configFlags.networkConfigFlags.networkFlagsVector)
                 )
              {
                  isConfigValid = true;
                  EventLogDebugPrint("\r\n *******Received SSID & PASSWORD RCVD WITH STATIC OPEN*******\r\n");
                  EventLogDebugFlush();
                  
              }
          }
          else
          {
            if(NETWORK_CONFIG_STATIC == configFlags.networkConfigFlags.networkFlagsVector)
            {
                isConfigValid = true;
                EventLogDebugPrint("\r\n *******Received SSID & PASSWORD RCVD WITH STATIC*******\r\n");
                EventLogDebugFlush();
            }
          }
      }
      
      if( true == isConfigValid )
      {
          SetNetworkConfigInProgress(false);
          ResetNetworkFlags();   
          PostConnectMessageToWIFI();
          PostWakeUpEventToSysTask();
      }
      
  }
  else
  {
      
  }
  
    if( 
     (1 == configFlags.cloudConfigFlags.isCloudURLRcvd)&&
     ( 1 == configFlags.cloudConfigFlags.isCloudUserIDRcvd)&&
     ( 1 == configFlags.cloudConfigFlags.isCloudUserPasswordRcvd)
    )
    {
        //Reset All Flags
        configFlags.cloudConfigFlags.cloudConfigFlagsVector = 0;
        configFlags.isCloudParameterChanged = true;
        CommitCloudParametersToFlash();
    }
}


//==============================================================================
//
//  static void ProcessGetRadioConfigureMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function extracts the Radio Configuarations requested by the master device 
//!  from incoming message. 
//
//==============================================================================
static void ProcessGetRadioConfigureMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
{
  uint8_t Index = 0u;
  
  // Byte 0-1 Start framing Character of the message ..//Ignore
  Index ++;
  Index ++;
  
  // Byte 2 Messgae Type ..Ignore
  Index ++;
  
  // Byte 3 Version of message ..//Ignore
  Index ++;
  
  // Byte 4 Parameter to be set
  MasterRequestedRadioParameter = IncomingBuffer[Index++];
  
  // Byte  End framing Character of the message 
  Index ++;
  Index ++;
  
  // Set this flag to make sure that slave send the data required by the master 
  configFlags.isRadioConfigMessageSent = false;
}


//==============================================================================
//
//  static void ProcessProximityAlarmMessage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function extracts the Alarm messages sent by the master device from
//!  incoming message. 
//
//==============================================================================
static void ProcessProximityAlarmMessage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
{
  uint16_t CalculatedChecksum = 0u;
  uint16_t ReceivedChecksum = 0u;
  uint8_t Index = 0u;
  uint8_t LoopCounter = 0u;
  uint8_t TempData = 0u;
  
  // Extract message byte for checksum calculation
  TempData= IncomingBuffer[LENGTH_BYTE];
  
  //Received checksum is present a byte before end framing characters
  ReceivedChecksum = IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData+1];
  ReceivedChecksum = (ReceivedChecksum<<8) | (IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData]);
  
  //Calculate checksum excluding the start and end framing characters 
  CalculatedChecksum = CalculateMessageChecksum(IncomingBuffer, (unsigned short)(StartingIndex+2), (unsigned short)(StartingIndex+4+TempData));
  
  if (ReceivedChecksum == CalculatedChecksum)
  {
    // Byte 0-1 Start framing Character of the message ..Ignore
    Index ++;
    Index ++;
    
    // Byte 2 Messgae Type ..Ignore
    Index ++;
    
    // Byte 3 Version of message 
    Index ++;
    
    // Byte 4 Instrument Type
    RemoteUnit.InstrumentType = (INSTRUMENT_TYPE_t)IncomingBuffer[Index++];
    
    // Byte 5 Length of payload
    TempData = IncomingBuffer[Index++];
    
    // Byte 6-21 Instrument serial number
    for (LoopCounter = 0; LoopCounter < SERIAL_NUMBER_LENGTH; LoopCounter++)
    {
      RemoteUnit.SerialNumber[LoopCounter] = IncomingBuffer[Index++];
    }
    
    // Byte 22 Messgae counter
    RemoteUnit.MessageCounter = IncomingBuffer[Index++];
    
    // Byte 23 Alarm type
    RemoteUnit.InstrumentState = IncomingBuffer[Index++];
    
    // Byte 24-39 Username 
    for (LoopCounter = 0; LoopCounter < USER_SITE_NAME_LENGTH; LoopCounter++)
    {
      RemoteUnit.CurrentWorkerInfo.UserName[LoopCounter] = IncomingBuffer[Index++];
    }
    
    // Byte 40-55 Sitename 
    for (LoopCounter = 0; LoopCounter < USER_SITE_NAME_LENGTH; LoopCounter++)
    {
      RemoteUnit.CurrentWorkerInfo.SiteName[LoopCounter] = IncomingBuffer[Index++];
    }
    
    // Byte 56 Site Security Level
    RemoteUnit.SiteSecurityLevel = IncomingBuffer[Index++];
    
    // Byte 57 User Security Level
    RemoteUnit.UserSecurityLevel = IncomingBuffer[Index++];
    
    // Byte 58-59 Checksum 
    Index ++;
    Index ++;
    
    // Byte 60-61 End framing Character of the message 
    Index ++;
    Index ++;
  }
  else
  {
    //Checksum failed no parsing needed
    EventLogDebugPrint("\r\n *******Checksum Failed*******\r\n");
    EventLogDebugFlush();
  }
}


//==============================================================================
//
//  static void ProcessWassupMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function extracts the wassup call from the incoming message 
//
//==============================================================================
static void ProcessWassupMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
{
  uint8_t Index = 0u;
  
  // Byte 0-1 Start framing Character of the message ..Ignore
  Index ++;
  Index ++;
  
  // Byte 2 Messgae Type ..Ignore
  Index ++;
  
  // Byte 3-4 End framing Character of the message 
  Index ++;
  Index ++;
  
  // This functions decides that what messgae to send to master in response to wassup 
  IsMessageAvalibleForMasterDevice();
  
}


//==============================================================================
//
//  static void ProcessWassupAgainMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function extracts the wassup again call from the incoming message 
//
//==============================================================================
static void ProcessWassupAgainMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
{   
  uint8_t Index = 0u;
  
  // Byte 0-1 Start framing Character of the message ..Ignore
  Index ++;
  Index ++;
  
  // Byte 2 Messgae Type ..Ignore
  Index ++;
  
  // Byte 3-4 End framing Character of the message 
  Index ++;
  Index ++;
  
  ResendFailedChecksumMessage();
}

//==============================================================================
//
//  static void ProcessFirmwareMessage(uint8_t *IncomingBuffer , uint8_t StartingIndex);
//
//   Author:   Tayyab Tahir
//   Date:     2018/08/03
//
//!  This function extracts the firmware Packet from incoming message 
//
//==============================================================================
static void ProcessFirmwareMessage(uint8_t *IncomingBuffer , uint8_t StartingIndex)
{
  uint16_t CalculatedChecksum = 0u;
  uint16_t ReceivedChecksum = 0u;
  uint8_t Index = 0u;
  uint8_t LoopCounter = 0u;
  uint8_t TempData = 0u;
  //uint32_t LocalSequenceNumber = 0;
  PTR_MSG_EVT_t msgEvt;
  
  //Fetch the coming sequence number for comparing to the p[revious sequence number
  /*
  LocalSequenceNumber  = (int)((unsigned char)(IncomingBuffer[7]) |
                               (unsigned char)(IncomingBuffer[8]) << 8 |
                                 (unsigned char)(IncomingBuffer[9]) << 16 |
                                   (unsigned char)(IncomingBuffer[10]) << 24);
  */
  //Set initial Response to VPRO as OK
  FW.AckNackByte = ACK;
  FW.ErrorType = NO_ERROR;
  
  // Extract message byte for checksum calculation
  TempData= IncomingBuffer[LENGTH_BYTE];
  
  //Received checksum is present a byte before end framing characters
  ReceivedChecksum = IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData+1];
  ReceivedChecksum = (ReceivedChecksum<<8) | (IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData]);
  
  //Calculate checksum excluding the start and end framing characters 
  CalculatedChecksum = (CalculateMessageChecksum(IncomingBuffer, (unsigned short)(StartingIndex+2), (unsigned short)(StartingIndex+4+TempData)));
  
  if (ReceivedChecksum == CalculatedChecksum)
  {
    // Allocate Space in Message Queue
    msgEvt = (PTR_MSG_EVT_t)AllocateEvent(sizeof(MsgEvent_t));
    //Only parse when Queue has space else request the packet again
    if(NULL != msgEvt)
    { 
      FW_LOG_DebugPrint("\n\r Queue space allocated\n\r");
      FW_LOG_EventLogDebugFlush();
      // Byte 0-1 Start framing Character of the message ..Ignore
      Index ++;
      Index ++;
      
      // Byte 2 Messgae Type ..Ignore
      Index ++;
      
      // Byte 3 Version of message 
      Index ++;;
      
      // Byte 4 Smart Battery Type
      FW.SmartBatteryType = (WIRELESS_DEVICE_TYPE_t)IncomingBuffer[Index++];
      
      // Byte 5 Length of payload
      TempData = IncomingBuffer[Index++];
      
      // Byte 6 file type
      FW.FileType = (FIRMWARE_FILE_TYPEt)IncomingBuffer[Index++];
      
      // Byte 7-10 Current Sequence or packet number
      FW.CurrentSequenceNumber  = (int32_t)((uint8_t)(IncomingBuffer[7]) |
                                            (uint8_t)(IncomingBuffer[8]) << 8 |
                                              (uint8_t)(IncomingBuffer[9]) << 16 |
                                                (uint8_t)(IncomingBuffer[10]) << 24);
          
      //memcpy(&FW.CurrentSequenceNumber,&IncomingBuffer[Index],sizeof(FW.CurrentSequenceNumber));
      Index += sizeof(FW.CurrentSequenceNumber);
      
      //Test PAcket count
      FWPackets_Received ++;
      
      // Byte  11-74 firmware packet 
      for(LoopCounter = 0; LoopCounter< (TempData-5);LoopCounter++)
      {
        //Populate firmware packet in message Queue
        msgEvt->Msg[LoopCounter] = IncomingBuffer[Index++];
      }
      
      //Specify Message size
      msgEvt->msgSize = LoopCounter-1;
      //Specify Message ID
      msgEvt->evtObj.id = EVENT_MSG_EVT;
      
      // Byte 75-76 Checksum 
      Index ++;
      Index ++;
      
      // Byte 77-78 End framing Character of the message 
      Index ++;
      Index ++;
      
      FW_LOG_DebugPrint("\n\r ** Current Sequence Number: %d Local Sequence Number: %d\n\r",FW.CurrentSequenceNumber,FW.PreviousSequenceNumber);
      FW_LOG_EventLogDebugFlush();
      
      if (FW.CurrentSequenceNumber > FW.PreviousSequenceNumber)
      {
        //Put Event to Message Quene
        PUT_EVENT_TO_FIRM_QUEUE((PTR_EVENT_t)msgEvt);
        ++FW.PreviousSequenceNumber;
        FW.RequestedPacketSequenceNumber = FW.PreviousSequenceNumber;
        FW_LOG_DebugPrint("\n\r Event Placed in Queue\n\r");
        FW_LOG_EventLogDebugFlush();
      }
      else if(FW.CurrentSequenceNumber == FW.PreviousSequenceNumber)
      { 
        FreeEvent((PTR_EVENT_t)msgEvt);
        FW_LOG_DebugPrint("\n\r Event Freed from Queue\n\r");
        FW_LOG_EventLogDebugFlush();
      }
      else if(FW.CurrentSequenceNumber < FW.PreviousSequenceNumber)
      {
        FreeEvent((PTR_EVENT_t)msgEvt);
        FW_LOG_DebugPrint("\n\r Event Freed from Queue\n\r");
        FW_LOG_EventLogDebugFlush();
      }
      //PutOneEventToQueueHead(&firmQueueInfo,(PTR_EVENT_t)msgEvt);
      //Post write Message Mailbox to Wifi
      //PostWriteFileToWIFI();
    }
    else
    {
      FW_LOG_DebugPrint("\n\r Queue space not allocated\n\r");
      FW_LOG_EventLogDebugFlush();
      //Request the packet again as no space right now 
      //Respond VPRO with NACK & Bad CRC
      FW.AckNackByte = NACK;
      FW.ErrorType = ALLOCATION_ERROR;
      FW.RequestedPacketSequenceNumber = 1 + FW.PreviousSequenceNumber;      
    }
  }
  else
  {
    //Checksum failed no parsing needed
    //Respond VPRO with NACK & Bad CRC
    FW.AckNackByte = NACK;
    FW.ErrorType = INVALID_CRC;
    //Set the requested packet to be next to last valid packet
    FW.RequestedPacketSequenceNumber = 1 + FW.PreviousSequenceNumber;  
    FW_LOG_DebugPrint("\n\rCRC Failure\n\r",Index,IncomingBuffer);
    FW_LOG_EventLogDebugFlush();
  }
    //Post write Message Mailbox to Wifi
    PostWriteFileToWIFI();
}

//==============================================================================
//
//  static void ProcessStartFirmwareMessage(uint8_t *IncomingBuffer , uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/08/03
//
//!  This function extracts the firmware start Packet from incoming message 
//
//==============================================================================
static void ProcessStartFirmwareMessage(uint8_t *IncomingBuffer , uint8_t StartingIndex)
{
  uint16_t CalculatedChecksum = 0u;
  uint16_t ReceivedChecksum = 0u;
  uint8_t Index = 0u;
  uint8_t TempData = 0u;  
  unsigned short msOffset = 0u;//..FW_Debug
  PRCMRTCGet(&FW.TotalTimeTakenForFwDownload, &msOffset);//..FW_Debug
  
  FW.CurrentSequenceNumber = 0;
  FW.PreviousSequenceNumber = 0;
  RELEASE_ALL_EVENTS_FROM_FIRM_QUEUE();
  
  //Set initial Response to VPRO as OK
  FW.AckNackByte = ACK;
  FW.ErrorType = NO_ERROR;
  //Reset the sequence number upon start
  FW.CurrentSequenceNumber = 0;
  
  //RELEASE_ALL_EVENTS_FROM_FIRM_QUEUE();
  FW.CurrentSequenceNumber = 0;  
  FW.PreviousSequenceNumber = 0;
  
  // Extract message byte for checksum calculation
  TempData= IncomingBuffer[LENGTH_BYTE];
  
  //Received checksum is present a byte before end framing characters
  ReceivedChecksum = IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData+1];
  ReceivedChecksum = (ReceivedChecksum<<8) | (IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData]);
  
  //Calculate checksum excluding the start and end framing characters 
  CalculatedChecksum = CalculateMessageChecksum(IncomingBuffer, (unsigned short)(StartingIndex+2), (unsigned short)(StartingIndex+4+TempData));
  FWPackets_Received = -1;
  if (ReceivedChecksum == CalculatedChecksum)
  {
    // Byte 0-1 Start framing Character of the message ..Ignore
    Index ++;
    Index ++;
    
    // Byte 2 Messgae Type ..Ignore
    Index ++;
    
    // Byte 3 Version of message 
    Index ++;
    
    // Byte 4 Smart Battery Type
    FW.SmartBatteryType = (WIRELESS_DEVICE_TYPE_t)IncomingBuffer[Index++];
    
    // Byte 5 Length of payload
    TempData = IncomingBuffer[Index++];
    
    // Byte 6 file type
    FW.FileType = (FIRMWARE_FILE_TYPEt)IncomingBuffer[Index++];

    // Byte 7-10 Current Sequence or packet number   
    FW.TotalNumberOfPackets  = (int32_t)((uint8_t)(IncomingBuffer[7]) |
                                         (uint8_t)(IncomingBuffer[8]) << 8 |
                                           (uint8_t)(IncomingBuffer[9]) << 16 |
                                             (uint8_t)(IncomingBuffer[10]) << 24);
    
    Index += sizeof(FW.TotalNumberOfPackets);
    
    // Byte 11-14 File CRC
    FW.FileCRC  = (int32_t)((uint8_t)(IncomingBuffer[11]) |
                            (uint8_t)(IncomingBuffer[12]) << 8 |
                              (uint8_t)(IncomingBuffer[13]) << 16 |
                                (uint8_t)(IncomingBuffer[14]) << 24);
    
    Index += sizeof(FW.FileCRC);
    
    // Byte 15-16 Checksum 
    Index ++;
    Index ++;
    
    // Byte 17-18 End framing Character of the message 
    Index ++;
    Index ++;
    PostOpenFileMessageToWIFI();
  }
  else
  {
    //Checksum failed no parsing needed
    //Respond VPRO with NACK & Bad CRC
    FW.AckNackByte = NACK;
    FW.ErrorType = INVALID_CRC;
    FW_LOG_DebugPrint("\n\rCRC Failure\n\r",Index,IncomingBuffer);
    FW_LOG_EventLogDebugFlush();
  }
}

//==============================================================================
//
//  static void ProcessCommitFirmwareMessage(uint8_t *IncomingBuffer , uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/08/03
//
//!  This function extracts the firmware commit Packet from incoming message 
//
//==============================================================================
static void ProcessCommitFirmwareMessage(uint8_t *IncomingBuffer , uint8_t StartingIndex)
{
  uint16_t CalculatedChecksum = 0u;
  uint16_t ReceivedChecksum = 0u;
  uint8_t Index = 0u;
  uint8_t TempData = 0u;
  unsigned long secondsSince1970 = 0u;
  unsigned short msOffset = 0u;
  PRCMRTCGet(&secondsSince1970, &msOffset);
  FW.TotalTimeTakenForFwDownload = secondsSince1970 - FW.TotalTimeTakenForFwDownload;
  //Set initial Response to VPRO as OK
  FW.AckNackByte = ACK;
  FW.ErrorType = NO_ERROR;
  
  // Extract message byte for checksum calculation
  TempData= IncomingBuffer[LENGTH_BYTE];
  
  //Received checksum is present a byte before end framing characters
  ReceivedChecksum = IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData+1];
  ReceivedChecksum = (ReceivedChecksum<<8) | (IncomingBuffer[StartingIndex+LENGTH_BYTE+TempData]);
  
  //Calculate checksum excluding the start and end framing characters 
  CalculatedChecksum = (CalculateMessageChecksum(IncomingBuffer, (unsigned short)(StartingIndex+2), (unsigned short)(StartingIndex+4+TempData)));
  
  if (ReceivedChecksum == CalculatedChecksum)
  {
    // Byte 0-1 Start framing Character of the message ..Ignore
    Index ++;
    Index ++;
    
    // Byte 2 Messgae Type ..Ignore
    Index ++;
    
    // Byte 3 Version of message 
    Index ++;
    
    // Byte 4 Smart Battery Type
    FW.SmartBatteryType = (WIRELESS_DEVICE_TYPE_t)IncomingBuffer[Index++];
    
    // Byte 5 Length of payload
    TempData = IncomingBuffer[Index++];
    
    // Byte 6 file type
    FW.FileType = (FIRMWARE_FILE_TYPEt)IncomingBuffer[Index++];

    // Byte 7-10 Current Sequence or packet number
    FW.TotalNumberOfPacketsResent  = (int32_t)((uint8_t)(IncomingBuffer[11]) |
                                               (uint8_t)(IncomingBuffer[12]) << 8 |
                                                 (uint8_t)(IncomingBuffer[13]) << 16 |
                                                   (uint8_t)(IncomingBuffer[14]) << 24);
    
    Index += sizeof(FW.TotalNumberOfPackets);
    
    // Byte 11-14 File CRC
    FW.FileCRC  = (int32_t)((uint8_t)(IncomingBuffer[11]) |
                            (uint8_t)(IncomingBuffer[12]) << 8 |
                              (uint8_t)(IncomingBuffer[13]) << 16 |
                                (uint8_t)(IncomingBuffer[14]) << 24);
 
    Index += sizeof(FW.FileCRC);
    
    // Byte 15-16 Checksum 
    Index ++;
    Index ++;
    
    // Byte 17-18 End framing Character of the message 
    Index ++;
    Index ++;
    PostCloseFileMessageToWIFI();
  }
  else
  {
    //Checksum failed no parsing needed
    //Respond VPRO with NACK & Bad CRC
    FW.AckNackByte = NACK;
    FW.ErrorType = INVALID_CRC;
    FW_LOG_DebugPrint("\n\rCRC Failure\n\r",Index,IncomingBuffer);
    FW_LOG_EventLogDebugFlush();
  }
}

#ifdef MANFACTURING_TEST_CODE
//==============================================================================
//
//  static void ProcessManufacturingTestMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2019/01/29
//
//!  This function extracts the manufacturing test to run 
//
//==============================================================================
static void ProcessManufacturingTestMesssage(uint8_t *IncomingBuffer, uint8_t StartingIndex)
{
  uint8_t Index = 0u;
  
  // Byte 0-1 Start framing Character of the message ..Ignore
  Index ++;
  Index ++;
  
  // Byte 2 Messgae Type ..Ignore
  Index ++;
  
  // Byte 3-4 Manufacturing tests to run
  ManufacturingTestsState.ManufacturingTestFlagsVector = (uint16_t)((uint8_t)(IncomingBuffer[Index++]) |
                                                                    (uint8_t)(IncomingBuffer[Index++]) << 8);
  
  // Byte 5-6 End framing Character of the message 
  Index ++;
  Index ++;
}
#endif /*MANFACTURING_TEST_CODE*/

//==============================================================================
//
//  static void  IsMessageAvalibleForMasterDevice()
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function decides which message to be sent upon wassup message  
//
//==============================================================================
static void  IsMessageAvalibleForMasterDevice()
{
  if (MasterRequest == NO_MESSAGE_LEFT)
  {
    if (configFlags.isStatusMessageSent == false)
    {
      //Prepare the Status Message in response to the first wassup message 
      MasterRequest = RESPOND_STATUS_MSG;
      //Reset the status message flag to ensure that it is sent 
      configFlags.isStatusMessageSent = true;
    }
    else if(configFlags.isQuickStatusMessageSent == false)
    {
      //Prepare the Quick status Message upon wassup message 
      MasterRequest = RESPOND_QUICK_STATUS_MSG;
      //Reset the quick status message flag to ensure that it is sent 
      configFlags.isQuickStatusMessageSent = true;
    }
    else if(configFlags.isRadioConfigMessageSent == false)
    {
      //Prepare the radio configuration message requested by vpro
      MasterRequest = RESPOND_RADIO_CONFIGURATION;
      //Reset the flag to ensure that setting message is being sent to vpro
      configFlags.isRadioConfigMessageSent = true;
    }
    else if(InstrumentInfo.IsINETMessageAvalibleforVpro == true)
    {
      //Prepare the Inet message for VPRO
      MasterRequest = PASS_INET_MESSAGE;
      //Reset the flag to ensure that Inet message is being sent to vpro 
      InstrumentInfo.IsINETMessageAvalibleforVpro = false;
    }
    else 
    {
      MasterRequest = NO_MESSAGE_LEFT;
      //Send the status message in the next transaction 
      configFlags.isStatusMessageSent = false;
      //Send the quick status message in the next transaction 
      configFlags.isQuickStatusMessageSent = false;
    }
  }
  
  //Save copy of master message for resending purpose
  CopyOfMasterRequest = MasterRequest;
}

//==============================================================================
//
//  static void  ResendFailedChecksumMessage(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function decides which message to be sent upon wassup again message  
//
//==============================================================================
static void  ResendFailedChecksumMessage(void)
{
  //Resend the last message that have been recorded
  MasterRequest = CopyOfMasterRequest;
}

//==============================================================================
//
//  uint8_t ManageOutgoingMessage(void)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function manages the response to be sent to master device upon request  
//
//==============================================================================
uint8_t ManageOutgoingMessage(void)
{
  uint32_t tempData = MasterRequest;
  uint8_t ReturnLength = 0u;
  
  switch (tempData)
  {
  case RESPOND_STATUS_MSG:
    //Reset the Transmit buffer before sending the data 
    memset(slaveTxBuffer,NULL,SPI_SLAVE_BUFFER_LENGTH);
    //Construct the requested the string  
    ReturnLength = BuildDataStatusMessage(slaveTxBuffer);
    break;
    
  case RESPOND_RADIO_CONFIGURATION:  
    //Reset the Transmit buffer before sending the data 
    memset(slaveTxBuffer,NULL,SPI_SLAVE_BUFFER_LENGTH);
    //Construct the requested the string
    ReturnLength = BuildRadioConfigureMessage(slaveTxBuffer);
    break;
    
  case RESPOND_QUICK_STATUS_MSG:
    //Reset the Transmit buffer before sending the data 
    memset(slaveTxBuffer,NULL,SPI_SLAVE_BUFFER_LENGTH);
    //Construct the requested the string
    ReturnLength = BuildQuickStatusMessage(slaveTxBuffer);
    break;
    
  case PASS_INET_MESSAGE:
    //Reset the Transmit buffer before sending the data 
    memset(slaveTxBuffer,NULL,SPI_SLAVE_BUFFER_LENGTH);
    //Construct the requested the string
    ReturnLength = BuildCustomINETMessage(slaveTxBuffer);
    break;
    
  case NO_MESSAGE_LEFT:
    //Reset the Transmit buffer before sending the data 
    memset(slaveTxBuffer,NULL,SPI_SLAVE_BUFFER_LENGTH);
    //Construct the requested the string
    ReturnLength = BuildNoRemainingMesage(slaveTxBuffer);
    break;
    
  case RENSPOND_FW_ACK_NACK_MESSAGE:
    //Reset the Transmit buffer before sending the data 
    memset(slaveTxBuffer,NULL,SPI_SLAVE_BUFFER_LENGTH);
    //Construct the requested the string
    ReturnLength = BuildFirmwareAckNackMessage(slaveTxBuffer);
    break;
    
  case RESPONCE_STATUS_ACK_MESSAGE:
      //Reset the Transmit buffer before sending the data 
      memset(slaveTxBuffer,NULL,SPI_SLAVE_BUFFER_LENGTH);
      //Construct the requested the string
      ReturnLength = BuildShutdownAckMesage(slaveTxBuffer);
      break;
    
  default:
    break;
  }
  
  //Reset Master Request Message 
  MasterRequest = NO_MESSAGE_LEFT;
  
  return ReturnLength;
}

//==============================================================================
//
//  static void BuildDataStatusMessage(uint8_t *OutgoingBuffer)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function writes the status message to the outgoing buffer   
//
//==============================================================================
static uint8_t BuildDataStatusMessage(uint8_t *OutgoingBuffer)
{
  uint8_t Index = 0u;
  uint8_t LoopCouter = 0u;
  uint8_t returnLength = 0u;
  uint16_t messageChecksum = 0u;
  
  uint32_t secondsSince1970 = 0u;
  uint16_t msOffset = 0u;
  PRCMRTCGet(&secondsSince1970, &msOffset);
  
  // Byte 0-1 Start framing Character of the message 
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  
  // Byte 2 Message Type: normal status message
  OutgoingBuffer[Index++] = SEND_READ_STATUS_MESSAGE;
  
  //Byte 3 Version
  OutgoingBuffer[Index++]  = InstrumentInfo.MessgaeVersion;
  
  //Byte 4 Battery Status 
  OutgoingBuffer[Index++] = InstrumentInfo.SmartBatteryStatus ;
  
  //Byte 5 INet Status 
  OutgoingBuffer[Index++] = InstrumentInfo.InetStatus;
  
  //Byte 6-7 RSSI
  OutgoingBuffer[Index++] = 0;
  OutgoingBuffer[Index++] = InstrumentInfo.wirelessInfo.WirelessRSSI;
  
  //Byte 8-23 Serial Number
  for(LoopCouter = 0; LoopCouter < SERIAL_NUMBER_LENGTH ;LoopCouter++)
  {
    OutgoingBuffer[Index++] = InstrumentInfo.batteryInfo.SerialNumber[LoopCouter];
  }
  
  // Byte 24-39 Network ID
  for(LoopCouter = 0; LoopCouter < STATUS_MSG_SSID_LEN ;LoopCouter++)
  {
    OutgoingBuffer[Index++] = InstrumentInfo.networkInfo.NetworkID[LoopCouter];
  }
  
  //Byte 40-43 Current Time
  OutgoingBuffer[Index++] = 0xFF & (secondsSince1970 >> 24);
  OutgoingBuffer[Index++] = 0xFF & (secondsSince1970 >> 16); 
  OutgoingBuffer[Index++] = 0xFF & (secondsSince1970 >>  8);
  OutgoingBuffer[Index++] = 0xFF & (secondsSince1970);
  
  // Byte 44-47 GPS Latitude 
  memcpy(&(OutgoingBuffer[Index]),&(GPSLastKnownInfoObj.gpsLocationInfo.gpsInfo.latitude),sizeof(float32_t));
  Index = Index+4;
  
  // Byte 48 GPS Latitude Direction
  OutgoingBuffer[Index++] = GPSLastKnownInfoObj.gpsLocationInfo.latitudeDir;
  
  // Byte 49-52 GPS Longitude
  memcpy(&(OutgoingBuffer[Index]),&(GPSLastKnownInfoObj.gpsLocationInfo.gpsInfo.longitude),sizeof(float32_t));
  Index = Index+4;
  
  // Byte 53 GPS Longitude Direction
  OutgoingBuffer[Index++] = GPSLastKnownInfoObj.gpsLocationInfo.longitudeDir;
  OutgoingBuffer[Index++] = GPSLastKnownInfoObj.gpsLocationInfo.isLockAcquired;
  
  // Byte 54-55 CheckSum of the message
  messageChecksum = CalculateMessageChecksum(OutgoingBuffer, (unsigned short)2, (unsigned short)(Index-1));
  
  OutgoingBuffer[Index++] = LOBYTE_WORD16(messageChecksum);
  OutgoingBuffer[Index++] = HIBYTE_WORD16(messageChecksum);
  
  // Byte 56-57 Start framing Character of the message 
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER;
  
  returnLength = Index-1;
  return returnLength;
}


//==============================================================================
//
//  static void BuildQuickStatusMessage(uint8_t *OutgoingBuffer)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function writes the quick status message to the outgoing buffer   
//
//==============================================================================
static uint8_t BuildQuickStatusMessage(uint8_t *OutgoingBuffer)
{
  uint8_t Index = 0u;
  uint8_t LoopCouter = 0u;
  uint8_t returnLength = 0u;
  uint16_t messageChecksum = 0u;
  
  // Byte 0-1 Start framing Character of the message 
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  
  // Byte 2 Message Type: quick status message
  OutgoingBuffer[Index++] = SEND_READ_QUICK_STATUS_MESSAGE;
  
  //Byte 3 Version
  OutgoingBuffer[Index++]  = InstrumentInfo.MessgaeVersion;
  
  //Byte 4 Battery Status 
  OutgoingBuffer[Index++] = InstrumentInfo.SmartBatteryStatus;
  
  //Byte 5 INet Status 
  OutgoingBuffer[Index++] = InstrumentInfo.InetStatus;
  
  //Byte 6-7 RSSI
  OutgoingBuffer[Index++] = 0;
  OutgoingBuffer[Index++] = InstrumentInfo.wirelessInfo.WirelessRSSI;
  
  //Byte 8 Cloud Message
  OutgoingBuffer[Index++] = InstrumentInfo.CloudMessageStatus;
  
  //Byte 9-24 Network ID
  for(LoopCouter = 0; LoopCouter < STATUS_MSG_SSID_LEN ;LoopCouter++)
  {
    OutgoingBuffer[Index++] = InstrumentInfo.networkInfo.NetworkID[LoopCouter];
  }
  
  // Byte 24-28 GPS Latitude 
  memcpy(&(OutgoingBuffer[Index]),&(GPSLastKnownInfoObj.gpsLocationInfo.gpsInfo.latitude),sizeof(float32_t));
  Index = Index+4;
  
  // Byte 29 GPS Latitude Direction
  OutgoingBuffer[Index++] = GPSLastKnownInfoObj.gpsLocationInfo.latitudeDir;
  
  // Byte 30-33 GPS Longitude
  memcpy(&(OutgoingBuffer[Index]),&(GPSLastKnownInfoObj.gpsLocationInfo.gpsInfo.longitude),sizeof(float32_t));
  Index = Index+4;
  
  // Byte 34 GPS Longitude Direction
  OutgoingBuffer[Index++] = GPSLastKnownInfoObj.gpsLocationInfo.longitudeDir;
  OutgoingBuffer[Index++] = GPSLastKnownInfoObj.gpsLocationInfo.isLockAcquired;
  // Byte 35-37 Firmware Version
  OutgoingBuffer[Index++] = FIRMWARE_VERSION_MAJOR;
  OutgoingBuffer[Index++] = FIRMWARE_VERSION_MINOR;
  OutgoingBuffer[Index++] = FIRMWARE_VERSION_BUILD;
  
  // Byte 35-36 CheckSum of the message
  messageChecksum = CalculateMessageChecksum(OutgoingBuffer, (unsigned short)2, (unsigned short)(Index-1));
  
  OutgoingBuffer[Index++] = LOBYTE_WORD16(messageChecksum);
  OutgoingBuffer[Index++] = HIBYTE_WORD16(messageChecksum);
  
  // Byte 37-38 Start framing Character of the message 
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER;
  
  returnLength = Index-1;
  return returnLength;
}


//==============================================================================
//
//  static void BuildRadioConfigureMessage(uint8_t *OutgoingBuffer)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function writes the requested parameter value to the outgoing buffer   
//
//==============================================================================
static uint8_t BuildRadioConfigureMessage(uint8_t *OutgoingBuffer)
{
  uint8_t Index = 0u;
  uint8_t LoopCounter = 0u;
  uint8_t returnLength = 0u;
  uint16_t messageChecksum = 0u;
  
  //Get the latest SNR value from GPS
  InstrumentInfo.gpsSNR = GetGpsSnr();  
  
  // Byte 0-1 Start framing Character of the message 
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  
  // Byte 2 Message Type: Radio configure message
  OutgoingBuffer[Index++] = RESPOND_GET_RADIO_CONFIGURE_MESSAGE;
  
  //Byte 3 Version/Config 
  OutgoingBuffer[Index++]  = InstrumentInfo.MessgaeVersion;
  
  //Byte 4 Parameters to be read 
  OutgoingBuffer[Index++]  = MasterRequestedRadioParameter;
  
  //Byte 5 Payload length 
  OutgoingBuffer[Index++] = 0x00;
  
  
  //Byte 6 Payload (Requested Radio parameter)
  switch (MasterRequestedRadioParameter)
  {
  case SERIAL_NUMBER:
      for (LoopCounter = 0; LoopCounter < SERIAL_NUMBER_LENGTH ;LoopCounter++)
      {
          // Read 16 Byte serial number
          OutgoingBuffer[Index++] = InstrumentInfo.batteryInfo.SerialNumber[LoopCounter];
      }
      //Populate length Byte left earlier
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = SERIAL_NUMBER_LENGTH + ONE_BYTE_LENGTH;
      break;
      
  case MANUFACTURING_DATE:
      for (LoopCounter = 0; LoopCounter < DATE_LENGTH ;LoopCounter++)
      {
          // Read 4 Byte Mfg date
          OutgoingBuffer[Index++] = InstrumentInfo.batteryInfo.ManufacturingDate[LoopCounter];
      }
      //Populate length Byte left earlier
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = DATE_LENGTH + ONE_BYTE_LENGTH;
      break;
      
  case PART_NUMBER:
      for (LoopCounter = 0; LoopCounter < PART_NUMBER_LENGTH ;LoopCounter++)
      {
          // Read 16 Byte Part number
          OutgoingBuffer[Index++] = InstrumentInfo.batteryInfo.PartNumber[LoopCounter];
      }
      //Populate length Byte left earlier
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = PART_NUMBER_LENGTH + ONE_BYTE_LENGTH;
      break;
      
  case TECHNICIAN_INITIALS:
      for (LoopCounter = 0; LoopCounter < TECH_INITIALS_LENGTH ;LoopCounter++)
      {
          // Read 4 Byte Technician initials 
          OutgoingBuffer[Index++] =  InstrumentInfo.jobInfo.TechniciansInitials[LoopCounter];
      }
      //Populate length Byte left earlier
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = TECH_INITIALS_LENGTH + ONE_BYTE_LENGTH;
      break;
      
  case JOB_NUMBER:
      for (LoopCounter = 0; LoopCounter < JOB_NUMBER_LENGTH ;LoopCounter++)
      {
          // Read 8 Byte Job number
          OutgoingBuffer[Index++] =  InstrumentInfo.jobInfo.JobNumber[LoopCounter];
      }
      //Populate length Byte left earlier
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = JOB_NUMBER_LENGTH + ONE_BYTE_LENGTH;
      break;
      
  case HARDWARE_VERSION:
      // Read 1 Byte Hardware version
      OutgoingBuffer[Index++] = InstrumentInfo.batteryInfo.HardwareVersion;
      //Populate length Byte left earlier
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = 1 + ONE_BYTE_LENGTH;
      break;
      
  case NETWORK_ID:
      for (LoopCounter = 0; LoopCounter < NETWORK_ID_LENGTH ;LoopCounter++)
      {
          // Read 20 Byte Network ID
          OutgoingBuffer[Index++] =  InstrumentInfo.networkInfo.NetworkID[LoopCounter];
      }
      //Populate length Byte left earlier
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = NETWORK_ID_LENGTH + ONE_BYTE_LENGTH;
      break;
      
  case NETWORK_PASSWORD:
      for (LoopCounter = 0; LoopCounter < NETWORK_PASSWORD_LENGTH ;LoopCounter++)
      {
          // Read 32 Byte Network PAssword
          OutgoingBuffer[Index++] =  InstrumentInfo.networkInfo.NetworkPassword[LoopCounter];
      }
      //Populate length Byte left earlier
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = NETWORK_PASSWORD_LENGTH + ONE_BYTE_LENGTH;
      break;
      
  case NETWORK_ENCRYPTION_TYPE:
      // Read 1 Byte Network Encryption Type
      OutgoingBuffer[Index++] = InstrumentInfo.networkInfo.NetworkEncryptionType;
      //Populate length Byte left earlier
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = 1 + ONE_BYTE_LENGTH;
      break;
      
  case BATTERY_TYPE:   
      // Read 1 Byte Network Encryption Type
      OutgoingBuffer[Index++] = InstrumentInfo.wirelessInfo.devType;
      //Populate length Byte left earlier
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = 1 + ONE_BYTE_LENGTH;
      break;
      
  case NETWORK_TYPE:  
      
      OutgoingBuffer[Index++] = InstrumentInfo.networkInfo.NetworkType;
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = 1 + ONE_BYTE_LENGTH;
      
      break;
  case Network_Mask:   
      for (LoopCounter = 0; LoopCounter < NETWORK_IP_LENGTH ;LoopCounter++)
      {
          
          OutgoingBuffer[Index++] = InstrumentInfo.networkInfo.NetworkMask[LoopCounter];
      }
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = NETWORK_IP_LENGTH + ONE_BYTE_LENGTH;
      break;
  case IP_Address:   
      for (LoopCounter = 0; LoopCounter < NETWORK_IP_LENGTH ;LoopCounter++)
      {
          
          OutgoingBuffer[Index++] = InstrumentInfo.networkInfo.IPAddress[LoopCounter];
      }
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = NETWORK_IP_LENGTH + ONE_BYTE_LENGTH;
      break;
  case DNS:   
      for (LoopCounter = 0; LoopCounter < NETWORK_IP_LENGTH ;LoopCounter++)
      {
          
          OutgoingBuffer[Index++] = InstrumentInfo.networkInfo.DNS[LoopCounter];
      }
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = NETWORK_IP_LENGTH + ONE_BYTE_LENGTH;
      break;
      
  case GATEWAY:
      for (LoopCounter = 0; LoopCounter < NETWORK_IP_LENGTH ;LoopCounter++)
      {
          
          OutgoingBuffer[Index++] = InstrumentInfo.networkInfo.Gateway[LoopCounter];
      }
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = NETWORK_IP_LENGTH + ONE_BYTE_LENGTH;
      break;
  case MAC_ADDRESS:
      for (LoopCounter = 0; LoopCounter < MAC_ADRESS_LENGTH ;LoopCounter++)
      {
          
          OutgoingBuffer[Index++] = InstrumentInfo.wirelessInfo.WirelessMAC[LoopCounter];
      }
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = MAC_ADRESS_LENGTH + ONE_BYTE_LENGTH;
      break;
  case INSTRUMENT_MSG_INTERVAL:
      memcpy(&OutgoingBuffer[Index],&(InstrumentInfo.batteryInfo.InstrumentSPICommunicationInterval),2);
      Index += 2;
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = 2 + ONE_BYTE_LENGTH;
      break;
  case FW_VERSION:  
      OutgoingBuffer[Index++] = FIRMWARE_VERSION_MAJOR;
      OutgoingBuffer[Index++] = FIRMWARE_VERSION_MINOR;
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = 2 + ONE_BYTE_LENGTH;
  
      break;
  case FW_BUILD:
      OutgoingBuffer[Index++] = FIRMWARE_VERSION_BUILD;
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = 1 + ONE_BYTE_LENGTH;
      break;
           
  case V_BAT_MON:
    //Get the latest read Battery monitoring voltages
    memcpy(&OutgoingBuffer[Index],&(InstrumentInfo.vBatMon),sizeof(InstrumentInfo.vBatMon));
    //Increment the index to entity size
    Index += 4;
    //Populate length Byte left earlier
    OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = SIZE_OF_INT_IN_BYTES + ONE_BYTE_LENGTH;
    break;
    
  case V_HOST_MON:
    //Get the latest read Host monitoring voltages
    memcpy(&OutgoingBuffer[Index],&(InstrumentInfo.vHostMon),sizeof(InstrumentInfo.vHostMon));
    //Increment the index to entity size
    Index += 4;
    //Populate length Byte left earlier
    OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = SIZE_OF_INT_IN_BYTES + ONE_BYTE_LENGTH;
    break;
      
          
#ifdef MANFACTURING_TEST_CODE    
    
  case RTC_TEST_RESULT:
    //Get the latest results for the RTC test
    OutgoingBuffer[Index++] = InstrumentInfo.RTCTest;
    //Populate length Byte left earlier
    OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = ONE_BYTE_LENGTH + ONE_BYTE_LENGTH;
    break;
    
#endif /*MANFACTURING_TEST_CODE*/
    
  case GPS_SNR:
    //Get the latest read GPS SNR
    memcpy(&OutgoingBuffer[Index],&(InstrumentInfo.gpsSNR),sizeof(InstrumentInfo.gpsSNR));
    //Increment the index to entity size
    Index += 4;
    //Populate length Byte left earlier
    OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = SIZE_OF_INT_IN_BYTES + ONE_BYTE_LENGTH;
    break;
    
  case CLOUD_USER_NAME_PARAM:
      for (LoopCounter = 0; LoopCounter < CLOUD_USER_ID_LENGTH ;LoopCounter++)
      {
          // Read 20 Byte Network ID
          OutgoingBuffer[Index++] = InstrumentInfo.cloudInfo.CloudUserID[LoopCounter];
      }
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = CLOUD_USER_ID_LENGTH + ONE_BYTE_LENGTH;
      break;
  case CLOUD_USER_PASS_PARAM:
      for (LoopCounter = 0; LoopCounter < CLOUD_USER_PASS_LENGTH ;LoopCounter++)
      {
          // Read 20 Byte Network ID
          OutgoingBuffer[Index++] = InstrumentInfo.cloudInfo.CloudUserPassword[LoopCounter];
      }
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = CLOUD_USER_PASS_LENGTH + ONE_BYTE_LENGTH;
      break;
  case CLOUD_URL_PARAM:
      for (LoopCounter = 0; LoopCounter < CLOUD_URL_LENGTH ;LoopCounter++)
      {
          // Read 20 Byte Network ID
          OutgoingBuffer[Index++] = InstrumentInfo.cloudInfo.CloudURL[LoopCounter];
      }
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = CLOUD_URL_LENGTH + ONE_BYTE_LENGTH;
      break;
      
  default:
      OutgoingBuffer[NETWORK_CONFG_LENGTH_BYTE] = ONE_BYTE_LENGTH;
      break;
  }
  
  // Byte (5 + n) – (5 + n + 1) CheckSum of the message
  messageChecksum = CalculateMessageChecksum(OutgoingBuffer, (unsigned short)2, (unsigned short)(Index-1));
  
  OutgoingBuffer[Index++] = LOBYTE_WORD16(messageChecksum);
  OutgoingBuffer[Index++] = HIBYTE_WORD16(messageChecksum);
  
  // Byte  (5 + n + 2) – (5 + n + 3) End framing Character of the message 
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER; 
  
  returnLength = Index-1;
  return returnLength;
}


//==============================================================================
//
//  static void BuildNoRemainingMesage(uint8_t *OutgoingBuffer)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function writes the no new message to the outgoing buffer   
//
//==============================================================================
static uint8_t BuildNoRemainingMesage(uint8_t *OutgoingBuffer)
{
  uint8_t Index = 0u;
  uint8_t returnLength = 0u;
  
  // Byte 0-1 Start framing Character of the message 
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  
  // Byte 2 Message Type: Radio configure message
  OutgoingBuffer[Index++] = RESPOND_NO_MORE_MESSAGES;
  
  // Byte 3-4 End framing Character of the message 
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER; 
  
  returnLength = Index-1;
  return returnLength;
}

//==============================================================================
//
//  static void BuildShutdownAckMesage(uint8_t *OutgoingBuffer)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function writes the no new message to the outgoing buffer   
//
//==============================================================================
static uint8_t BuildShutdownAckMesage(uint8_t *OutgoingBuffer)
{
  uint8_t Index = 0u;
  uint8_t returnLength = 0u;
  
  // Byte 0-1 Start framing Character of the message 
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  
  // Byte 2 Message Type: Radio configure message
  OutgoingBuffer[Index++] = RESPOND_SHUTDOWN_ACK_MESSAGES;
  
  // Byte 3-4 End framing Character of the message 
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER; 
  
  returnLength = Index-1;
  return returnLength;
}

//==============================================================================
//
//  static void BuildFirmwareAckNackMessage(uint8_t *OutgoingBuffer)
//
//   Author:   Tayyab Tahir
//   Date:     2018/08/41
//
//!  This function writes the firmware ACK NACK message to the outgoing buffer   
//
//==============================================================================
static uint8_t BuildFirmwareAckNackMessage(uint8_t *OutgoingBuffer)
{
  uint8_t Index = 0u;
  uint8_t returnLength = 0u;
  uint16_t messageChecksum = 0u;
  //This variable will hold the value true if something goes wrong in Wifi file commiting process
  BOOLEAN isFileCommitingErrorUpdated = FALSE; 
  //This structure will hold the updated value of firmwareQueueStructure
  firmErrorInfo_t * FirmwareQueueStruct = getFirmErrorInfo(&isFileCommitingErrorUpdated);
  
  // Byte 0-1 Start framing Character of the message 
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  
  // Byte 2 Message Type: Firmware ACK NACK message
  OutgoingBuffer[Index++] = RESPOND_ACK_NACK_FIRMWARE_MESSAGE;
  
  if (isFileCommitingErrorUpdated == FALSE)
  {
    // Byte 3 ACK/NACK
    OutgoingBuffer[Index++] = FW.AckNackByte;
    // Byte 4 Error Type
    OutgoingBuffer[Index++] = FW.ErrorType;
  }
  else
  {
    // Byte 3 ACK/NACK
    OutgoingBuffer[Index++] = NACK;
    // Byte 4 Error Type
    OutgoingBuffer[Index++] = FirmwareQueueStruct->error.genError;
  }
  
  // Byte 5 Payload Length
  OutgoingBuffer[Index++] = FW.ErrorType;
    
  // Byte 6 File Type
  OutgoingBuffer[Index++] = FW.FileType;
  
  // Byte 7-10 Sequence Number
  OutgoingBuffer[Index++] = (FW.RequestedPacketSequenceNumber) & 0xFF;
  OutgoingBuffer[Index++] = (FW.RequestedPacketSequenceNumber>>8) & 0xFF;
  OutgoingBuffer[Index++] = (FW.RequestedPacketSequenceNumber>>16) & 0xFF;
  OutgoingBuffer[Index++] = (FW.RequestedPacketSequenceNumber>>24) & 0xFF;
  
  if(FW.AckNackByte == NACK)
  {
    FW_LOG_DebugPrint("\n\r ** NACK Sent & Requested Packet Number: %d \n\r",FW.RequestedPacketSequenceNumber);
    FW_LOG_EventLogDebugFlush();
  }

  // Byte 11-14 Error Code
  OutgoingBuffer[Index++] = (FirmwareQueueStruct->error.errorInfo ) & 0xFF;
  OutgoingBuffer[Index++] = (FirmwareQueueStruct->error.errorInfo >>8) & 0xFF;
  OutgoingBuffer[Index++] = (FirmwareQueueStruct->error.errorInfo >>16) & 0xFF;
  OutgoingBuffer[Index++] = (FirmwareQueueStruct->error.errorInfo >>24) & 0xFF;
  
  // Byte 15-18 Failure Packet Number
  OutgoingBuffer[Index++] = (FirmwareQueueStruct->seqNumber) & 0xFF;
  OutgoingBuffer[Index++] = (FirmwareQueueStruct->seqNumber >>8) & 0xFF;
  OutgoingBuffer[Index++] = (FirmwareQueueStruct->seqNumber >>16) & 0xFF;
  OutgoingBuffer[Index++] = (FirmwareQueueStruct->seqNumber >>24) & 0xFF;
  
  // Byte 10-11 CheckSum of the message
  messageChecksum = CalculateMessageChecksum(OutgoingBuffer, (unsigned short)2, (unsigned short)(Index-1));
  
  OutgoingBuffer[Index++] = LOBYTE_WORD16(messageChecksum);
  OutgoingBuffer[Index++] = HIBYTE_WORD16(messageChecksum);
  
  // Byte 12-13 End framing Character of the message 
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER; 
  
  returnLength = Index-1;
  return returnLength;
}
//==============================================================================
//
//  static void BuildCustomINETMessage(uint8_t *OutgoingBuffer)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function writes the custom messagesent from INET to the outgoing buffer   
//
//==============================================================================
static uint8_t BuildCustomINETMessage(uint8_t *OutgoingBuffer)
{
  uint8_t Index = 0u;
  uint8_t LoopCouter = 0u;
  uint8_t returnLength = 0u;
  uint16_t messageChecksum = 0u;
  
  // Byte 0-1 Start framing Character of the message 
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = START_FRAMING_CHARACTER;
  
  // Byte 2 Message Type: Radio configure message
  OutgoingBuffer[Index++] = RESPOND_TEXT_INET_MESSAGE;
  
  // Byte 3 Version of message/Config
  OutgoingBuffer[Index++] = InstrumentInfo.MessgaeVersion;
  
  // Byte 4 Device (Smart battery)type (Wifi/Cell)
  OutgoingBuffer[Index++] = InstrumentInfo.wirelessInfo.devType;
  
  // Byte 5 Payload length
  OutgoingBuffer[Index++] = CUSTOM_MSG_LENGTH + ONE_BYTE_LENGTH + ONE_BYTE_LENGTH; //1 Byte for Payload Length , 1Byte for the instrument alarm 
  
  // Byte 6-50 Custom message from Inet 
  for(LoopCouter=0; LoopCouter < CUSTOM_MSG_LENGTH;LoopCouter++)
  {
    OutgoingBuffer[Index++] = InstrumentInfo.INETCustomMessage[LoopCouter];
  }
  
  //Byte 51 Alarm for the Master to go in 
  OutgoingBuffer[Index++] = 0x00; //.Alarm type from inet to set on vpro
  
  // Byte 52-53 CheckSum of the message
  messageChecksum = CalculateMessageChecksum(OutgoingBuffer, (unsigned short)2, (unsigned short)(Index-1));
  
  OutgoingBuffer[Index++] = LOBYTE_WORD16(messageChecksum);
  OutgoingBuffer[Index++] = HIBYTE_WORD16(messageChecksum);
  
  // Byte 54-55 End framing Character of the message 
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER;
  OutgoingBuffer[Index++] = END_FRAMING_CHARACTER;   
  
  returnLength = Index-1;
  return returnLength;
}


//==============================================================================
//
//static uint16_t CalculateMessageChecksum(
//                                              uint8_t *buffer,     
//                                              unsigned short startIndex,
//                                              unsigned short endIndex)                                           
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function calculate the checksums for outgoing as well as incoming messages
//
//==============================================================================
static uint16_t CalculateMessageChecksum(
                                        uint8_t *buffer,     //!< Buffer for which checksum is being calculated
                                        uint16_t startIndex, //!< Start index for checksum calculation
                                        uint16_t endIndex    //!< End index for checksum calculation 
                                          )
{
  // Variable for calculating the checksum, Making it short for the starup so
  uint16_t calculatedChecksum = 0u;
  int32_t index = 0;
  
  // Calculate the checksum (by summing up all bytes)
  for (index = (int)startIndex; index <= (int)endIndex; index++)
  {
    // Add the values in calculated checksum
    calculatedChecksum = (calculatedChecksum + buffer[index]);
  }
  return (calculatedChecksum);   
}
//==============================================================================
//
//static uint8_t CalculateMessageChecksum(void)
//
//   Author:   Dilawar Ali
//   Date:     2018/04/29
//
//!  This function create communication event when data received from instrument has changed
//
//==============================================================================
static void CreateInstrumentEvent(void)
{
    uint8_t loopCounter = 0;
    PTR_EVENT_t commEvt;
    
    BOOLEAN isWakeUpEventReq = false;
    BOOLEAN isDeepSleepEventReq = false;
    
    if( 
       ( true == CHECKBIT ( ( RemoteUnit.InstrumentState ) , SHUTDOWN ) ) && 
       (false == CHECKBIT ( ( RemoteUnit.InternalInstrumentState ), SHUTDOWN ) )
      )
    {
      isDeepSleepEventReq = true;
      
    }
    
    if(
       ( false == CHECKBIT ( ( RemoteUnit.InstrumentState ) , SHUTDOWN ) ) && 
       ( true == CHECKBIT ( ( RemoteUnit.InternalInstrumentState ), SHUTDOWN ) )
      )
    {
      isWakeUpEventReq = true;
    }
    
    if(isWakeUpEventReq)
    {
      //Remove ALL Events From COMMUNICATION Queue
      RELEASE_ALL_EVENTS_FROM_COMM_QUEUE();  
      PostWakeUpEventToSysTask();
    }
    
    //Monitor the Shutdown status of the instrument 
    if(CHECKBIT ((RemoteUnit.InstrumentState),SHUTDOWN))
    {
      //Commit Parameter Here
      CommitParametersToFlash();
    }
    
    // Do Not Create a shutdown event if Instrument is already in shutdown state
    if( 
       ( true == CHECKBIT ( ( RemoteUnit.InstrumentState ) , SHUTDOWN ) ) && 
       ( true == CHECKBIT ( ( RemoteUnit.InternalInstrumentState ), SHUTDOWN ) )
      )
    {
      //Duplicate shutdown handle here
    }
    else
    {
      //Create Instrument update Event.
      RemoteUnit.InternalInstrumentState = RemoteUnit.InstrumentState;
      
      //As Queue is of size one event we will empty it to ensure that space is avalible for next event
      // Release the event from queue
      commEvt = GET_EVENT_FROM_COMM_QUEUE();
      if(NULL != commEvt)
      {
        FreeEvent(commEvt);
        commEvt = NULL;
      }
      
      //Allocate Queue Element
      commEvt = AllocateEvent(sizeof(ComEvent_t));
      if( NULL != commEvt )
      {
        //Get latest time stamp to attach with the event to notify creation time.
        GetCurrentTimeAndDate(&(((PTR_COMM_EVT_t)commEvt)->dateTimeInfo));
        
        //Set the type of communication event.
        commEvt->id = EVENT_COMM_EVT;    
        
        //Check the state for the instrument state for event type.
        if(CHECKBIT ((RemoteUnit.InstrumentState),SHUTDOWN))
        {
          ((PTR_COMM_EVT_t)commEvt)->commEvtType = INSTRUMENT_SHUTDOWN_EVENT;
        }
        else
        {
          ((PTR_COMM_EVT_t)commEvt)->commEvtType = INSTRUMENT_DATA_UPLOAD;
        }
        
        //Populate the instrument state in the event.
        //((PTR_COMM_EVT_t)commEvt)->commEvtType = INSTRUMENT_DATA_UPLOAD;
        ((PTR_COMM_EVT_t)commEvt)->InstrumentState = (INSTRUMENT_STATUS_t)RemoteUnit.InstrumentState;
        ((PTR_COMM_EVT_t)commEvt)->instSensorInfo.numberOfSensors = RemoteUnit.SensorsInfo.numberOfSensors;
        
        //Populate the sensor information in the event.
        for(loopCounter = 0; loopCounter < RemoteUnit.SensorsInfo.numberOfSensors; loopCounter++)
        {
          ((PTR_COMM_EVT_t)commEvt)->instSensorInfo.sensorArray[loopCounter].componentCode        = RemoteUnit.SensorsInfo.sensorArray[loopCounter].componentCode;
          ((PTR_COMM_EVT_t)commEvt)->instSensorInfo.sensorArray[loopCounter].DecimalPlaces        = RemoteUnit.SensorsInfo.sensorArray[loopCounter].DecimalPlaces;
          ((PTR_COMM_EVT_t)commEvt)->instSensorInfo.sensorArray[loopCounter].SensorMeasuringUnits = RemoteUnit.SensorsInfo.sensorArray[loopCounter].SensorMeasuringUnits;
          ((PTR_COMM_EVT_t)commEvt)->instSensorInfo.sensorArray[loopCounter].SensorReadingHigh    = RemoteUnit.SensorsInfo.sensorArray[loopCounter].SensorReadingHigh;
          ((PTR_COMM_EVT_t)commEvt)->instSensorInfo.sensorArray[loopCounter].SensorReadingLow     = RemoteUnit.SensorsInfo.sensorArray[loopCounter].SensorReadingLow;
          ((PTR_COMM_EVT_t)commEvt)->instSensorInfo.sensorArray[loopCounter].SensorStatus         = RemoteUnit.SensorsInfo.sensorArray[loopCounter].SensorStatus;
          ((PTR_COMM_EVT_t)commEvt)->instSensorInfo.sensorArray[loopCounter].SensorType           = RemoteUnit.SensorsInfo.sensorArray[loopCounter].SensorType;
        }
        
        //Populate the next sequence number in the event.
        ((PTR_COMM_EVT_t)commEvt)->sequenceNumber = GetNextSequence();
        
        //Populate the user name and site name in the event.
        strncpy((char *)(((PTR_COMM_EVT_t)commEvt)->WorkerInfo.UserName), (char const*)RemoteUnit.CurrentWorkerInfo.UserName, USER_SITE_NAME_LENGTH);
        strncpy((char *)(((PTR_COMM_EVT_t)commEvt)->WorkerInfo.SiteName), (char const*)RemoteUnit.CurrentWorkerInfo.SiteName, USER_SITE_NAME_LENGTH);
        
        //Populate the converted GPS data in the event.
        GPSGetConvertedLocationInfo(&(((PTR_COMM_EVT_t)commEvt)->GPSLocationInfo));
        
        //Put the ecent to communication queue.
        PUT_EVENT_TO_COMM_QUEUE((PTR_EVENT_t)commEvt);
        
        //Send Communication event to systask
        SendEvntToCommTask();
      }
      else
      {
        EventLogDebugPrint("Queue Is Full Event Will Be Discardded\r\n");
        EventLogDebugFlush(); 
      }
      
      if(isDeepSleepEventReq)
      {
        PostDeepSleepEventToSysTask();
      }
    }    
}

//==============================================================================
//
//  BOOLEAN IsRemoteUnitInAlarm(void);
//
//   Author:   Tayyab Tahir
//   Date:     2018/07/30
//
//!  This function returns instrument and sensor status
//
//==============================================================================
BOOLEAN IsRemoteUnitInAlarm(void)
{
  BOOLEAN returnStatus = FALSE;
  uint8_t loopCounter = 0u;
  
  //Verify the Alaarm statuses of the instrument. 
  if(
     CHECKBIT ((RemoteUnit.InstrumentState),PANIC)          ||\
     CHECKBIT ((RemoteUnit.InstrumentState),MANDOWN)        ||\
     CHECKBIT ((RemoteUnit.InstrumentState),PUMP_FAULT)     ||\
     CHECKBIT ((RemoteUnit.InstrumentState),LOW_BATTERY)
     )
  {
    //Instrument is in faulty or Alarm State. 
    returnStatus  = TRUE;
  }
  else
  {
    //Iterate Sensor Array for Sensor Statuses
    for (loopCounter=0u; loopCounter< MAX_SENSOR_SUPPORTED ;loopCounter++)
    {
      if(RemoteUnit.SensorsInfo.sensorArray[loopCounter].SensorStatus != SENSOR_NORMAL)
      {
        //Found a Sensor in Alarm or Fault.
        returnStatus  = TRUE;
        break;
      }
    }
  }
  return returnStatus;
}

void ResetNetworkFlags(void)
{
    configFlags.networkConfigFlags.networkFlagsVector = 0;
}

BOOLEAN GetParameterUpdateStatus()
{
    return configFlags.isParameterChanged;
}

void SetParameterUpdateStatus(BOOLEAN status)
{
    configFlags.isParameterChanged = status;
}

void SetNetworkConfigInProgress(BOOLEAN isInProgress)
{
    configFlags.isNetworkConfigInProgress = isInProgress;   
}

BOOLEAN GetNetworkConfigInProgress(void)
{
    return configFlags.isNetworkConfigInProgress;
}

BOOLEAN GetUpdatedCloudParam(CloudInfo_t *ptrToCloudInfo)
{
    BOOLEAN retVal = false;
    if(true == configFlags.isCloudParameterChanged)
    {
        memcpy(ptrToCloudInfo->CloudURL,InstrumentInfo.cloudInfo.CloudURL,CLOUD_URL_LENGTH_PLUS_END_OF_FRAME);
        memcpy(ptrToCloudInfo->CloudUserID,InstrumentInfo.cloudInfo.CloudUserID,CLOUD_USER_ID_LENGTH_PLUS_END_OF_FRAME);
        memcpy(ptrToCloudInfo->CloudUserPassword,InstrumentInfo.cloudInfo.CloudUserPassword,CLOUD_USER_PASS_LENGTH_PLUS_END_OF_FRAME);
        configFlags.isCloudParameterChanged = false;
        retVal = true;
    }
    return retVal;
}