//==============================================================================
//
//  SPI_Communication.h
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

#ifndef __SPI_COMM_H
#define __SPI_COMM_H

//#include "Event.h"
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "SysDef.h"



#define EventLogDebugPrint(format,...)                          SYSTEM_PRINT(ENABLE_EVENTS_DEBUG_LOG, format, ##__VA_ARGS__)
#define EventLogDebugFlush()                                    SYSTEM_FLUSH(ENABLE_EVENTS_DEBUG_LOG)

//==============================================================================
//  GLOBAL CONSTANTS, TYPEDEFS AND MACROS
//==============================================================================
#define VPRO_SPI_TRANSFER_TIME_OUT                              100u     //system ticks count 
#define VPRO_SPI_BIT_RATE                                       100000//125000u //set 1 mega hz
#define VPRO_SPI_DATA_TRANSFER_SIZE                             8u 
#define SPI_SLAVE_BUFFER_LENGTH                                 95u
#define SUCCESSFULL                                             1u
#define UNSUCCESSFULL                                           0u
#define PIN_STATUS_HIGH                                         1u
#define PIN_STATUS_LOW                                          0u

//Requests by the Master device 
#define COMMAND_SEND_DATA_TO_WIFI                               0x18
#define COMMAND_SEND_CANNED_MSG_TO_CLOUD                        0x1A
#define COMMAND_SEND_PROXIMITY_ALARM_TO_CLOUD                   0x1B
#define COMMAND_SEND_RADIO_CONFIGURE_MESSAGE                    0x41
#define COMMAND_GET_RADIO_CONFIGURE_MESSAGE                     0x51
#define COMMAND_SEND_WASSUP_MESSAGE                             0xD0
#define COMMAND_SEND_WASSUP_AGAIN_MESSAGE                       0xDA

#ifdef MANFACTURING_TEST_CODE
#define COMMAND_MANUFACTURING_TEST_MESSAGE                      0xBA
#endif /*MANFACTURING_TEST_CODE*/

//Commands by master for firmware update
#define COMMAND_START_RECEIVE_FIRMWARE_MESSAGE                  0x5a
#define COMMAND_RECEIVE_FIRMWARE_MESSAGE                        0x5E
#define COMMAND_COMMIT_FIRMWARE_MESSAGE                         0x5C

//Response from Slave to master regarding firmwrae upgrade 
#define RESPOND_ACK_NACK_FIRMWARE_MESSAGE                       0x5F

//Response from Slave to master
#define RESPOND_NO_MORE_MESSAGES                                0xDD
#define RESPOND_TEXT_INET_MESSAGE                               0x9A
#define RESPOND_GET_RADIO_CONFIGURE_MESSAGE                     0x51
#define RESPOND_SHUTDOWN_ACK_MESSAGES                           0x0A

//Messgae Types 
#define SEND_CANNED_MESSAGE_TO_CLOUD                            0x1A
#define SEND_PROXIMITY_ALARM_TO_CLOUD                           0x1B
#define SEND_GET_RADIO_CONFIGURE_MESSAGE                        0x51
#define SEND_READ_STATUS_MESSAGE                                0x90
#define SEND_READ_QUICK_STATUS_MESSAGE                          0x91
#define SEND_MESSAGE_REQUEST                                    0xAD

//Additional required defines 
#define ONE_BYTE_LENGTH                                         0x01u
#define MAX_SENSOR_SUPPORTED                                    0x07u
#define GPS_LAT_LONG_LENGTH                                     0x04u
#define NETWORK_CONFG_LENGTH_BYTE                               0x05u
#define LENGTH_BYTE                                             5u

#define STATUS_MESSAGE_LENGTH_BYTE                              3u
#define START_FRAMING_CHARACTER                                 0x24u 
#define END_FRAMING_CHARACTER                                   0x23u
#define MSG_VERSION_ONE                                         0x1u

#define I_AM_OK_MESSAGE_SIZE                                    51U          //!< Size of I am OK Message

#define ONE_MINUTE                                              60u //Seconds
#define THIRTY_SECONDS                                          30u
#define SHUTDOWN_TIME_INTERVAL                                  60

#define END_OF_FRAME_BYTE                                       1u


#define USER_SITE_NAME_LENGTH                                   16u
#define USER_SITE_NAME_LENGTH_PLUS_END_OF_FRAME                 (USER_SITE_NAME_LENGTH + END_OF_FRAME_BYTE)

#define SERIAL_NUMBER_LENGTH                                    16u
#define DATE_LENGTH                                             04u
#define PART_NUMBER_LENGTH                                      16u

#define SERIAL_NUMBER_LENGTH_PLUS_END_OF_FRAME                  (SERIAL_NUMBER_LENGTH + END_OF_FRAME_BYTE)
#define DATE_LENGTH_PLUS_END_OF_FRAME                           (DATE_LENGTH + END_OF_FRAME_BYTE)                                    
#define PART_NUMBER_LENGTH_PLUS_END_OF_FRAME                    (PART_NUMBER_LENGTH + END_OF_FRAME_BYTE)

