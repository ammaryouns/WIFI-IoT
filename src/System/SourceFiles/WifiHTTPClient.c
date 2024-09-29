//==============================================================================
// Start of file
//==============================================================================
//
//  WifiHttpApp.h
//
//  Copyright (C) 2016 by Industrial Scientific.
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
//  Source:        WifiHttpApp.c
//
//  Project:       Morrison
//
//  Author:        Ammar Younas
//
//  Date:          2017/2/15
//
//  Revision:      1.0
//
//==============================================================================
//  FILE DESCRIPTION
//==============================================================================
//
//! \file
//! This is a Wifi HTPPS module.
//
//==============================================================================
//  REVISION HISTORY
//==============================================================================
//
// Revision: 1.0 2016/12/09 Ammar Younas
//           Initial Version
//
//==============================================================================

//==============================================================================
//  INCLUDES
//==============================================================================
#include <WifiHTTPClient.h>
#include "WatchDog.h"

#define SL_HTTP_NUM_HEADERS_FOR_ACCESS_TOKEN                    7
#define SL_HTTP_NUM_HEADERS_FOR_POST_REQ                        9

static uint8_t certificatePath[]=SL_HTTP_CERTIFICATE;
static uint8_t slWifiRecvResponseBuffer[SL_HTTP_RECV_BUFFER_SIZE];
static uint8_t slHttpDataURLBuffer[SL_HTTP_URL_BUFFER_SIZE];
static uint8_t slWifiHttpDataBuffer[SL_HTTP_DATA_BUFFER_SIZE];
//==============================================================================
//  LOCAL FUNCTION Declaration
//==============================================================================
static int32_t GetAccessToken(slHTTPCliStruct_t *http_cli,uint8_t recvBuffer[],uint16_t recvBufferSize);
static int32_t SlParsHttp(void * responce_buffer,size_t len,size_t pre_length,slHTTPParse_t * http_parser);
static int32_t PostDataToINet(slHTTPCliStruct_t *httpCli,uint8_t postReqUri[],uint8_t data[],uint32_t dataLen,uint8_t recvBuffer[],uint16_t recvBufferSize);
static int32_t GetDataFromINet(slHTTPCliStruct_t *httpCli,uint8_t getReqUri[],uint8_t recvBuffer[],uint16_t recvBufferSize,PTR_EVENT_t ptrComEvent);
//==============================================================================
//  GLOBAL DATA Declaration
//==============================================================================
extern CloudInfo_t httpCloudInfo;
static BOOLEAN isTokenValid = false;
static uint8_t token[MAX_JSON_TOKEN_STRING_SIZE]={0};
static BOOLEAN wifiComError = false;
//------------------------------------------------------------------------------
// void HttpInit(slHTTPCliStruct_t *httpCli)
//
//   Author:  Ammar Younas
//   Date:    2017/04/5
//   Wifi HTTP Task
//
//------------------------------------------------------------------------------
void HttpInit(slHTTPCliStruct_t *httpCli)
{
    httpCli->host = httpCloudInfo.CloudURL;
    httpCli->host_length = strlen((const char *)httpCloudInfo.CloudURL);
    httpCli->host_addr.sin_family=(uint16_t)SL_AF_INET;
    httpCli->host_addr.sin_port=sl_Htons(INET_SSL_PORT);
    httpCli->clen=0u;
    httpCli->sec_method=(uint8_t)SL_SO_SEC_METHOD_TLSV1_2;
    httpCli->sec_cipher=(uint32_t)SL_SEC_MASK_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256;
    httpCli->certificatePath=certificatePath;
    httpCli->CertificatePathLength=sizeof(certificatePath);
}


