#include "SlHttpCli.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "WatchDog.h"

#define SEND_BUFLEN                 512u
#define RCV_BUFFERLEN               512u
#define SL_STATUS_BUFLEN            16

#define HTTP_VER "HTTP/1.1"
#define HTTP_PREFIX "http://"
#define HTTPS_PREFIX "https://"

/* HTTP Client instance state flags */
#define READ_FLAG       (0x01)
#define REDIRECT_FLAG   (0x02)
#define CHUNKED_FLAG    (0x04)
#define INPROGRESS_FLAG (0x08)

#define MAX_FIELD_NAME_LEN 24

static uint8_t rcvBuffer[RCV_BUFFERLEN];
static uint16_t bytesInRcvBuffer =0u;
static uint32_t index =0u;
CloudInfo_t httpCloudInfo;

uint16_t ReadFromRecvBuffer(uint8_t buffer[],uint16_t len);
static int16_t SlHTTPSndHeaders(slHTTPCliStruct_t * http_cli,slHTTPCliFieldStruct_t headers[],uint32_t num_of_headers,uint32_t last_header_flag);
static int16_t SlHttpClireadLine(slHTTPCliStruct_t * cli,uint8_t line[], uint16_t len, BOOLEAN *moreFlag);
static int16_t SlSndToSock(slHTTPCliStruct_t * http_cli,const char *fmt,...);
static uint8_t ToLowerCase(uint8_t c);

int16_t SlHTTPCliSetSocketOpt(slHTTPCliStruct_t * http_cli)
{
    int16_t status=-1;
    static uint32_t host_ip =0u;
    struct SlTimeval_t timeVal;  
    timeVal.tv_sec =  SOCKET_SND_RCV_TIMEOUT;    // Seconds  
    timeVal.tv_usec = 0;    // Microseconds. 10000 microseconds resolution  
    http_cli->sock_fd = -1;
    
    //Get new Cloud Information
    if( true == GetUpdatedCloudParam(&httpCloudInfo) )
    {
        http_cli->host = httpCloudInfo.CloudURL;
        http_cli->host_length = strlen( (const char *) httpCloudInfo.CloudURL);
        //reset Host IP for DNS query
        host_ip = 0;
    }
    
    //Get host ip address only once
    if(0u == host_ip)
    {
        //Set the Watch dog state to inactive
        SysWatchDogChangeState(WATCH_DOG_WIFI_TASK_INDEX,TASK_STATE_INACTIVE);
        
        status = sl_NetAppDnsGetHostByName((_i8 *)http_cli->host,http_cli->host_length,(_u32 *)&host_ip,http_cli->host_addr.sin_family);
        
        //Set the Watch dog state to active 
        SysWatchDogChangeState(WATCH_DOG_WIFI_TASK_INDEX,TASK_STATE_ALIVE);
        
        http_cli->host_addr.sin_addr.s_addr=sl_Htonl(host_ip);
        if(0 > status)
        {
            WiFiDebugPrint("Error: DNS Query for Server Failed. Error\n");
            WiFiDebugFlush();
        }
    }
    // enabling Secure Socket
    if(host_ip != 0u)
    {
        http_cli->sock_fd = sl_Socket((int16_t)http_cli->host_addr.sin_family, SL_SOCK_STREAM, SL_SEC_SOCKET);
        
        if (0 <= http_cli->sock_fd )
        {
            //Setting Socket Certificate options
            status = sl_SetSockOpt(http_cli->sock_fd, SL_SOL_SOCKET,SL_SO_SECURE_FILES_CA_FILE_NAME,http_cli->certificatePath,http_cli->CertificatePathLength);
            if(0 == status)
            {
                status = sl_SetSockOpt(http_cli->sock_fd,SL_SOL_SOCKET,SL_SO_RCVTIMEO,&timeVal, sizeof(timeVal));
                if(0 == status)
                {
                    status = sl_SetSockOpt(http_cli->sock_fd, SL_SOL_SOCKET, SL_SO_SECMETHOD, &http_cli->sec_method, sizeof(http_cli->sec_method));
                    if(0 == status)
                    {
                        /*
                        //setting Cipher Type to Use
                        status = sl_SetSockOpt(http_cli->sock_fd, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &http_cli->sec_cipher,  sizeof(http_cli->sec_cipher));
                        if(0==status)
                        {
                        WiFiDebugPrint("Error: SocketOpt Ciphers Selection failed.\n");
                        WiFiDebugFlush();
                        }
                        */ 
                        status=sl_Connect(http_cli->sock_fd,(const SlSockAddr_t *)&http_cli->host_addr,(int16_t)sizeof(http_cli->host_addr));
                    }
                    else
                    {
                        WiFiDebugPrint("Error: socket not created.\n");
                        WiFiDebugFlush();
                    }  
                }
                else
                {
                    WiFiDebugPrint("Error: SocketOpt SSL security setting failed setting failed .\n");
                    WiFiDebugFlush();
                }
            }
            else
            {
                WiFiDebugPrint("Error: SocketOpt seting timeout failed .\n");
                WiFiDebugFlush();
            }
        }
        else
        {
            WiFiDebugPrint("Error: SocketOpt Connetion failed due to connectivity or bad SSL certificate = %d \n",(http_cli->sock_fd));
            WiFiDebugFlush();
        }
        
    }
    else
    {
        WiFiDebugPrint("Error: socket creation failed.\n");
        WiFiDebugFlush();   
    }
    // Fixed for SL_ERROR_BSD_ESECUNKNOWNROOTCA
    
    if( (-468 == status) || (-467 == status) || (-461 == status) )
    {
        status = 0;
    }
    
    if(status !=0)
    {
        SlHTTPCliDisconnect(http_cli);
    }
return status; 
}