#define TECH_INITIALS_LENGTH                                    04u
#define JOB_NUMBER_LENGTH                                       8u

#define TECH_INITIALS_LENGTH_PLUS_END_OF_FRAME                  (TECH_INITIALS_LENGTH + END_OF_FRAME_BYTE)
#define JOB_NUMBER_LENGTH_PLUS_END_OF_FRAME                     (JOB_NUMBER_LENGTH + END_OF_FRAME_BYTE)

#define CLOUD_USER_ID_LENGTH                                    32u
#define CLOUD_USER_PASS_LENGTH                                  32u
#define CLOUD_URL_LENGTH                                        64u

#define CLOUD_USER_ID_LENGTH_PLUS_END_OF_FRAME                  (CLOUD_USER_ID_LENGTH + END_OF_FRAME_BYTE) 
#define CLOUD_USER_PASS_LENGTH_PLUS_END_OF_FRAME                (CLOUD_USER_PASS_LENGTH + END_OF_FRAME_BYTE)
#define CLOUD_URL_LENGTH_PLUS_END_OF_FRAME                      (CLOUD_URL_LENGTH + END_OF_FRAME_BYTE)

#define NETWORK_ID_LENGTH                                       32u
#define NETWORK_PASSWORD_LENGTH                                 32u
#define NETWORK_IP_LENGTH                                       16u

#define NETWORK_ID_LENGTH_PLUS_END_OF_FRAME                     (NETWORK_ID_LENGTH + END_OF_FRAME_BYTE)
#define NETWORK_PASSWORD_LENGTH_PLUS_END_OF_FRAME               (NETWORK_PASSWORD_LENGTH + END_OF_FRAME_BYTE)
#define NETWORK_IP_LENGTH_PLUS_END_OF_FRAME                     (NETWORK_IP_LENGTH + END_OF_FRAME_BYTE)

#define MAC_ADRESS_LENGTH                                       17u
#define MAC_ADRESS_LENGTH_PLUS_END_OF_FRAME                     (MAC_ADRESS_LENGTH + END_OF_FRAME_BYTE)

#define CUSTOM_MSG_LENGTH                                       45u
#define CUSTOM_MSG_LENGTH_PLUS_END_OF_FRAME                     (CUSTOM_MSG_LENGTH + END_OF_FRAME_BYTE)

#define SIZE_OF_INT_IN_BYTES                                    4u

#define FW_LOG_DebugPrint(format,...)                           SYSTEM_PRINT(ENABLE_FIRMWARE_DOWNLOAD_LOG, format, ##__VA_ARGS__)
#define FW_LOG_EventLogDebugFlush()                             SYSTEM_FLUSH(ENABLE_FIRMWARE_DOWNLOAD_LOG)

#define CHECKBIT(var,bit)                                       (((var) >> (bit)) & (int8_t) 0x01)   //!< Macro to read a particular bit of a number
#define SETBIT(var,bit)                                         ((var) |= ((int8_t) 0x01 << (bit)))  //!< Macro to set a particular bit of a number

#define HIBYTE_WORD16(param)                                    (uint8_t)((param) >> 8)              //!< Macro to get higher byte (8 MSB bits) from a 16 bit word
#define LOBYTE_WORD16(param)                                    (uint8_t)(param)                     //!< Macro to get lower byte (8 LSB bits) from a 16 bit word

#define HINIBBLE_WORD8(param)                                   ( ((int8_t)((param) >> 4)) & 0x0F )  //!< Macro to get higher nibble (4 MSB bits) from a 8 bit word
#define LONIBBLE_WORD8(param)                                   ( ((int8_t)(param)) & 0x0F)          //!< Macro to get lower nibble (4 LSB bits) from a 8 bit word

#define SHIFT_BYTE_TO_LEFT_WORD16(param)                        ((uint16_t)(((param) << 8) & 0xFF00))//!< Macro to shift higher Byte of short word to lower place
#define SHIFT_BYTE_TO_RIGHT_WORD16(param)                       ((uint16_t)(((param) >> 8) & 0x00FF))//!< Macro to shift higher Byte of short word to lower place

//==============================================================================
//  GLOBAL DATA STRUCTURES DEFINITION
//==============================================================================

typedef enum
{
    WIRELESS_DEVICE_DEFAULT =0,
    WIRELESS_DEVICE_WIFI,
    WIRELESS_DEVICE_CELLULAR
    
}WIRELESS_DEVICE_TYPE_t;

typedef enum
{
    INSTRUMENT_NORMAL   = 0,
    PANIC               = 1,
    SHUTDOWN            = 2,
    MANDOWN             = 3,
    PUMP_FAULT          = 4,
    LOW_BATTERY         = 5,
    INSTRUMENT_TYPE     = 6,//(Instrument type; Local = 0, Remote = 1)
    INSTRUMENT_CHARGING = 7  
}
INSTRUMENT_STATUS_t;


typedef enum
{
   ENCRYPTION_TYPE_OPEN = 0,
   ENCRYPTION_TYPE_WEP,
   ENCRYPTION_TYPE_WPA_WPA2,
} SMART_BATTERY_ENCRYPTION_TYPE_t;