void SndQueueEventsToINET(slHTTPCliStruct_t *httpCli,uint8_t eventToSend)
{
    PTR_EVENT_t ptrEvent;
    int32_t reTransmitAttempts=(int32_t)EVENT_RETRANSMIT_ATTEMPTS;
    int32_t retVal=-1;
    int32_t size = 0;
    BOOLEAN isConnected = false;
    
    WiFiDebugPrint("**SndQueueEventsToINET : Events to send %d \r\n",eventToSend);
    WiFiDebugFlush();
    
    while (
           (0 != eventToSend)
           &&(false ==wifiComError)
           &&(ptrEvent = READ_EVENT_FROM_COMM_QUEUE())
           &&( WIFI_CONNECTED == GetWifiState() ) 
             )
    {
        reTransmitAttempts=EVENT_RETRANSMIT_ATTEMPTS;
        size = jsonCreatorAndParser[((PTR_COMM_EVT_t)ptrEvent)->commEvtType].jCreator(slHttpDataURLBuffer, SL_HTTP_URL_BUFFER_SIZE, slWifiHttpDataBuffer, SL_HTTP_DATA_BUFFER_SIZE, ptrEvent);
        
        if(0 < size)
        {
//            WiFiDebugPrint("WiFI HTTP url:%.*s\r\n",SL_HTTP_URL_BUFFER_SIZE,slHttpDataURLBuffer);
//            WiFiDebugFlush();
            do
            {
                //Clear the watch dog counter for this task 
                SysWatchDogClearCounter(WATCH_DOG_WIFI_TASK_INDEX);
             
                if( false == isConnected )
                {
                    retVal=SlHTTPCliSetSocketOpt(httpCli);
                    isConnected = ( (retVal ==0) ? true:false );
                }
                // Get acess token from Server
                retVal = GetAccessToken(httpCli,slWifiRecvResponseBuffer,SL_HTTP_RECV_BUFFER_SIZE);
                if(0 < retVal)
                {
                    switch(((PTR_COMM_EVT_t)ptrEvent)->commEvtType)
                    {
                    case REGISTER_INST_ON_INET:
                        retVal = GetDataFromINet(httpCli,slHttpDataURLBuffer, slWifiRecvResponseBuffer,SL_HTTP_RECV_BUFFER_SIZE, ptrEvent);
                        break;
                    default:
                        
                        retVal=PostDataToINet(httpCli,slHttpDataURLBuffer,slWifiHttpDataBuffer,size,slWifiRecvResponseBuffer,SL_HTTP_RECV_BUFFER_SIZE);
                        break;
                    }
                    
                    WiFiDebugPrint("WiFI HTTP: SndQueueEventsToINET, Return Code: %d\r\n",retVal);
                    WiFiDebugFlush();
                    
                    if(0 < retVal)
                    {
                      //jsonCreatorAndParser[((PTR_COMM_EVT_t)ptrEvent)->commEvtType].jParser(slWifiRecvResponseBuffer, retVal, NULL, NULL, NULL);                                                        
                      ParseResponce(slWifiRecvResponseBuffer, retVal, NULL, NULL,ptrEvent);
                      // Release the event from queue
                      ptrEvent = GET_EVENT_FROM_COMM_QUEUE();
                      if(NULL != ptrEvent)
                      {
                        FreeEvent(ptrEvent);
                         --eventToSend;
                      }
                    }
                    else
                    {
                        // In case of Error Close Socket
                        isConnected = false;
                        SlHTTPCliDisconnect(httpCli);
                    }
                }
                else
                {
                    WiFiDebugPrint("WiFI HTTP: Token Access Failure, Reason Code: %d\r\n",retVal);
                    WiFiDebugFlush();
                }
                
            }while( (--reTransmitAttempts) && (0 >= retVal) );
            
            if(0 == reTransmitAttempts)
            {
                wifiComError =true;
            }
            else
            {
                wifiComError =false;
            }
            //Update the status of Wifi connection.
            UpdateConnectionStaus();
        }
    }
    SlHTTPCliDisconnect(httpCli);
}

//------------------------------------------------------------------------------
//   void TestConnectionWithINET( slHTTPCliStruct_t *httpCli )
//
//   Author:  Ammar Younas
//   Date:    2017/04/5
//   Test Connection with INet Server
//
//------------------------------------------------------------------------------