void SlHTTPCliDisconnect(slHTTPCliStruct_t * http_cli)
{
  if (http_cli->sock_fd > 0)
    {
      sl_Close(http_cli->sock_fd);
    }
}
int16_t SlHTTPSendBody(slHTTPCliStruct_t * http_cli,const uint8_t body[],uint16_t len)
{
    int16_t ret=0;
    ret=sl_Send(http_cli->sock_fd,body,(int16_t)len,0);
    
    //WiFiDebugPrint("%.*s",len,body);
    //WiFiDebugFlush();
    
    return ret;
}

int16_t SlHTTPCliSndRequest(slHTTPCliStruct_t * http_cli,const uint8_t method[],const uint8_t req_uri[],slHTTPCliFieldStruct_t http_headers[],uint32_t num_of_headers)
{
    int16_t ret = 0;
    
    ret=SlSndToSock(http_cli,"%s %s %s\r\n", method,req_uri ? req_uri : "/",HTTP_VER);
    if(0 < ret)
    {
        ret=SlHTTPSndHeaders(http_cli,http_headers,num_of_headers,1u);
    }
    return ret;
}

static int16_t SlHTTPSndHeaders(slHTTPCliStruct_t * http_cli,slHTTPCliFieldStruct_t headers[],uint32_t num_of_headers,uint32_t last_header_flag)
{
  int16_t ret=0;
  for(uint32_t indx= 0u;(indx<num_of_headers)&&(headers[indx].name!=NULL);++indx)
  {
    ret=SlSndToSock(http_cli,"%s: %s\r\n",headers[indx].name,headers[indx].value);
  }
  if(last_header_flag)
  {
      ret=SlSndToSock(http_cli,"\r\n");
  }
  return ret;
}
static int16_t SlSndToSock(slHTTPCliStruct_t * http_cli,const char *fmt,...)
{
   uint8_t sendbuf[SEND_BUFLEN];
   int16_t len = 0;
   int16_t ret;
   
   va_list ap;
   va_start(ap, fmt);
      len = vsnprintf((char *)sendbuf, SEND_BUFLEN, fmt, ap);
   va_end(ap);
   
   ret=sl_Send(http_cli->sock_fd,sendbuf,len,0);
   //WiFiDebugPrint("%.*s",len,sendbuf);
   //WiFiDebugFlush();
   return ret;
}