typedef enum 
{
    MEAS_UNITS_INVALID          = 0,//!< This constant is used for invalid gas
    MEAS_UNITS_PPM              = 1,//!< This constant is used for gas in ppm units 
    MEAS_UNITS_VOL              = 2,//!< This constant is used for gas in percentage volume units 
    MEAS_UNITS_LEL              = 3,//!< This constant is used for gas in perecntage lel units 
    
}MEASUREMENT_UNITS_t;

#ifdef MANFACTURING_TEST_CODE

typedef enum
{
  TEST_IN_PROGRESS = 0,
  TEST_PASSED = 1,  
  TEST_FAILED = 255,
}TEST_RESULT_t;

#endif /*MANFACTURING_TEST_CODE*/ 

typedef struct BatteryInfo
{
    uint8_t SerialNumber[SERIAL_NUMBER_LENGTH_PLUS_END_OF_FRAME];
    uint8_t PartNumber[PART_NUMBER_LENGTH_PLUS_END_OF_FRAME];
    uint8_t ManufacturingDate[DATE_LENGTH_PLUS_END_OF_FRAME];
    uint16_t InstrumentSPICommunicationInterval;
    uint8_t HardwareVersion;
}BatteryInfo_t;

typedef struct JobInfo
{
    uint8_t TechniciansInitials[TECH_INITIALS_LENGTH_PLUS_END_OF_FRAME];
    uint8_t JobNumber[JOB_NUMBER_LENGTH_PLUS_END_OF_FRAME];
}JobInfo_t;

typedef struct SereverInfo
{
    uint8_t CloudUserID[CLOUD_USER_ID_LENGTH_PLUS_END_OF_FRAME];
    uint8_t CloudUserPassword[CLOUD_USER_PASS_LENGTH_PLUS_END_OF_FRAME];
    uint8_t CloudURL[CLOUD_URL_LENGTH_PLUS_END_OF_FRAME];
    
}CloudInfo_t;

typedef struct
{
    WIRELESS_DEVICE_TYPE_t devType;
    uint8_t WirelessMAC[MAC_ADRESS_LENGTH_PLUS_END_OF_FRAME];
    int8_t WirelessRSSI;
}WirelessInfo_t;

typedef struct NetworkInfo
{
    uint8_t NetworkID[NETWORK_ID_LENGTH_PLUS_END_OF_FRAME];
    uint8_t NetworkPassword[NETWORK_PASSWORD_LENGTH_PLUS_END_OF_FRAME];
    SMART_BATTERY_ENCRYPTION_TYPE_t NetworkEncryptionType;
    
    uint8_t NetworkType;
    uint8_t NetworkMask[NETWORK_IP_LENGTH_PLUS_END_OF_FRAME];
    uint8_t IPAddress[NETWORK_IP_LENGTH_PLUS_END_OF_FRAME];
    uint8_t DNS[NETWORK_IP_LENGTH_PLUS_END_OF_FRAME];
    uint8_t Gateway[NETWORK_IP_LENGTH_PLUS_END_OF_FRAME];
}NetworkInfo_t;

typedef enum
{
    RUNNING_NORMAL                      = 0u,
    NO_CONNECTION_AVALIBLE              = 1u,
    CREDENTIALS_REJECTED_BY_NETWORK     = 2u,
    NO_CELL_SERVICE                     = 3u,
    ERROR_UNDEFINED                     = 4u
}SMART_BATTERY_STATUS_t;

typedef enum 
{
    INET_ACCEPTS_INSTRUMENT_AND_MONITORING      = 0u,
    INET_NOT_MONITORING_INSTRUMENT              = 1u,
    NO_CELL_OR_WIFI                             = 2u,
    CAN_NOT_CONNECT_TO_INET                     = 3u,
    INVALID_INET_CREDANTIALS                    = 4u
}INET_STATUS_t;

typedef struct
{
  BatteryInfo_t batteryInfo;
  JobInfo_t jobInfo;
  NetworkInfo_t networkInfo;
  WirelessInfo_t wirelessInfo;
  CloudInfo_t cloudInfo;
  uint8_t MessgaeVersion;
  SMART_BATTERY_STATUS_t SmartBatteryStatus;
  INET_STATUS_t InetStatus;
  uint8_t CloudMessageStatus;
  uint8_t IsINETMessageAvalibleforVpro;
  uint8_t INETCustomMessage[CUSTOM_MSG_LENGTH+1];
  uint32_t lastSlaveCommunictionTimeStamp;
  uint32_t vBatMon;
  uint32_t vHostMon;
  uint32_t gpsSNR;
#ifdef MANFACTURING_TEST_CODE
  TEST_RESULT_t dataFlashTest;
  TEST_RESULT_t RTCTest;
  BOOLEAN isWDTTestRunnig;
#endif /*MANFACTURING_TEST_CODE*/

  
}InstInfo_t;

extern InstInfo_t InstrumentInfo;