void TestConnectionWithINET( slHTTPCliStruct_t *httpCli )
{
  BOOLEAN isConnected=false;
  int16_t localiNetStatus = 0;
  
  localiNetStatus = SlHTTPCliSetSocketOpt(httpCli);
  //Test Inet Connectivity
  //if(0 == SlHTTPCliSetSocketOpt(httpCli))
  if (0 == localiNetStatus)
  {
    isConnected = true;
    SlHTTPCliDisconnect(httpCli);
  }
  
  if(false == isConnected )
  {
    wifiComError = true;
    WiFiDebugPrint("WiFI HTTP Test Connection Fail, Reason Code: %d\r\n",localiNetStatus);
    WiFiDebugFlush();
  }
  else
  {
    wifiComError = false;
    WiFiDebugPrint("WiFI HTTP Test Connection Pass, Return Code: %d\r\n",localiNetStatus);
    WiFiDebugFlush();
  }
  
  //Update the status of Wifi connection.
  UpdateConnectionStaus();
}

//------------------------------------------------------------------------------
//  static int32_t PostDataToINet(slHTTPCliStruct_t *httpCli,char postReqUri[],char data[]
//                              ,uint32_t dataLen,char recvBuffer[]
//                                  ,uint32_t recvBufferSize)
//
//   Author:  Ammar Younas
//   Date:    2017/04/5
//   Post Data To INet Server
//
//------------------------------------------------------------------------------
static int32_t PostDataToINet(slHTTPCliStruct_t *httpCli,uint8_t postReqUri[],uint8_t data[]
                              ,uint32_t dataLen,uint8_t recvBuffer[]
                                  ,uint16_t recvBufferSize)
{
    slHTTPParse_t httpParser;
    uint8_t dataLenStr[SL_INT_TO_STRING_BUFFER_SIZE]={0};
    BOOLEAN more_flags=0;
    int32_t retVal=-1;
    uint32_t ChunkedBytesRead = 0;

    slHTTPCliFieldStruct_t  httpHeaders[SL_HTTP_NUM_HEADERS_FOR_POST_REQ]=
    {{SL_HTTPCli_FIELD_NAME_HOST,CLOUD_URL},
    {SL_HTTPCli_FIELD_NAME_USER_AGENT,SL_USER_AGENT},
    {SL_HTTPCli_FIELD_NAME_ACCEPT,"*/*"},
    {SL_HTTPCli_FIELD_NAME_CONTENT_TYPE,SL_CONTENT_TYPE_JSON},
    {SL_HTTPCli_FIELD_NAME_AUTHORIZATION,token},
    {SL_HTTPCli_FIELD_NAME_CONTENT_LENGTH,dataLenStr},
    {SL_HTTPCli_FIELD_NAME_CONNECTION,SL_CONNECTION_TYPE},
    {NULL,NULL}};

    uint32_t numOfHeaders=(sizeof(httpHeaders)/sizeof(httpHeaders[0]));


    snprintf((char *)dataLenStr,SL_INT_TO_STRING_BUFFER_SIZE,"%d",dataLen);
    retVal=SlHTTPCliSndRequest(httpCli,(const uint8_t *)SL_HTTPCli_METHOD_POST,(const uint8_t *)postReqUri,httpHeaders,numOfHeaders);
    if(0 <retVal)
    {
        WiFiDebugPrint("WIFI HTTP JSON PayLoad:%.*s\r\n",dataLen,data);
        WiFiDebugFlush();
        
        retVal = SlHTTPSendBody(httpCli,data,dataLen);
        
        if(0 < retVal)
        {
            retVal=SlHttpCliReadFields(httpCli,recvBuffer,recvBufferSize,&more_flags);
            if(0 < retVal)
            {
                retVal=SlParsHttp(recvBuffer,(uint32_t)retVal,0u,&httpParser);
                if( 0 > retVal )
                {
                    
                    WiFiDebugPrint("Pico HTTP Parser ERROR in (PostDataToINet)WIFI ERROR CODE:%d\n",retVal);
                    WiFiDebugFlush();
                    
                }
                else if( (HTTPStd_CREATED == httpParser.status) || (HTTPStd_OK == httpParser.status) )
                {
                    
                    WiFiDebugPrint("WIFI HTTP Status Code:%d\n",httpParser.status);
                    WiFiDebugFlush();
                    
                    if(TRANSFER_CONTENT_LENGTH == httpParser.transfer_encoding)
                    {
                        uint16_t bytesToRead=(httpParser.content_length >recvBufferSize ?recvBufferSize:httpParser.content_length);
                        retVal=SlHttpCliReadRawResponseBody(httpCli,recvBuffer,bytesToRead);
                        
                        if(0 < retVal)
                        {                                
                            WiFiDebugPrint("WIFI Response ID%.*s\r\n",retVal,recvBuffer);
                            WiFiDebugFlush();
                            
                        }
                        else
                        {
                            
                            WiFiDebugPrint("Failed To Get Response Body in (PostDataToINet)WIFI ERROR CODE:%d\n",retVal);
                            WiFiDebugFlush();
                            
                        }
                        
                    }
                    else if(CHUNKED==httpParser.transfer_encoding)
                    {
                      
                      WiFiDebugPrint("Wifi HTTP Module cannot handel chunked data currently in (PostDataToINet)\n");
                      WiFiDebugFlush();
                      
                      do{
                        
                        retVal = SlHttpCligetChunkedData(httpCli,&recvBuffer[ChunkedBytesRead],recvBufferSize-ChunkedBytesRead,&more_flags);
                        if(0 < retVal)
                        {
                          ChunkedBytesRead += retVal;
                        }
                        
                      }while(true == more_flags && 0 < retVal);
                      
                      if(0 < ChunkedBytesRead && (0 == retVal) && (false == more_flags) )
                      {
                        retVal = ChunkedBytesRead;
                        recvBuffer[ChunkedBytesRead] = 0x00;
                        WiFiDebugPrint("WIFI Acess Token:\n%.*s\n",ChunkedBytesRead,slWifiRecvResponseBuffer);
                        WiFiDebugFlush();
                        //SlHttpCligetChunkedData(http_cli,&recvBuffer[retVal],recvBufferSize-(uint32_t)retVal,&more_flags);
                      }
                      
                    }
                    else
                    {
                        
                        WiFiDebugPrint("Wifi HTTP module get unknown Transfer Encoding in (PostDataToINet)\n");
                        WiFiDebugFlush();
                        
                    }
                }
                else if( HTTPStd_UNAUTHORIZED == httpParser.status )
                {
                    isTokenValid = false;
                    WiFiDebugPrint("Wifi Status Code:%d\nWifi HTTP Message:%.*s\n",httpParser.status,httpParser.msg_len,httpParser.msg);
                    WiFiDebugFlush();
                    retVal = -1;
                }
                else if( (HTTPStd_NON_AUTHORITATIVE_INFORMATION < httpParser.status) && (HTTPStd_STATUS_CODE_END > httpParser.status) )
                { 
                    WiFiDebugPrint("Wifi Status Code:%d\nWifi HTTP Message:%.*s\n",httpParser.status,httpParser.msg_len,httpParser.msg);
                    WiFiDebugFlush();
                    WiFiDebugPrint("Event Will Be Discarded\n");
                    WiFiDebugFlush();
                    retVal = ((-1) * httpParser.status);
                }
                else
                {
                    retVal= -1;
                }
            }
        }
    }
    return retVal;
}