uint16_t ReadFromRecvBuffer(uint8_t buffer[],uint16_t len)
{
    uint16_t bytesToRead=0u;
    
    bytesToRead = len >bytesInRcvBuffer?bytesInRcvBuffer:len;
    
    memcpy(buffer,&rcvBuffer[index],(uint32_t)bytesToRead);
    index += bytesToRead;
    bytesInRcvBuffer -=bytesToRead;
    if(bytesInRcvBuffer == 0u)
    {
       index = 0u;
       bytesInRcvBuffer =0u;
    }
    return bytesToRead;
}

int32_t SlHttpCligetChunkedData(slHTTPCliStruct_t * cli,uint8_t body[],uint16_t len,BOOLEAN *moreFlag)
{
    BOOLEAN lastFlag = true;
    BOOLEAN mFlag = false;
    uint8_t crlf;
    int32_t ret = 0;
    int16_t chunkLen;

    *moreFlag = true;

    /* Check if in the middle of reading respone body? */
    chunkLen = (int16_t)cli->clen;
    if (chunkLen == 0) 
    {
        ret = SlHttpClireadLine(cli,body, len, &mFlag);
        if (ret < 0) 
        {
            return (ret);
        }

        chunkLen = (int32_t)strtoul((char const *)body, NULL, 16);
        /* don't need the remaining buffer */
        if (mFlag) 
        {
            ret = SlHttpClireadLine(cli,body, len, &mFlag);
            if (ret < 0) 
            {
                return (ret);
            }
        }

        if (chunkLen == 0) 
        {
            /* skip the rest (trailer headers) */
            do {
                ret = SlHttpClireadLine(cli,body, len, &mFlag);
                if (ret < 0) 
                {
                    return (ret);
                }

                /* Avoids fake do-while check */
                if (lastFlag) 
                {
                    body[0] = 'a';
                    lastFlag = false;
                }

                if (mFlag) 
                {
                    lastFlag = true;
                }

            }
            while (body[0] == '\0');

            *moreFlag = false;
            cli->clen = 0u;

            return (0);
        }

        cli->clen = (uint16_t)chunkLen;
    }

    if (chunkLen < (int16_t)len) 
    {
        len = (uint16_t)chunkLen;
    }

    ret = SlHttpCliReadRawResponseBody(cli, body, len);
    if (ret > 0) 
    {
        cli->clen -= (uint16_t)ret;
        /* Clean up the CRLF */
        if (cli->clen == 0u) 
        {
            chunkLen = SlHttpClireadLine(cli,&crlf, sizeof(crlf), &mFlag);
            if (chunkLen != 0) 
            {
                return (chunkLen);
            }
        }
    }

    return (ret);
}


static int16_t SlHttpClireadLine(slHTTPCliStruct_t * cli,uint8_t line[], uint16_t len, BOOLEAN *moreFlag)
{
   uint8_t c;
   int16_t ret = 0;

    *moreFlag = true;

    for (uint32_t i = 0u; ((i < len) && (*moreFlag == true)); i++) {

        if(bytesInRcvBuffer > 0u)
        {
            ret = (int16_t)ReadFromRecvBuffer(&c, 1u);
        }
        else
        {
            ret =  SlHttpCliReadRawResponseBody(cli, &c, 1u);
        }
        if ( ret < 0 )  
        {
            *moreFlag = false;
        }
        else if(c == '\n')
        {
            line[i] = 0u;
            *moreFlag = false;
        }
        else if ((c == '\r') || ( (i == 0u) && (c == ' ') ) ) 
        {
            i--;
            /* drop CR or drop leading SP*/
        }
        else 
        {
            line[i] = c;
        }
    }

    if(ret >0)
    {
        ret = 0;
    }
    return ret;
}