typedef enum
{
    INVALID_UNITS       = 0,
    PARTS_PER_MILLION   = 1,
    PERCENTAGE_VOLUME   = 2,
    PERCANTAGE_LEL      = 3  
}GAS_MEASUREMENT_UNITS_t;

typedef enum 
{
    SENSOR_NORMAL       =0X00,
    LOW_ALARM           =0X01,
    HIGH_ALARM          =0X02,
    NEGATIVE_OR         =0X03,
    OR                  =0X04,
    CALIBRATION_FAULT   =0X05,
    ZERO_FAULT          =0X06,
    USER_DISABLED       =0X08,
    BUMP_FAULT          =0X09,
    CALIBRATION_OVERDUE =0X0B,
    DATA_FAIL           =0X0D,
    TWA_ALARM           =0X0E,
    STEL_ALARM          =0X0F
    
}SENSOR_STATUS_t;

typedef enum
{
    SERIAL_NUMBER               = 0u,
    MANUFACTURING_DATE          = 1u,
    PART_NUMBER                 = 2u,
    TECHNICIAN_INITIALS         = 3u,
    JOB_NUMBER                  = 4u,
    HARDWARE_VERSION            = 5u,
    NETWORK_ID                  = 6u,
    NETWORK_PASSWORD            = 7u,
    NETWORK_ENCRYPTION_TYPE     = 8u,
    BATTERY_TYPE                = 9u,       //Wi-Fi set to 1; Cell set to 2
    NETWORK_TYPE                = 10u,       // 0 = DHCP 1 = STATIC
    Network_Mask                = 11u,
    IP_Address                  = 12u,
    DNS                         = 13u,
    GATEWAY                     = 14u,
    COMMIT_PARAM                = 15u,
    MAC_ADDRESS                 = 16u,
    INSTRUMENT_MSG_INTERVAL     = 17u,
    FW_VERSION                  = 18u,
    FW_BUILD                    = 19u,
    V_BAT_MON                   = 28u,
    V_HOST_MON                  = 29u,
#ifdef MANFACTURING_TEST_CODE    
    DATA_FLASH_TEST_RESULT      = 31u,
    RTC_TEST_RESULT             = 32u,
#endif /*MANFACTURING_TEST_CODE*/
    
    GPS_SNR                     = 30u,
    CLOUD_USER_NAME_PARAM       = 33u,
    CLOUD_USER_PASS_PARAM       = 34u,
    CLOUD_URL_PARAM             = 35u,
        
    NO_PARAMETER                = 40u
        
}RADIO_CONFIGURATION_PARAMETER_t;