//------------------------------------------------------------------------------
//  static int32_t GetDataFromINet(slHTTPCliStruct_t *httpCli,char getReqUri[],char recvBuffer[]
//                                  ,uint32_t recvBufferSize)
//
//   Author:  Ammar Younas
//   Date:    2017/04/5
//   Post Data To INet Server
//
//------------------------------------------------------------------------------
static int32_t GetDataFromINet(slHTTPCliStruct_t *httpCli,uint8_t getReqUri[],
                               uint8_t recvBuffer[],uint16_t recvBufferSize, PTR_EVENT_t ptrComEvent)
{
    slHTTPParse_t httpParser;
    BOOLEAN more_flags= false;
    int32_t retVal=-1;  
    uint32_t bytesToRead = 0u;
    uint32_t ChunkedBytesRead = 0;

    slHTTPCliFieldStruct_t  httpHeaders[SL_HTTP_NUM_HEADERS_FOR_POST_REQ]=
    {{SL_HTTPCli_FIELD_NAME_HOST,CLOUD_URL},
    {SL_HTTPCli_FIELD_NAME_USER_AGENT,SL_USER_AGENT},
    {SL_HTTPCli_FIELD_NAME_ACCEPT,"*/*"},
    {SL_HTTPCli_FIELD_NAME_AUTHORIZATION,(uint8_t *)token},
    {SL_HTTPCli_FIELD_NAME_CONNECTION,SL_CONNECTION_TYPE},
    {NULL,NULL}};

    uint32_t numOfHeaders=(sizeof(httpHeaders)/sizeof(httpHeaders[0]));

    retVal=SlHTTPCliSndRequest(httpCli,(uint8_t const *)SL_HTTPCli_METHOD_GET,getReqUri,httpHeaders,numOfHeaders);
    if(0 < retVal)
    {
        retVal=SlHttpCliReadFields(httpCli,recvBuffer,recvBufferSize,&more_flags);
        if(0 < retVal)
        {
            retVal=SlParsHttp(recvBuffer,(uint32_t)retVal,0u,&httpParser);
            if( 0 > retVal )
            {
                
                WiFiDebugPrint("Pico HTTP Parser ERROR in (PostDataToINet)WIFI ERROR CODE:%d\n",retVal);
                WiFiDebugFlush();
                
            }
            else if( (HTTPStd_CREATED == httpParser.status) || (HTTPStd_OK == httpParser.status) || (HTTPStd_NO_CONTENT == httpParser.status))
            {
                
                WiFiDebugPrint("WIFI HTTP Status Code:%d\n",httpParser.status);
                WiFiDebugFlush();
                
                if(TRANSFER_CONTENT_LENGTH == httpParser.transfer_encoding)
                {
                    do
                    {
                        bytesToRead=( (httpParser.content_length >recvBufferSize) ? recvBufferSize:httpParser.content_length );
                        retVal = SlHttpCliReadRawResponseBody(httpCli,recvBuffer,bytesToRead);
                        if(0 < retVal)
                        {
                            bytesToRead = (uint16_t)retVal;
                            httpParser.content_length -= bytesToRead;
                            
                            
                            if ((uint32_t)retVal < recvBufferSize)
                            {
                                // Append NULL At The End of Response
                                recvBuffer[bytesToRead]= (uint8_t)NULL;
                                ++bytesToRead;
                            }
                            
                            WiFiDebugPrint("WIFI Response ID%.*s\r\n",retVal,recvBuffer);
                            WiFiDebugFlush();
                            
                            
                        }
                        else
                        {
                            
                            WiFiDebugPrint("Failed To Get Response Body in (PostDataToINet)WIFI ERROR CODE:%d\n",retVal);
                            WiFiDebugFlush();
                            
                        }
                    }
                    while( (httpParser.content_length) && (0 < retVal) );
                }
                else if(CHUNKED==httpParser.transfer_encoding)
                {                  
                  do{
                    
                      retVal = SlHttpCligetChunkedData(httpCli,&recvBuffer[ChunkedBytesRead],recvBufferSize-ChunkedBytesRead,&more_flags);
                      if(0 < retVal)
                      {
                        ChunkedBytesRead += retVal;
                      }
                    
                    }while(true == more_flags && 0 < retVal);
                  
                  if(0 < ChunkedBytesRead && (0 == retVal) && (false == more_flags) )
                  {
                    retVal = ChunkedBytesRead;
                    recvBuffer[ChunkedBytesRead] = 0x00;
                    WiFiDebugPrint("WIFI Acess Token:\n%.*s\n",ChunkedBytesRead,slWifiRecvResponseBuffer);
                    WiFiDebugFlush();
                    //SlHttpCligetChunkedData(http_cli,&recvBuffer[retVal],recvBufferSize-(uint32_t)retVal,&more_flags);
                  }
                }
                else
                {
                    
                    WiFiDebugPrint("Wifi HTTP module get unknown Transfer Encoding in (PostDataToINet)\n");
                    WiFiDebugFlush();
                    
                }
            }
            else if( HTTPStd_UNAUTHORIZED == httpParser.status )
            {
                isTokenValid = false;
                WiFiDebugPrint("Wifi Status Code:%d\nWifi HTTP Message:%.*s\n",httpParser.status,httpParser.msg_len,httpParser.msg);
                WiFiDebugFlush();
                retVal = -1;
            }
            else if( (HTTPStd_NON_AUTHORITATIVE_INFORMATION < httpParser.status) && (HTTPStd_STATUS_CODE_END < httpParser.status) )
            { 
                WiFiDebugPrint("Wifi Status Code:%d\nWifi HTTP Message:%.*s\n",httpParser.status,httpParser.msg_len,httpParser.msg);
                WiFiDebugFlush();
                WiFiDebugPrint("Event Will Be Discarded\n");
                WiFiDebugFlush();
                retVal= httpParser.status;
            }
            else
            {
                retVal= -1;
            }
        }
    }
    return retVal;
}