int16_t SlHttpCliReadRawResponseBody(slHTTPCliStruct_t * cli,uint8_t buffer[],uint16_t len)
{

  uint16_t bytesToReadFromBuffer = 0u;
  int16_t retVal = 0;
  
  bytesToReadFromBuffer = len > bytesInRcvBuffer?bytesInRcvBuffer:len;
  
  //check if data is present in buffer
  if(0u < bytesToReadFromBuffer)
  {
      // read data from buffer
     ReadFromRecvBuffer(buffer, bytesToReadFromBuffer);
     len -= bytesToReadFromBuffer;
  }
  // if more data is required
  if(0u < len)
  {
      //get data from TCP/IP stack
      retVal=sl_Recv(cli->sock_fd, &buffer[bytesToReadFromBuffer], (int16_t)len,0);
      
      WiFiDebugPrint("sl_Recv Data: %.*s", len , &buffer[bytesToReadFromBuffer]);
      WiFiDebugFlush();
  }
  
  if(retVal >= 0)
  {
    retVal += (int16_t)bytesToReadFromBuffer;
  }
  
  return retVal;
}

int16_t SlHttpCliReadFields(slHTTPCliStruct_t * cli,uint8_t buffer[],uint16_t len, BOOLEAN *more_flags)
{
  int16_t retVal=0;
  uint8_t *httpHeaderEnd=NULL;
  uint16_t bytesRecived = 0u;
  
  * more_flags=true;
  //Reset Rcv Buffer State
  bytesInRcvBuffer =0u;
  index =0u;
  
  do
  {
      retVal=SlHttpCliReadRawResponseBody(cli,&buffer[bytesRecived],(RCV_BUFFERLEN - bytesRecived) );
      if(0 < retVal)
      {
          // retVal at this point contain number of bytes reciver TCP/IP stack
          bytesRecived += (uint16_t)retVal;
          //Place null at the end of recived bytes
          buffer[bytesRecived] = (uint8_t)NULL;
          //check if HTPP Header end is recived
          httpHeaderEnd = (uint8_t *)strstr((char const *)buffer,"\r\n\r\n");
      }
  }while( (0 < retVal) && (NULL == httpHeaderEnd) && (bytesRecived < RCV_BUFFERLEN) );
  
      
  if(NULL !=httpHeaderEnd)
  {
      httpHeaderEnd +=strlen("\r\n\r\n");
      bytesInRcvBuffer = (uint16_t)( (uint32_t)(&buffer[bytesRecived])- (uint32_t)(httpHeaderEnd) );
      memcpy(rcvBuffer,httpHeaderEnd,bytesInRcvBuffer);
      retVal = (int16_t)(bytesRecived - bytesInRcvBuffer);
  }
  
  return retVal;

}

//------------------------------------------------------------------------------
//  int16_t StringCompareByLen(const char *str1, const char *str2, uint16_t len);
//   Author:  Ammar Younas
//   Date:    2017/04/5
//   Compare string By Length
//
//------------------------------------------------------------------------------
int16_t StringCompareByLen(const uint8_t str1[],const uint8_t str2[], uint16_t len)
{
    uint8_t c1;
    uint8_t c2;
    int16_t retVal = 0;
    
    for(uint32_t i = 0u; i < len; i++) 
    {
        c1 = ToLowerCase(str1[i]);
        c2 = ToLowerCase(str2[i]);
        if( (c1 == 0u) || ( c1!=c2) )
        {   
            retVal = (int16_t)c1 - (int16_t)c2;
        }
    }
    
    return retVal;
}

//------------------------------------------------------------------------------
//  static uint8_t ToLowerCase(uint8_t c);
//
//   Author:  Ammar Younas
//   Date:    2017/04/5
//   Convert string to lower case
//
//------------------------------------------------------------------------------
static uint8_t ToLowerCase(uint8_t c)
{
    if ( (c >= 65u) && (c <= 90u) ) 
    {
        c += 32u;
    }
    return (c);
}