typedef enum 
{
    SENSOR_GAS_TYPE_INVALID                           = 0u, //!< Sensor type code - Invalid
    SENSOR_GAS_TYPE_CO                                = 1u, //!< Sensor type code - Carbon Monoxide
    SENSOR_GAS_TYPE_H2S                               = 2u, //!< Sensor type code - Hydrogen Sulphide
    SENSOR_GAS_TYPE_SO2                               = 3u, //!< Sensor type code - Sulphur Dioxide
    SENSOR_GAS_TYPE_NO2                               = 4u, //!< Sensor type code - Nitrogen Dioxide
    SENSOR_GAS_TYPE_CL2                               = 5u, //!< Sensor type code - Chlorine
    SENSOR_GAS_TYPE_CLO2                              = 6u, //!< Sensor type code - Chlorine Dioxide
    SENSOR_GAS_TYPE_HCN                               = 7u, //!< Sensor type code - Hydrogen Cyanide
    SENSOR_GAS_TYPE_PH3                               = 8u, //!< Sensor type code - Phosphine
    SENSOR_GAS_TYPE_H2                                = 9u, //!< Sensor type code - Hydrogen
    SENSOR_GAS_TYPE_CO2                              = 11u, //!< Sensor type code - Carbon Dioxide (IR)
    SENSOR_GAS_TYPE_NO                               = 12u, //!< Sensor type code - Nitric Oxide
    SENSOR_GAS_TYPE_NH3                              = 13u, //!< Sensor type code - Ammonia
    SENSOR_GAS_TYPE_HCL                              = 14u, //!< Sensor type code - Hydrogen Chloride
    SENSOR_GAS_TYPE_O3                               = 15u, //!< Sensor type code - Ozone
    SENSOR_GAS_TYPE_PHOSGENE                         = 16u, //!< Sensor type code - Phosgene
    SENSOR_GAS_TYPE_HF                               = 17u, //!< Sensor type code - HF sensor 
    SENSOR_GAS_TYPE_CH4_IR                           = 18u, //!< Sensor type code - Methane IR
    SENSOR_GAS_TYPE_CO_H2_NULL                       = 19u, //!< Sensor type code - CO H2 Null sensor 
    SENSOR_GAS_TYPE_O2                               = 20u, //!< Sensor type code - Oxygen
    SENSOR_GAS_TYPE_CH4                              = 21u, //!< Sensor type code - Methane (LEL & IR)
    SENSOR_GAS_TYPE_HEXANE                           = 23u, //!< Sensor type code - Hexane (LEL)
    SENSOR_GAS_TYPE_PENTANE                          = 26u, //!< Sensor type code - Pentane (LEL)
    SENSOR_GAS_TYPE_PROPANE                          = 27u, //!< Sensor type code - Propane (LEL & IR)
    SENSOR_GAS_TYPE_14BUTANEDIOL                     = 28u, //!< Sensor type code - Butanediol,1,4-
    SENSOR_GAS_TYPE_14DIOXANE                        = 29u, //!< Sensor type code - Dioxane,1,4-
    SENSOR_GAS_TYPE_124TRIMETH                       = 30u, //!< Sensor type code - Trimethylbenzene,1,2,4-
    SENSOR_GAS_TYPE_123TRIMETH                       = 31u, //!< Sensor type code - Trimethylbenzene,1,2,3-
    SENSOR_GAS_TYPE_12DIBROMO                        = 32u, //!< Sensor type code - Dibromoethane,1,2-
    SENSOR_GAS_TYPE_12DICHLORO                       = 33u, //!< Sensor type code - Dichlorobenzene,1,2-
    SENSOR_GAS_TYPE_135TRIMETH                       = 34u, //!< Sensor type code - Trimethylbenzene,1,3,5-
    SENSOR_GAS_TYPE_1BUTANOL                         = 35u, //!< Sensor type code - Butanol,1-
    SENSOR_GAS_TYPE_1METH2PROP                       = 36u, //!< Sensor type code - Methoxypropanol
    SENSOR_GAS_TYPE_1PROPANOL                        = 37u, //!< Sensor type code - Propanol,1-
    SENSOR_GAS_TYPE_METHACETATE                      = 38u, //!< Sensor type code - Methyl Acetate
    SENSOR_GAS_TYPE_METHACRYL                        = 39u, //!< Sensor type code - Methyl Acrylate
    SENSOR_GAS_TYPE_METHACETO                        = 40u, //!< Sensor type code - Methyl Acetoacetate
    SENSOR_GAS_TYPE_METHBENZO                        = 41u, //!< Sensor type code - Methyl Benzoate
    SENSOR_GAS_TYPE_METHMETHACR                      = 42u, //!< Sensor type code - Methyl Methacrylate
    SENSOR_GAS_TYPE_2BUTANONE                        = 43u, //!< Sensor type code - Butanone
    SENSOR_GAS_TYPE_DIMTHFORM                        = 44u, //!< Sensor type code - Dimethylformamide
    SENSOR_GAS_TYPE_2METHOXYETH                      = 45u, //!< Sensor type code - Methoxy Ethanol,2-
    SENSOR_GAS_TYPE_2PENTANONE                       = 46u, //!< Sensor type code - Pentanone,2-
    SENSOR_GAS_TYPE_2PICOLINE                        = 47u, //!< Sensor type code - Picoline,2-
    SENSOR_GAS_TYPE_2PROPANOL                        = 48u, //!< Sensor type code - Propanol,2-
    SENSOR_GAS_TYPE_NNDIMETHFORM                     = 49u, //!< Sensor type code - Dimethylformamide
    SENSOR_GAS_TYPE_NNDIMETHACET                     = 50u, //!< Sensor type code - Dimethyl Acetamide
    SENSOR_GAS_TYPE_3PICOLINE                        = 51u, //!< Sensor type code - Picoline,3-
    SENSOR_GAS_TYPE_DIACETONE                        = 52u, //!< Sensor type code - Diacetone Alcohol
    SENSOR_GAS_TYPE_ACETALDEHYDE                     = 53u, //!< Sensor type code - Acetaldehyde
    SENSOR_GAS_TYPE_ACETONE                          = 54u, //!< Sensor type code - Acetone
    SENSOR_GAS_TYPE_ACETOPHENONE                     = 55u, //!< Sensor type code - Acetophenone
    SENSOR_GAS_TYPE_ALLYL_ALC                        = 56u, //!< Sensor type code - Allyl Alcohol
    SENSOR_GAS_TYPE_AMYLACETATE                      = 58u, //!< Sensor type code - Amyl Acetate
    SENSOR_GAS_TYPE_BENZENE                          = 59u, //!< Sensor type code - Benzene
    SENSOR_GAS_TYPE_METHBROMIDE                      = 60u, //!< Sensor type code - Methyl Bromide
    SENSOR_GAS_TYPE_BUTADIENE                        = 61u, //!< Sensor type code - Butadiene
    SENSOR_GAS_TYPE_BUTOXYETH                        = 62u, //!< Sensor type code - Butoxy Ethanol
    SENSOR_GAS_TYPE_BUTYLACETATE                     = 63u, //!< Sensor type code - Butyl Acetate
    SENSOR_GAS_TYPE_TETRACHLORO                      = 64u, //!< Sensor type code - Tetrachloroethylene
    SENSOR_GAS_TYPE_11DICHLORO                       = 65u, //!< Sensor type code - Dichloroethene,t-1,2
    SENSOR_GAS_TYPE_ETHYLBENZENE                     = 66u, //!< Sensor type code - Ethyl Benzene
    SENSOR_GAS_TYPE_TRICHLOROETH                     = 67u, //!< Sensor type code - Trichloroethylene
    SENSOR_GAS_TYPE_ETHYLACETO                       = 68u, //!< Sensor type code - Ethyl Acetoacetate
    SENSOR_GAS_TYPE_CHLOROBENZ                       = 69u, //!< Sensor type code - Chlorobenzene
    SENSOR_GAS_TYPE_CUMENE                           = 70u, //!< Sensor type code - Cumene
    SENSOR_GAS_TYPE_CYCLOHEXANE                      = 71u, //!< Sensor type code - Cyclohexane
    SENSOR_GAS_TYPE_CYCHEXANONE                      = 72u, //!< Sensor type code - Cyclohexanone
    SENSOR_GAS_TYPE_DECANE                           = 73u, //!< Sensor type code - Decane
    SENSOR_GAS_TYPE_DIETHYLAMINE                     = 74u, //!< Sensor type code - Diethylamine
    SENSOR_GAS_TYPE_DIMETHOXY                        = 75u, //!< Sensor type code - Dimethoxymethane
    SENSOR_GAS_TYPE_EPICHLORO                        = 76u, //!< Sensor type code - Epichlorohydrin
    SENSOR_GAS_TYPE_ETHANOL                          = 77u, //!< Sensor type code - Etahnol
    SENSOR_GAS_TYPE_ETHGLYCOL                        = 78u, //!< Sensor type code - Ethylene Glycol
    SENSOR_GAS_TYPE_ETHACETATE                       = 79u, //!< Sensor type code - Ethyl Acetate
    SENSOR_GAS_TYPE_ETHYLENE                         = 80u, //!< Sensor type code - Ethylene
    SENSOR_GAS_TYPE_ETO                              = 81u, //!< Sensor type code - Ethylene Oxide
    SENSOR_GAS_TYPE_BUTYROLACT                       = 82u, //!< Sensor type code - Butyrolactone
    SENSOR_GAS_TYPE_HEPTANE                          = 84u, //!< Sensor type code - Heptane
    SENSOR_GAS_TYPE_HYDRAZINE                        = 86u, //!< Sensor type code - Hydrazine
    SENSOR_GAS_TYPE_ISOAMYLACET                      = 87u, //!< Sensor type code - Isoamyl Acetate
    SENSOR_GAS_TYPE_ISOPROPAMINE                     = 88u, //!< Sensor type code - Isopropylamine
    SENSOR_GAS_TYPE_ISOPROPETHER                     = 89u, //!< Sensor type code - Isopropyl Ether
    SENSOR_GAS_TYPE_ISOBUTANOL                       = 90u, //!< Sensor type code - Isobutanol
    SENSOR_GAS_TYPE_ISOBUTYLENE                      = 91u, //!< Sensor type code - Isobutylene
    SENSOR_GAS_TYPE_ISOOCTANE                        = 92u, //!< Sensor type code - Isooctane
    SENSOR_GAS_TYPE_ISOPHORONE                       = 93u, //!< Sensor type code - Isophorone
    SENSOR_GAS_TYPE_ISOPROPANOL                      = 94u, //!< Sensor type code - Isopropanol
    SENSOR_GAS_TYPE_JETAFUEL                         = 95u, //!< Sensor type code - Jet A Fuel
    SENSOR_GAS_TYPE_JETA1FUEL                        = 96u, //!< Sensor type code - Jet A1 Fuel
    SENSOR_GAS_TYPE_MEK                              = 98u, //!< Sensor type code - MEK (Methylethyl Ketone)
    SENSOR_GAS_TYPE_MESITYLOXIDE                     = 99u, //!< Sensor type code - Mesityl Oxide
    SENSOR_GAS_TYPE_MIBK                             = 100u,//!< Sensor type code - MIBK (Methyl-Isobutyl Ketone)
    SENSOR_GAS_TYPE_MONOMETHYL                       = 101u,//!< Sensor type code - Monomethylamine
    SENSOR_GAS_TYPE_MTBE                             = 102u,//!< Sensor type code - MTBE (Methyl-tertbutyl Ether)
    SENSOR_GAS_TYPE_METHBENZYL                       = 103u,//!< Sensor type code - Methylbenzyl Alcohol
    SENSOR_GAS_TYPE_MXYLENE                          = 104u,//!< Sensor type code - Xylene,m-
    SENSOR_GAS_TYPE_NMETHPYRROL                      = 105u,//!< Sensor type code - Methylpyrrolidone,n-
    SENSOR_GAS_TYPE_OCTANE                           = 106u,//!< Sensor type code - Octane
    SENSOR_GAS_TYPE_OXYLENE                          = 107u,//!< Sensor type code - Xylene,o-
    SENSOR_GAS_TYPE_PHENELETHYL                      = 108u,//!< Sensor type code - Phenylethyl Alcohol
    SENSOR_GAS_TYPE_PHENOL                           = 109u,//!< Sensor type code - Phenol
    SENSOR_GAS_TYPE_PROPYLENE                        = 111u,//!< Sensor type code - Propylene
    SENSOR_GAS_TYPE_PROPYLOXIDE                      = 112u,//!< Sensor type code - Propylene Oxide
    SENSOR_GAS_TYPE_PXYLENE                          = 113u,//!< Sensor type code - Xylene,p-
    SENSOR_GAS_TYPE_PYRIDINE                         = 114u,//!< Sensor type code - Pyridine
    SENSOR_GAS_TYPE_QUINOLINE                        = 115u,//!< Sensor type code - Quinoline
    SENSOR_GAS_TYPE_STYRENE                          = 116u,//!< Sensor type code - Styrene
    SENSOR_GAS_TYPE_TBUTYLAMINE                      = 117u,//!< Sensor type code - tert-Butylamine
    SENSOR_GAS_TYPE_TRISDICHLETH                     = 118u,//!< Sensor type code - Dichloroethene,t-1,2
    SENSOR_GAS_TYPE_TBUTYLMERCAP                     = 119u,//!< Sensor type code - tert-Butyl Mercaptan
    SENSOR_GAS_TYPE_TBUTYLALCO                       = 120u,//!< Sensor type code - tert-Butyl Alcohol
    SENSOR_GAS_TYPE_THF                              = 121u,//!< Sensor type code - THF (Tetrahydrofuran)
    SENSOR_GAS_TYPE_THIOPHENE                        = 122u,//!< Sensor type code - Thiophene
    SENSOR_GAS_TYPE_TOLUENE                          = 123u,//!< Sensor type code - Toluene
    SENSOR_GAS_TYPE_TURPENTINE                       = 124u,//!< Sensor type code - Turpentine
    SENSOR_GAS_TYPE_VINYLCYCLO                       = 125u,//!< Sensor type code - Vinylcyclohexone
    SENSOR_GAS_TYPE_VINYLACETATE                     = 126u,//!< Sensor type code - Vinyl Acetate
    SENSOR_GAS_TYPE_VINYLCHLOR                       = 127u,//!< Sensor type code - Vinyl Chloride
    SENSOR_GAS_TYPE_ACETYLENE                        = 200u,//!< Sensor type code - Acetylene
    SENSOR_GAS_TYPE_BUTANE                           = 201u,//!< Sensor type code - Butane
    SENSOR_GAS_TYPE_ETHANE                           = 202u,//!< Sensor type code - Ethane
    SENSOR_GAS_TYPE_METHANOL                         = 203u,//!< Sensor type code - Methanol
    SENSOR_GAS_TYPE_JP4                              = 205u,//!< Sensor type code - JP-4
    SENSOR_GAS_TYPE_JP5                              = 206u,//!< Sensor type code - JP-5
    SENSOR_GAS_TYPE_JP8                              = 207u,//!< Sensor type code - JP-8
    SENSOR_GAS_TYPE_ACETIC_ACID                      = 208u,//!< Sensor type code - Acetic Acid
    SENSOR_GAS_TYPE_ACETIC_ANHYD                     = 209u,//!< Sensor type code - Acetic Anhydride
    SENSOR_GAS_TYPE_ARSINE                           = 210u,//!< Sensor type code - Arsine
    SENSOR_GAS_TYPE_BROMINE                          = 211u,//!< Sensor type code - Bromine
    SENSOR_GAS_TYPE_CARBON_DISUL                     = 212u,//!< Sensor type code - Carbon Disulfide
    SENSOR_GAS_TYPE_CYCLOHEXENE                      = 213u,//!< Sensor type code - Cyclohexene
    SENSOR_GAS_TYPE_DIESEL_FUEL                      = 214u,//!< Sensor type code - Diesel Fuel
    SENSOR_GAS_TYPE_DIMETHSULFOX                     = 215u,//!< Sensor type code - Dimethyl Sulfoxide
    SENSOR_GAS_TYPE_ETHYL_ETHER                      = 216u,//!< Sensor type code - Ethyl Ether
    SENSOR_GAS_TYPE_IODINE                           = 217u,//!< Sensor type code - Iodine
    SENSOR_GAS_TYPE_METHYL_MERC                      = 218u,//!< Sensor type code - Methyl Mercaptan
    SENSOR_GAS_TYPE_NAPHTHALENE                      = 219u,//!< Sensor type code - Naphthalene
    SENSOR_GAS_TYPE_NITROBENZENE                     = 220u,//!< Sensor type code - Nitrobenzene
    SENSOR_GAS_TYPE_2MTHOXETHOXE                     = 221u,//!< Sensor type code - Methoxyethoxyethanol,2-
    SENSOR_GAS_TYPE_NONANE                           = 222u,//!< Sensor type code - Nonane
    SENSOR_GAS_TYPE_HC		                     = 248u,//!< Sensor type code - Hydrocarben sensor
    // Custom response factors are not supported in VPRO
    
}SENSOR_TYPES_t;