//------------------------------------------------------------------------------
//  static int32_t GetAccessToken(slHTTPCliStruct_t *http_cli,char recvBuffer[],uint32_t recvBufferSize)
//
//   Author:  Ammar Younas
//   Date:    2017/04/5
//   Get Access Token from INet Server
//
//------------------------------------------------------------------------------

static int32_t GetAccessToken(slHTTPCliStruct_t *http_cli,uint8_t recvBuffer[],uint16_t recvBufferSize)
{

    uint8_t tokenAccessURI[30];
    uint8_t tokenReqCredentials[256]={0};
    uint32_t tokenReqCredentialsSize = 0;
    uint8_t tokenReqCredentialsSizeStr[SL_INT_TO_STRING_BUFFER_SIZE];
    memset(tokenReqCredentialsSizeStr, 0u, SL_INT_TO_STRING_BUFFER_SIZE);
    
    jsonCreatorAndParser[GET_INET_TOKEN].jCreator(tokenAccessURI, 30, tokenReqCredentials, 256, NULL);
    tokenReqCredentialsSize = strlen((char const *)tokenReqCredentials);
    snprintf((char *)tokenReqCredentialsSizeStr, SL_INT_TO_STRING_BUFFER_SIZE,"%d",tokenReqCredentialsSize);
    
    slHTTPCliFieldStruct_t  httpHeaders[SL_HTTP_NUM_HEADERS_FOR_ACCESS_TOKEN]=
    {{SL_HTTPCli_FIELD_NAME_HOST,CLOUD_URL},
    {SL_HTTPCli_FIELD_NAME_USER_AGENT,SL_USER_AGENT},
    {SL_HTTPCli_FIELD_NAME_CONTENT_TYPE,SL_CONTENT_TYPE_URL_ENCODED},
    {SL_HTTPCli_FIELD_NAME_ACCEPT,"*/*"},
    {SL_HTTPCli_FIELD_NAME_CONTENT_LENGTH,tokenReqCredentialsSizeStr},
    {SL_HTTPCli_FIELD_NAME_CONNECTION,SL_CONNECTION_TYPE},
    {NULL,NULL}};
    
    uint32_t numOfHeaders=(sizeof(httpHeaders)/sizeof(httpHeaders[0]));
    uint32_t  bytesToRead = 0;
    uint32_t ChunkedBytesRead = 0;
    slHTTPParse_t httpParser;
    BOOLEAN more_flags=0;
    int32_t retVal=-1;
    
    if(false == isTokenValid)
    {
        retVal=SlHTTPCliSndRequest(http_cli,(uint8_t const *)SL_HTTPCli_METHOD_POST,tokenAccessURI,httpHeaders,numOfHeaders);
        if(0 < retVal)
        {
            retVal=SlHTTPSendBody(http_cli,tokenReqCredentials,tokenReqCredentialsSize);
            //WiFiDebugPrint("%.*s\r\n",tokenReqCredentialsSize,tokenReqCredentials);
            //WiFiDebugFlush();
            if(0 < retVal)
            {
                retVal=SlHttpCliReadFields(http_cli,recvBuffer,recvBufferSize,&more_flags);
                //WiFiDebugPrint("%.*s\r\n",recvBufferSize,recvBuffer);
                //WiFiDebugFlush();
                if(0 < retVal)
                {
                    retVal=SlParsHttp(recvBuffer,(uint32_t)retVal,0u,&httpParser);
                    //In case of Content Length Encoding
                    if((HTTPStd_OK == httpParser.status) && (0 < retVal) && (TRANSFER_CONTENT_LENGTH == httpParser.transfer_encoding))
                    {
                      do
                      {
                        bytesToRead = ( (httpParser.content_length >recvBufferSize) ? recvBufferSize:httpParser.content_length );
                        retVal = SlHttpCliReadRawResponseBody(http_cli,recvBuffer,bytesToRead);
                        if(0 < retVal)
                        {
                          bytesToRead = (uint16_t)retVal;
                          httpParser.content_length -= bytesToRead;
                          
                          
                          if ((uint32_t)retVal < recvBufferSize)
                          {
                            // Append NULL At The End of Response
                            recvBuffer[bytesToRead]= (uint8_t)NULL;
                            ++bytesToRead;
                          }
                          
                          WiFiDebugPrint("WIFI Response ID%.*s\r\n",retVal,recvBuffer);
                          WiFiDebugFlush();
                          
                          
                        }
                        else
                        {
                          
                          WiFiDebugPrint("Failed To Get Response Body in (PostDataToINet)WIFI ERROR CODE:%d\n",retVal);
                          WiFiDebugFlush();
                          
                        }
                      }
                      while( (httpParser.content_length) && (0 < retVal) );
                      
                      WiFiDebugPrint("WIFI Acess Token:\n%.*s\n",retVal,slWifiRecvResponseBuffer);
                      WiFiDebugFlush();
                      
                      if(0 < retVal)
                      {
                        // retVal = ExtCommunicationJParseGetToken(recvBuffer,(size_t)retVal,token,sizeof(token),&isTokenValid);
                        retVal = jsonCreatorAndParser[GET_INET_TOKEN].jParser(recvBuffer, (uint32_t)retVal, token, (uint32_t)sizeof(token), NULL);
                        if(retVal > 0)
                        {
                          isTokenValid = true;
                        }
                      }
                      
                    }
                    //In case of Content Chunked Encoding
                 else if( (HTTPStd_OK == httpParser.status) && (0 < retVal)  && (CHUNKED == httpParser.transfer_encoding))
                    {
                        do{
                          
                          retVal = SlHttpCligetChunkedData(http_cli,&recvBuffer[ChunkedBytesRead],recvBufferSize-ChunkedBytesRead,&more_flags);
                          if(0 < retVal)
                          {
                            ChunkedBytesRead += retVal;
                          }
                          
                        }while(true == more_flags && 0 < retVal);
                        
                        if(0 < ChunkedBytesRead && (0 == retVal) && (false == more_flags) )
                        {
                          retVal = ChunkedBytesRead;
                          recvBuffer[ChunkedBytesRead] = 0x00;
                          WiFiDebugPrint("WIFI Acess Token:\n%.*s\n",ChunkedBytesRead,slWifiRecvResponseBuffer);
                          WiFiDebugFlush();
                          //SlHttpCligetChunkedData(http_cli,&recvBuffer[retVal],recvBufferSize-(uint32_t)retVal,&more_flags);
                        }
                        else
                        {
                          WiFiDebugPrint("WIFI Acess Token failure: %d\n",retVal);
                          WiFiDebugFlush();
                        }
                        
                        if(0 < retVal)
                        {
                            // retVal = ExtCommunicationJParseGetToken(recvBuffer,(size_t)retVal,token,sizeof(token),&isTokenValid);
                            retVal = jsonCreatorAndParser[GET_INET_TOKEN].jParser(recvBuffer, (uint32_t)retVal, token, (uint32_t)sizeof(token), NULL);
                            if(retVal > 0)
                            {
                                isTokenValid = true;
                            }
                        }
                    }
                    else if( ( HTTPStd_OK !=httpParser.status ) && ( 0 < retVal ) )
                    {
                        if ((CHUNKED != httpParser.transfer_encoding) && (TRANSFER_CONTENT_LENGTH != httpParser.transfer_encoding))
                        {
                          
                          WiFiDebugPrint("\r\n**Encoding not supported in GetAccessToken() API**\r\n");
                          WiFiDebugFlush();
                          
                        }
                        
                        WiFiDebugPrint("\nToken Request to Server Failed :- \n");
                        WiFiDebugPrint("SERVER RESPONCE:%d\n",httpParser.status);
                        WiFiDebugFlush();
                        
                        retVal *= ((-1) * httpParser.status);
                    }
                    else
                    {
                        
                        WiFiDebugPrint("Parser Error\n");
                        WiFiDebugFlush();
                        
                        retVal = -1;
                    }
                }
            }
        }
    }
    else
    {
        retVal=1;
    }
    return retVal;
}


