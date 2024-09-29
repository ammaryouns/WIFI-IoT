#ifndef __FIRMWARE_UPLOAD_H
#define __FIRMWARE_UPLOAD_H

#include <stdint.h>
#include <SysDef.h>

#define MAX_SIG_SIZE_POSSIBLE           256

#define ERROR_CODE_BATTERY_IDENT_ERROR                          (-100)
#define ERROR_CODE_MAIN_HEADER_IDENT_ERROR                      (-101)
#define ERROR_CODE_FILE_HEADER_IDENT_ERROR                      (-102)
#define ERROR_CODE_MAIN_HEADER_CRC_ERROR                        (-103)
#define ERROR_CODE_FILE_HEADER_CRC_ERROR                        (-104)
#define ERROR_CODE_DISC_HEADER_IDENT_ERROR                      (-105)
#define ERROR_CODE_FILE_OPENING_FAILED                          (-106)
#define ERROR_CODE_FILE_WRITTING_FAILED                         (-107)
#define ERROR_CODE_FILE_LENGTH_ERROR                            (-108)
#define ERROR_CODE_DISC_LENGTH_ERROR                            (-109)
#define ERROR_CODE_FILE_CRC_ERROR                               (-110)
#define ERROR_CODE_FILE_CLOSING_FAILED                          (-111)

typedef enum
{
    UNKNOWN_FILE_TYPE =0,
    MAIN_FIRMWARE_FILE_TYPE,
    SIGNING_CERTIFICATE_FILE_TYPE,
    HASH_KEY_FILE_TYPE,
    SERVICE_PACK_FILE_TYPE
}FileType_t;

typedef enum
{
    UNKNOWN_SIG = 0,
    DESCRIPTOR,
    RSA_SIG
        
}FIRM_SIG_TYPE_t;

typedef __packed  struct
{
   uint16_t firmHeaderIdent;
   uint8_t bateryType;
   uint8_t frimFileCount;
   uint32_t firmSize;
   uint32_t firmCRC32;
   uint32_t firmHeaderCRC32;
}FirmwareHeader_t;

typedef __packed struct
{
    uint16_t fileHeaderIdent;
    uint8_t fileType;
    uint32_t fileSize;
    uint8_t fileSigType;
    uint16_t fileSigSize;
    uint32_t fileHeaderCRC32;
}FileHeader_t;

typedef __packed struct
{
    uint16_t discHeaderIdent;
    uint8_t fileSig[MAX_SIG_SIZE_POSSIBLE];
}DiscSigHeader_t;

typedef __packed struct
{
    uint16_t discHeaderIdent;
    uint32_t fileCRC32;
    uint32_t fileNCRC32;
    uint32_t fileStartAddress;
    
}DiscCRCHeader_t;

typedef __packed union
{
    DiscSigHeader_t sig;
    DiscCRCHeader_t crc;
    
}DiscHeader_t;

typedef struct errorInfo
{
    int8_t genError;
    int32_t errorInfo;
}errorInfo_t;

typedef struct firmErrorInfo
{
    errorInfo_t error;
    uint8_t fileType;
    int32_t seqNumber;
    int16_t isUpdated;
}firmErrorInfo_t;

void GetFirmwareFromQueue(void);
void ReInitFirmwareUploadStateMachine(void);
int8_t FirmwareUploadStateMachine(uint8_t buffer[],uint16_t bufferSize);
uint32_t CalculateCRC32(uint8_t buffer[],uint16_t bufferSize,uint32_t prevCRC32,BOOLEAN isLastPacket);
firmErrorInfo_t  * getFirmErrorInfo(BOOLEAN * isUpdated);

#endif /* __FIRMWARE_UPLOAD_H */