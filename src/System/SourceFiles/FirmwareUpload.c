#include "FirmwareUpload.h"
#include "EventQ.h"
#include "WifiTask.h"
#include "ExtCommunication.h"

#define WIFI_BATTERY_IDENTIFIER                         1u

#define MAIN_HEADER_IDENT_SIZE                          2u
#define MAIN_HEADER_BATTERY_IDENT_SIZE                  1u
#define MAIN_HEADER_FILE_COUNT_SIZE                     1u
#define MAIN_HEADER_FILE_TOTAL_LENGTH_SIZE              4u
#define MAIN_HEADER_FILE_TOTAL_CRC_SIZE                 4u
#define MAIN_HEADER_FILE_HEADER_CRC_SIZE                4u


#define MAIN_HEADER_IDENT_OFFSET                        0u
#define MAIN_HEADER_FILE_COUNT_OFFSET                   2u
#define MAIN_HEADER_FILE_TOTAL_LENGTH_OFFSET            3u
#define MAIN_HEADER_FILE_TOTAL_CRC_OFFSET               7u
#define MAIN_HEADER_FILE_HEADER_CRC_OFFSET              11u


#define FILE_HEADER_IDENT_SIZE                          2u
#define FILE_HEADER_FILE_TYPE_SIZE                      1u
#define FILE_HEADER_FILE_SIZE_SIZE                      4u
#define FILE_HEADER_FILE_SIG_TYPE_SIZE                  1u
#define FILE_HEADER_FILE_SIG_SIZE_SIZE                  2u
#define FILE_HEADER_CRC_SIZE                            4u


#define FILE_HEADER_IDENT_OFFSET                        0u
#define FILE_HEADER_FILE_TYPE_OFFSET                    2u
#define FILE_HEADER_FILE_SIZE_OFFSET                    3u
#define FILE_HEADER_FILE_SIG_TYPE_OFFSET                7u
#define FILE_HEADER_FILE_SIG_SIZE_OFFSET                8u
#define FILE_HEADER_CRC_OFFSET                          10u

#define DISC_HEADER_IDENT_SIZE                          2u
#define DISC_HEADER_CRC_SIZE                            4u
#define DISC_HEADER_NCRC_SIZE                           4u
#define DISC_HEADER_START_ADDRESS_SIZE                  4u



#define DISC_HEADER_IDENT_OFFSET                        0u
#define DISC_HEADER_CRC_OFFSET                          2u
#define DISC_HEADER_NCRC_OFFSET                         6u
#define DISC_HEADER_START_ADDRESS_OFFSET                10u


#define FIRM_FILE_MASTER_TOKEN                          (1560865688)
#define MAIN_HEADER_IDENTIFIER                          "MH"
#define FILE_HEADER_IDENTIFIER                          "FH"
#define DISC_HEADER_IDENTIFIER                          "DS"

#define FILE_FORCE_CLOSE_CMD                            'A'

#define CRC32_INIT_VALUE                                (0xFFFFFFFFu)

typedef enum
{
    MAIN_HEADER_PARSING_STATE = 0,
    FILE_HEADER_PARSING_STATE,
    FILE_WRITING_STATE,
    FILE_DISC_PARSING_STATE,
    FILE_SIG_COPYING_STATE,
    FILE_VERIFIY_AND_COMMIT_STATE
        
}FirmwareCommitState_t;

typedef struct FileDiscriptor
{
    int32_t fileHandler;
    uint32_t fileOffset;
    uint32_t fileMasterToken;
    uint32_t fileDiscSize;
    uint32_t fileDiscOffset;
    
}FileDiscriptor_t;

typedef struct FirmwareUploadInfo
{
    FirmwareHeader_t firmHeader;
    FileHeader_t currentFileHeader;
    FileDiscriptor_t fileDisc;
    DiscHeader_t dicHeader;
    FirmwareCommitState_t firmState;
    firmErrorInfo_t firmErrorInfo;
    
}FirmwareUploadInfo_t;


const uint8_t * const slFilePath[]={"Unknown","/sys/mcuflashimg.bin","dummy-trusted-cert","/sys/hashkey","/sys/servicepack.ucf"};
FirmwareUploadInfo_t firmInfo;