//------------------------------------------------------------------------------
//  static int SlParsHttp(void * responce_buffer,size_t len,size_t pre_length,slHTTPParse_t * http_parser)
//   Author:  Ammar Younas
//   Date:    2017/04/5
//   Parse HTTP responce using Pico HTTP parser
//
//------------------------------------------------------------------------------
static int32_t SlParsHttp(void * responce_buffer,size_t len,size_t pre_length,slHTTPParse_t * http_parser)
{

    uint8_t transfer_encoding[] ="Transfer-Encoding";
    uint8_t content_len[] = "Content-Length";
    uint8_t connection[] = "Connection";
    uint8_t keepalive[] = "keep-alive";

    size_t numOfHeaders=sizeof(http_parser->headers)/sizeof(http_parser->headers[0]);

    int32_t pret=0;
    pret=phr_parse_response((const char* )responce_buffer,len,&http_parser->minor_version,&http_parser->status,(char const **)&http_parser->msg
                            ,&http_parser->msg_len,http_parser->headers,&numOfHeaders,pre_length);

    if ( 0 <  pret)
    {
        for (uint32_t i = 0u; i != numOfHeaders; ++i) 
    {
            if(0 == StringCompareByLen(transfer_encoding,(const uint8_t *)http_parser->headers[i].name,(sizeof(transfer_encoding) - 1u) ) )
        {
            http_parser->transfer_encoding=CHUNKED;
                
        }
            else if(0 == StringCompareByLen(content_len,(const uint8_t *)http_parser->headers[i].name, (sizeof(content_len) - 1u) ) )
        {
            http_parser->transfer_encoding = TRANSFER_CONTENT_LENGTH;
            http_parser->content_length=strtoul(http_parser->headers[i].value,NULL,10);
        }
            else if(0 == StringCompareByLen(connection,(const uint8_t *)http_parser->headers[i].name,(sizeof(connection) - 1u) ) )
        {
            if(0 == StringCompareByLen(keepalive,(const uint8_t *)http_parser->headers[i].value,(sizeof(keepalive)- 1u) ) )
            {
                http_parser->connection_type=KEEP_ALIVE;
            }
            else
            {
                http_parser->connection_type=CLOSE;
            }
        }
        else
        {
            // Control will never be here
        }
    }
    }
    else
    {
        
        WiFiDebugPrint("\n ParseError \n ");
        WiFiDebugFlush();
        
    }
    return pret;

}
BOOLEAN WifiGetCommErrorStatus(void)
{
  return wifiComError;
}