typedef enum
{
    NO_MESSAGE                    =0u,
    MESSAGE_FROM_INET             =1u,
    EMERGENCY_MESSAGE_FROM_INET   =2u,
    ALARM_MESSAGE_FROM_INET       =3u
}CLOUD_MESSAGE_BYTE_t;

typedef enum
{
    NO_MESSAGE_LEFT             = 0u,
    RESPOND_STATUS_MSG          = 1u,
    RESPOND_QUICK_STATUS_MSG    = 2u,
    RESPOND_RADIO_CONFIGURATION = 3u,
    PASS_INET_MESSAGE           = 4u,
    RENSPOND_FW_ACK_NACK_MESSAGE= 5u,
    RESPONCE_STATUS_ACK_MESSAGE = 6u,
}MASTER_REQUESTS_t;

typedef enum
{
  INSTRUMENT_TYPE_DEFAULT     = 0u,
  INSTRUMENT_TYPE_SAFECORE    = 0x12u,
  INSTRUMENT_TYPE_VPRO        = 0x13u,
}INSTRUMENT_TYPE_t;

//*****************Smart Battery Firmware Upgrade Enums************//
typedef enum
{
UNKNOWN = 0,
MAIN_FIRMWARE,
CA_CERTIFICATE,
HASH_KEY,
SERVICE_PACK,
}FIRMWARE_FILE_TYPEt;