uint32_t CRC32Table[] =
{
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

int8_t ParseFirmwareHeader(FirmwareUploadInfo_t *ptrFirmInfo,uint8_t buffer[],uint8_t sizeofBuffer);
int8_t ParseFileHeader(FirmwareUploadInfo_t *ptrFirmInfo,uint8_t buffer[],uint8_t sizeofBuffer);
int8_t ParseDiscriptorHeader(FirmwareUploadInfo_t *ptrFirmInfo,uint8_t buffer[],uint8_t bufferSize);
int32_t CommitFirmwareToDataFlash(uint8_t buffer[],uint16_t bufferSize);
int8_t CopyRSASig(FirmwareUploadInfo_t *ptrFirmInfo,uint8_t buffer[],uint8_t bufferSize);

int8_t OpenFileForWrite(FirmwareUploadInfo_t *ptrFirmInfo);
int8_t WriteFileDataToFlash(FirmwareUploadInfo_t *ptrFirmInfo,uint8_t buffer[],uint8_t bufferSize);
int8_t CloseFileForWrite(FirmwareUploadInfo_t *ptrFirmInfo);


void GetFirmwareFromQueue(void)
{
    PTR_MSG_EVT_t ptrEvent = NULL;
    int8_t retVal = -1;
    while (
             ( ptrEvent = (PTR_MSG_EVT_t)READ_EVENT_FROM_FIRM_QUEUE() )
          )
    {
        //WiFiDebugPrint("%.*s\r\n",ptrEvent->msgSize,ptrEvent->Msg);
        //WiFiDebugFlush();
        
        retVal = FirmwareUploadStateMachine(ptrEvent->Msg,ptrEvent->msgSize);
        if(0 == retVal)
        {
            if( firmInfo.firmState != FILE_VERIFIY_AND_COMMIT_STATE )
            {
                ptrEvent = (PTR_MSG_EVT_t)GET_EVENT_FROM_FIRM_QUEUE();
                FreeEvent( (PTR_EVENT_t)ptrEvent);
            }
        }
        else
        {
            WiFiDebugPrint("State Filed Reason Code:\r\n",retVal);
            WiFiDebugFlush();
            
            firmInfo.firmErrorInfo.fileType = firmInfo.currentFileHeader.fileType;
            firmInfo.firmErrorInfo.isUpdated = true;
            
            ptrEvent = (PTR_MSG_EVT_t)GET_EVENT_FROM_FIRM_QUEUE();
            FreeEvent( (PTR_EVENT_t)ptrEvent);
            
            break;
        }
    }
}

void ReInitFirmwareUploadStateMachine(void)
{
    firmInfo.firmState = MAIN_HEADER_PARSING_STATE;
}

int8_t FirmwareUploadStateMachine(uint8_t buffer[],uint16_t bufferSize)
{
    int8_t retVal = -1;
    switch(firmInfo.firmState)
    {
    case MAIN_HEADER_PARSING_STATE :
        WiFiDebugPrint("MH Parsing STATE\r\n");
        WiFiDebugFlush();
        retVal = ParseFirmwareHeader(&firmInfo,buffer,bufferSize);
        
        if(retVal != 0)
        {
            WiFiDebugPrint("MH Parsing Failed\r\n");
            WiFiDebugFlush();
        }
        break;
    case FILE_HEADER_PARSING_STATE :
        
        WiFiDebugPrint("FH Parsing STATE\r\n");
        WiFiDebugFlush();
        
        retVal = ParseFileHeader(&firmInfo,buffer,bufferSize);
        if(retVal == 0)
        {
            WiFiDebugPrint("File Opening STATE\r\n");
            WiFiDebugFlush();
            OpenFileForWrite(&firmInfo);
        }
        else
        {
            WiFiDebugPrint("FH Parsing Failed\r\n");
            WiFiDebugFlush();
        }
        break;
    case FILE_WRITING_STATE :
        WiFiDebugPrint("File Writting STATE\r\n");
        WiFiDebugFlush();
        retVal = WriteFileDataToFlash(&firmInfo,buffer,bufferSize);
        if(retVal != 0)
        {
            WiFiDebugPrint("File Parsing Failed\r\n");
            WiFiDebugFlush();
        }
        break;
    case FILE_DISC_PARSING_STATE:
        retVal = ParseDiscriptorHeader(&firmInfo,buffer,bufferSize);
        break;
    case FILE_SIG_COPYING_STATE :
        retVal = CopyRSASig(&firmInfo,buffer,bufferSize);
        break;
        
    case FILE_VERIFIY_AND_COMMIT_STATE :
        retVal = CloseFileForWrite(&firmInfo);
        break;
    default:
        break;
    }
    return retVal;
}

int8_t ParseFirmwareHeader(FirmwareUploadInfo_t *ptrFirmInfo,uint8_t buffer[],uint8_t bufferSize)
{
    int8_t retVal = -1;
    uint32_t CalculatedCRC = CRC32_INIT_VALUE;
    //Check Header Identification and buffer Size
    if( (0 ==  strncmp(MAIN_HEADER_IDENTIFIER,(char const *)buffer,MAIN_HEADER_IDENT_SIZE) ) && ( sizeof(FirmwareHeader_t) == bufferSize )  )
    {
        //Copy data here
        memcpy( &(ptrFirmInfo->firmHeader),buffer,bufferSize);
        
        if( WIFI_BATTERY_IDENTIFIER == ptrFirmInfo->firmHeader.bateryType )
        {
            
            //Calculate CRC here
            CalculatedCRC = CalculateCRC32(buffer, (bufferSize - MAIN_HEADER_FILE_HEADER_CRC_SIZE) ,CalculatedCRC,true);
            if(ptrFirmInfo->firmHeader.firmHeaderCRC32 == CalculatedCRC)
            {
                ptrFirmInfo->firmState = FILE_HEADER_PARSING_STATE;
                retVal = 0;
            }
            else
            {
                // Set ERROR INFO HERE
                ptrFirmInfo->firmErrorInfo.error.genError = ERROR_CODE_MAIN_HEADER_CRC_ERROR;
                ptrFirmInfo->firmErrorInfo.error.errorInfo = NULL;
            }
        }
        else
        {
            // Set ERROR INFO HERE
            ptrFirmInfo->firmErrorInfo.error.genError = ERROR_CODE_BATTERY_IDENT_ERROR;
            ptrFirmInfo->firmErrorInfo.error.errorInfo = NULL;
            
        }
    }
    else
    {
        // Set ERROR INFO HERE
        ptrFirmInfo->firmErrorInfo.error.genError = ERROR_CODE_MAIN_HEADER_IDENT_ERROR;
        ptrFirmInfo->firmErrorInfo.error.errorInfo = NULL;
    }
    
    return retVal;
}

int8_t ParseFileHeader(FirmwareUploadInfo_t *ptrFirmInfo,uint8_t buffer[],uint8_t bufferSize)
{
    int8_t retVal = -1;
    uint32_t CalculatedCRC = CRC32_INIT_VALUE;
    int32_t LocalfileHandler;
    //Check Header Identification and buffer Size
    if( (0 == strncmp(FILE_HEADER_IDENTIFIER,(char const *)buffer,FILE_HEADER_IDENT_SIZE) ) && (sizeof(FileHeader_t) == bufferSize)  )
    {
        //Copy data here
        memcpy( &(ptrFirmInfo->currentFileHeader),buffer,bufferSize);
        //Calculate CRC here
        CalculatedCRC = CalculateCRC32(buffer,(bufferSize - FILE_HEADER_CRC_SIZE),CalculatedCRC,true);
        if(ptrFirmInfo->currentFileHeader.fileHeaderCRC32 == CalculatedCRC)
        {
          LocalfileHandler = ptrFirmInfo->fileDisc.fileHandler;
          memset( &(ptrFirmInfo->fileDisc),0,sizeof(FileDiscriptor_t) );
          ptrFirmInfo->fileDisc.fileHandler = LocalfileHandler;
          retVal = 0;
        }
        else
        {
            // Set ERROR INFO HERE
            ptrFirmInfo->firmErrorInfo.error.genError = ERROR_CODE_FILE_HEADER_CRC_ERROR;
            ptrFirmInfo->firmErrorInfo.error.errorInfo = NULL;
        }
    }
    else
    {
        // Set ERROR INFO HERE
        ptrFirmInfo->firmErrorInfo.error.genError = ERROR_CODE_FILE_HEADER_IDENT_ERROR;
        ptrFirmInfo->firmErrorInfo.error.errorInfo = NULL;
    }
    
    return retVal;
}

int8_t ParseDiscriptorHeader(FirmwareUploadInfo_t *ptrFirmInfo,uint8_t buffer[],uint8_t bufferSize)
{
    int8_t retVal = -1;
    //Check Header Identification and buffer Size
    if( 0 == strncmp(DISC_HEADER_IDENTIFIER,(char const *)buffer,DISC_HEADER_IDENT_SIZE) )
    {
        WiFiDebugPrint("Descriptor Buffer Size %d \r\n",bufferSize);
        WiFiDebugFlush();
        ptrFirmInfo->fileDisc.fileDiscSize = ptrFirmInfo->currentFileHeader.fileSigSize - DISC_HEADER_IDENT_SIZE; 
        memcpy( &(ptrFirmInfo->dicHeader.sig.discHeaderIdent),buffer,DISC_HEADER_IDENT_SIZE);
        memcpy( &(ptrFirmInfo->dicHeader.sig.fileSig),&buffer[DISC_HEADER_IDENT_SIZE],(bufferSize - DISC_HEADER_IDENT_SIZE) );
        ptrFirmInfo->fileDisc.fileDiscSize -= (bufferSize - DISC_HEADER_IDENT_SIZE);
        ptrFirmInfo->fileDisc.fileDiscOffset += (bufferSize - DISC_HEADER_IDENT_SIZE);
        ptrFirmInfo->firmState = (ptrFirmInfo->currentFileHeader.fileSigType ==  DESCRIPTOR) ? FILE_VERIFIY_AND_COMMIT_STATE : FILE_SIG_COPYING_STATE ;
        retVal = 0;
    }
    else
    {
        // Set ERROR INFO HERE
        ptrFirmInfo->firmErrorInfo.error.genError = ERROR_CODE_DISC_HEADER_IDENT_ERROR;
        ptrFirmInfo->firmErrorInfo.error.errorInfo = NULL;
    }
    
    return retVal;
}

int8_t CopyRSASig(FirmwareUploadInfo_t *ptrFirmInfo,uint8_t buffer[],uint8_t bufferSize)
{
    int8_t retVal = 0;
    int32_t chunkLen = 0;
    chunkLen = FIND_MIN(bufferSize,ptrFirmInfo->fileDisc.fileDiscSize);
    
    WiFiDebugPrint("Chunk Size %d \r\n",chunkLen);
    WiFiDebugFlush();
    
    memcpy( &(ptrFirmInfo->dicHeader.sig.fileSig[ptrFirmInfo->fileDisc.fileDiscOffset]),buffer,chunkLen);
    ptrFirmInfo->fileDisc.fileDiscSize -= chunkLen;
    ptrFirmInfo->fileDisc.fileDiscOffset += chunkLen;
    if(ptrFirmInfo->fileDisc.fileDiscSize == 0)
    {
        ptrFirmInfo->firmState = FILE_VERIFIY_AND_COMMIT_STATE;
    }
 
    return retVal;
}

int8_t OpenFileForWrite(FirmwareUploadInfo_t *ptrFirmInfo)
{
    int32_t retVal=-1;
    uint32_t fileFlag = 0;
    uint8_t forcedClose = FILE_FORCE_CLOSE_CMD;
    
      
    fileFlag = SL_FS_CREATE|SL_FS_CREATE_FAILSAFE|SL_FS_OVERWRITE|SL_FS_CREATE_PUBLIC_WRITE;
    
    ptrFirmInfo->fileDisc.fileMasterToken = FIRM_FILE_MASTER_TOKEN;
    fileFlag = (ptrFirmInfo->currentFileHeader.fileSigType == RSA_SIG) ? (fileFlag | SL_FS_CREATE_SECURE) : fileFlag;
    
    sl_FsClose(ptrFirmInfo->fileDisc.fileHandler,0,&forcedClose, 1);
    
    ptrFirmInfo->fileDisc.fileHandler = sl_FsOpen(slFilePath[ptrFirmInfo->currentFileHeader.fileType],
                                                  fileFlag|SL_FS_CREATE_MAX_SIZE( ptrFirmInfo->currentFileHeader.fileSize ) ,
                                                  (unsigned long *)&(ptrFirmInfo->fileDisc.fileMasterToken));
    if( 0 < (ptrFirmInfo->fileDisc.fileHandler) )
    {
        ptrFirmInfo->firmState = FILE_WRITING_STATE;
        WiFiDebugPrint("%s File Opened For Writting\r\n",slFilePath[ptrFirmInfo->currentFileHeader.fileType]);
        WiFiDebugFlush();
        retVal = 0;
        
    }
    else
    {
        WiFiDebugPrint("%s File Opened Failed Reason %d\r\n",slFilePath[ptrFirmInfo->currentFileHeader.fileType],ptrFirmInfo->fileDisc.fileHandler );
        WiFiDebugFlush();
        //Set ERROR INFO HERE
        ptrFirmInfo->firmErrorInfo.error.genError = ERROR_CODE_FILE_OPENING_FAILED;
        ptrFirmInfo->firmErrorInfo.error.errorInfo = retVal;
    }
    return retVal;
    
}

int8_t CloseFileForWrite(FirmwareUploadInfo_t *ptrFirmInfo)
{
    int32_t retVal=-1;
    uint8_t * ptrToSigFileName = NULL;
    
    WiFiDebugPrint("\nTranfer Of img to CC3100 Flash Is Completed\n");
    WiFiDebugFlush();
    
    if( ptrFirmInfo->currentFileHeader.fileSigType == RSA_SIG )
    {
        WiFiDebugPrint("%s",slFilePath[SIGNING_CERTIFICATE_FILE_TYPE]);
        WiFiDebugFlush();
        
        ptrToSigFileName = (ptrFirmInfo->currentFileHeader.fileType != SERVICE_PACK_FILE_TYPE)? (uint8_t *)slFilePath[SIGNING_CERTIFICATE_FILE_TYPE] : NULL;
     
        retVal = sl_FsClose( ptrFirmInfo->fileDisc.fileHandler,ptrToSigFileName,ptrFirmInfo->dicHeader.sig.fileSig,(ptrFirmInfo->currentFileHeader.fileSigSize - DISC_HEADER_IDENT_SIZE) );
                
        if(0 != retVal)
        {
            // Set ERROR INFO HERE
            ptrFirmInfo->firmErrorInfo.error.genError = ERROR_CODE_FILE_CLOSING_FAILED;
            ptrFirmInfo->firmErrorInfo.error.errorInfo = retVal;
        }
    }
    else
    {
        retVal = sl_FsClose( ptrFirmInfo->fileDisc.fileHandler,0,0,0 );
        if(0 != retVal)
        {
            // Set ERROR INFO HERE
            ptrFirmInfo->firmErrorInfo.error.genError = ERROR_CODE_FILE_CLOSING_FAILED;
            ptrFirmInfo->firmErrorInfo.error.errorInfo = retVal;
        }
    }
    
    if(--ptrFirmInfo->firmHeader.frimFileCount)
    {
        ptrFirmInfo->firmState = FILE_HEADER_PARSING_STATE;
    }
    else
    {
        ptrFirmInfo->firmState = MAIN_HEADER_PARSING_STATE;
    }
    
    return (int8_t)retVal;
    
}

int8_t WriteFileDataToFlash(FirmwareUploadInfo_t *ptrFirmInfo,uint8_t buffer[],uint8_t bufferSize)
{
    int32_t retVal = -1;
    uint32_t chunkLen = 0;
    
    chunkLen = FIND_MIN(bufferSize, ptrFirmInfo->currentFileHeader.fileSize);
    
    retVal = sl_FsWrite(ptrFirmInfo->fileDisc.fileHandler, ptrFirmInfo->fileDisc.fileOffset, (_u8 *)buffer,chunkLen);
    if (retVal > 0)
    {
        ptrFirmInfo->currentFileHeader.fileSize -= chunkLen;
        ptrFirmInfo->fileDisc.fileOffset += chunkLen;
        
        //WiFiDebugPrint("%.*s", chunkLen,buffer);
        //WiFiDebugPrint("%d bytes of img are transferd to CC3100 Flash...\n",ptrFirmInfo->fileDisc.fileOffset);
        //WiFiDebugFlush();
        
        retVal = 0;
    }
    else
    {
        // Set ERROR INFO HERE
        ptrFirmInfo->firmErrorInfo.error.genError = ERROR_CODE_FILE_WRITTING_FAILED;
        ptrFirmInfo->firmErrorInfo.error.errorInfo = retVal;
    }
    
    if(0 == ptrFirmInfo->currentFileHeader.fileSize)
    {
        ptrFirmInfo->firmState = FILE_DISC_PARSING_STATE;
    }
    
    return retVal;
}

uint32_t CalculateCRC32(uint8_t buffer[],uint16_t bufferSize,uint32_t prevCRC32,BOOLEAN isLastPacket)
{
    uint32_t CRC32 = prevCRC32;
    
    for (uint16_t index = 0; index < bufferSize; ++index)
    {
        CRC32 = (CRC32 >> 8) ^ CRC32Table[( (CRC32 ^ buffer[index]) & 0XFF )];
    }
    
    if(isLastPacket)
    {
      CRC32 = ~CRC32;
    }
    return CRC32;
}

firmErrorInfo_t  * getFirmErrorInfo(BOOLEAN * isUpdated)
{
  *isUpdated = firmInfo.firmErrorInfo.isUpdated;
  firmInfo.firmErrorInfo.isUpdated = false;
  
  return &firmInfo.firmErrorInfo;
}