//==============================================================================
//
//  void UpdateConnectionStaus(void)
//
//   Author:   Tayyab Tahir
//   Date:     2019/01/10
//
//!  This function updates the Instrument connection staus to wifi.
//
//==============================================================================
void UpdateConnectionStaus(void)
{
  //Update the Inet statuses of the Cornell at every connection test
  if( WIFI_CONNECTED == GetWifiState())
  {
    //Set the battery status to be connected via wifi
    InstrumentInfo.SmartBatteryStatus = RUNNING_NORMAL;
  }
  else
  {
    //Set the battery status to be not connected via wifi
    InstrumentInfo.SmartBatteryStatus = NO_CONNECTION_AVALIBLE;
  }
  
  if( (WIFI_CONNECTED == GetWifiState()) && (FALSE == wifiComError) )
  {
    //Inet is accepting the instrument 
    InstrumentInfo.InetStatus = INET_ACCEPTS_INSTRUMENT_AND_MONITORING;
  }
  else
  {
    //Inet is not accepting the instrument 
    InstrumentInfo.InetStatus = INET_NOT_MONITORING_INSTRUMENT;
  }
}

void UpdateHTTPHostInfo(CloudInfo_t *cloudInfo)
{
    memcpy(&httpCloudInfo,cloudInfo,sizeof(CloudInfo_t));
}