typedef enum
{
  ACK  = 0x06,
  NACK = 0x15,
}ACK_NACK_t;

typedef enum
{
  NO_ERROR= 0,
  BAD_LENGTH,
  INVALID_CRC,
  BAD_FRAME_LENGTH,
  ALLOCATION_ERROR,
}FW_PACKET_ERROR_TYPE_t;
//*****************Smart Battery Firmware Upgrade Enums************//

typedef struct 
{
    SENSOR_TYPES_t  SensorType;
    GAS_MEASUREMENT_UNITS_t   SensorMeasuringUnits;
    char   SensorReadingHigh;
    char   SensorReadingLow;   
    SENSOR_STATUS_t SensorStatus;
    unsigned char DecimalPlaces;
    uint8_t componentCode;
}SensorInfo_t;

typedef struct 
{
    uint8_t numberOfSensors;
    SensorInfo_t sensorArray[MAX_SENSOR_SUPPORTED];
}InstSensorInfo_t;


typedef struct
{
    uint8_t UserName[USER_SITE_NAME_LENGTH_PLUS_END_OF_FRAME];
    uint8_t SiteName[USER_SITE_NAME_LENGTH_PLUS_END_OF_FRAME];
}Worker_Info_t;

typedef struct
{
    unsigned char SerialNumber[SERIAL_NUMBER_LENGTH+1];
    Worker_Info_t CurrentWorkerInfo;
    INSTRUMENT_TYPE_t InstrumentType;
    unsigned char MessageCounter;
    unsigned char InstrumentState;
    unsigned char InternalInstrumentState;
    unsigned char UserSecurityLevel;
    unsigned char SiteSecurityLevel;
    InstSensorInfo_t  SensorsInfo;
    unsigned char UserCustomMessage[CUSTOM_MSG_LENGTH+1];
}Remote_Instrument_t;

typedef enum
{
  NETWORK_TYPE_STATIC = 0,
  NETWORK_TYPE_DHCP
}NETWORK_TYPE_t;

//==============================================================================
//  GLOBAL DATA
//==============================================================================
extern Remote_Instrument_t RemoteUnit;
void VproSlaveStateMachine (uint16_t EventType);
int32_t SPISlaveConfigure(void);

extern Swi_Struct swiSPISlaveStruct;
extern Swi_Handle swiSPISlaveHandle;
extern Semaphore_Struct semSPISlaveStruct;
extern Semaphore_Handle semSPISlaveHandle;

//==============================================================================
//  EXTERNAL OR GLOBAL FUNCTIONS
//==============================================================================
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
uint8_t ManageOutgoingMessage(void);

//==============================================================================
//
//  BOOLEAN ProcessIncomingmessage(unsigned char *IncomingBuffer, unsigned char StartingIndex)
//
//   Author:   Tayyab Tahir
//   Date:     2018/04/23
//
//!  This function handeles data extraction from incoming message according to 
//!  their type.
//
//==============================================================================
BOOLEAN ProcessIncomingmessage(unsigned char *IncomingBuffer, unsigned char StartingIndex);

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
void InitializeIntrumentData(void);

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
uint8_t ConfigureCSPin(void);

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
Void swiSPISlaveFxn(UArg arg0, UArg arg1);
//==============================================================================
//
//  void ChipSelectInterrupt(uint_least8_t index)
//
//   Author:   Tayyab Tahir
//   Date:     2018/07/26
//
//!  This function Chip select GPIO ISR 
//
//==============================================================================
void ChipSelectInterrupt(uint_least8_t index);
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
BOOLEAN IsRemoteUnitInAlarm(void);
void ResetNetworkFlags(void);

BOOLEAN GetNetworkConfigInProgress(void);
void SetParameterUpdateStatus(BOOLEAN status);
BOOLEAN GetParameterUpdateStatus();
BOOLEAN GetUpdatedCloudParam(CloudInfo_t *ptrToCloudInfo);

#endif/*__SPI_COMM_H*/