#ifndef __SLHTTTPCLI_H__
#define __SLHTTTPCLI_H__

#include <string.h>
#include <stdbool.h>
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/net/wifi/device.h>
#include "picohttpparser.h"
#include <SysDef.h>
#include "WifiTask.h"
#include "SPI_Comm.h"
#include "ExtCommunication.h"

#define MAXIMUM_NUM_OF_REQ_HEADERS  30
#define SL_STATUS_BUFLEN            16

#define SL_HTTPCli_ESOCKETFAIL        (-101)

/*!
 *  @brief Connect failed
 */
#define SL_HTTPCli_ECONNECTFAIL       (-102)

/*!
 *  @brief Send failed
 */
#define SL_HTTPCli_ESENDFAIL          (-103)

/*!
 *  @brief Recieve failed
 */
#define SL_HTTPCli_ERECVFAIL          (-104)

/*!
 *  @brief TLS create failed
 */
#define SL_HTTPCli_ETLSFAIL           (-105)

/*!
 *  @brief Cannot get IP address from host name
 */
#define SL_HTTPCli_EHOSTNAME          (-106)

/*!
 *  @brief Send buffer is not big enough
 */
#define SL_HTTPCli_ESENDBUFSMALL      (-107)

/*!
 *  @brief Recieve buffer is not big enough
 */
#define SL_HTTPCli_ERECVBUFSMALL      (-108)

/*!
 *  @brief Cannot call getResponseStatus in async mode
 */
#define SL_HTTPCli_EASYNCMODE         (-109)

/*!
 *  @brief Thread create failed
 */
#define SL_HTTPCli_ETHREADFAIL        (-110)

/*!
 *  @brief Cannot create tunnel through proxy
 */
#define SL_HTTPCli_EPROXYTUNNELFAIL   (-111)

/*!
 *  @brief Response recieved is not HTTP/1.1 response
 */
#define SL_HTTPCli_ERESPONSEINVALID   (-112)

/*!
 *  @brief Content length returned is too large
 */
#define SL_HTTPCli_ECONTENTLENLARGE   (-114)

/*!
 *  @brief Redirection URI returned is too long to be read into buffer
 */
#define SL_HTTPCli_EREDIRECTURILONG   (-115)

/*!
 *  @brief Content type returned is too long to be read into buffer
 */
#define SL_HTTPCli_ECONTENTTYPELONG   (-116)

/*!
 *  @brief Recieved content type does not match any registered callback
 */
#define SL_HTTPCli_ENOCONTENTCALLBACK (-117)

/*!
 *  @brief Encoded data is not of chunked type
 */
#define SL_HTTPCli_ENOTCHUNKDATA      (-118)

/*!
 *  @brief Operation could not be completed. Try again.
 */
#define SL_HTTPCli_EINPROGRESS        (-119)

/*!
 *  @brief Internal instance buffer to send/recieve data is too small.
 */
#define SL_HTTPCli_EINTERNALBUFSMALL  (-120)

/*!
 *  @brief Could not setup the notify callbacks
 */
#define SL_HTTPCli_ESETNOTIFYFAIL     (-121)

/*!
 *  @brief Input URI length is too long to read into buffer.
 */
#define SL_HTTPCli_EURILENLONG  (-120)

enum HTTPStd_STATUS_CODE {
    HTTPStd_CONTINUE                          = 100, /*!< Informational */
    HTTPStd_SWITCHING_PROTOCOLS               = 101, /*!< Informational */

    HTTPStd_OK                                = 200, /*!< Success */
    HTTPStd_CREATED                           = 201, /*!< Success */
    HTTPStd_ACCEPTED                          = 202, /*!< Success */
    HTTPStd_NON_AUTHORITATIVE_INFORMATION     = 203, /*!< Success */
    HTTPStd_NO_CONTENT                        = 204, /*!< Success */
    HTTPStd_RESET_CONTENT                     = 205, /*!< Success */
    HTTPStd_PARTIAL_CONTENT                   = 206, /*!< Success */
    HTTPStd_MULTI_STATUS                      = 207, /*!< Success */

    HTTPStd_MULTIPLE_CHOICES                  = 300, /*!< Redirection */
    HTTPStd_MOVED_PERMANENTLY                 = 301, /*!< Redirection */
    HTTPStd_FOUND                             = 302, /*!< Redirection */
    HTTPStd_SEE_OTHER                         = 303, /*!< Redirection */
    HTTPStd_NOT_MODIFIED                      = 304, /*!< Redirection */
    HTTPStd_USE_PROXY                         = 305, /*!< Redirection */
    HTTPStd_TEMPORARY_REDIRECT                = 307, /*!< Redirection */

    HTTPStd_BAD_REQUEST                       = 400, /*!< Client Error */
    HTTPStd_UNAUTHORIZED                      = 401, /*!< Client Error */
    HTTPStd_PAYMENT_REQUIRED                  = 402, /*!< Client Error */
    HTTPStd_FORBIDDEN                         = 403, /*!< Client Error */
    HTTPStd_NOT_FOUND                         = 404, /*!< Client Error */
    HTTPStd_METHOD_NOT_ALLOWED                = 405, /*!< Client Error */
    HTTPStd_NOT_ACCEPTABLE                    = 406, /*!< Client Error */
    HTTPStd_PROXY_AUTHENTICATION_REQUIRED     = 407, /*!< Client Error */
    HTTPStd_REQUEST_TIMEOUT                   = 408, /*!< Client Error */
    HTTPStd_CONFLICT                          = 409, /*!< Client Error */
    HTTPStd_GONE                              = 410, /*!< Client Error */
    HTTPStd_LENGTH_REQUIRED                   = 411, /*!< Client Error */
    HTTPStd_PRECONDITION_FAILED               = 412, /*!< Client Error */
    HTTPStd_REQUEST_ENTITY_TOO_LARGE          = 413, /*!< Client Error */
    HTTPStd_REQUEST_URI_TOO_LONG              = 414, /*!< Client Error */
    HTTPStd_UNSUPPORTED_MEDIA_TYPE            = 415, /*!< Client Error */
    HTTPStd_REQUESTED_RANGE_NOT_SATISFAIABLE  = 416, /*!< Client Error */
    HTTPStd_EXPECTATION_FAILED                = 417, /*!< Client Error */
    HTTPStd_UNPROCESSABLE_ENTITY              = 422, /*!< Client Error */
    HTTPStd_TOO_MANY_REQUESTS                 = 429, /*!< Client Error */

    HTTPStd_INTERNAL_SERVER_ERROR             = 500, /*!< Server Error */
    HTTPStd_NOT_IMPLEMENTED                   = 501, /*!< Server Error */
    HTTPStd_BAD_GATEWAY                       = 502, /*!< Server Error */
    HTTPStd_SERVICE_UNAVAILABLE               = 503, /*!< Server Error */
    HTTPStd_GATEWAY_TIMEOUT                   = 504, /*!< Server Error */
    HTTPStd_HTTP_VERSION_NOT_SUPPORTED        = 505, /*!< Server Error */

    HTTPStd_STATUS_CODE_END                   = 600
};


/* HTTP methods */
#define SL_HTTPCli_METHOD_GET      "GET"
#define SL_HTTPCli_METHOD_POST     "POST"
#define SL_HTTPCli_METHOD_HEAD     "HEAD"
#define SL_HTTPCli_METHOD_OPTIONS  "OPTIONS"
#define SL_HTTPCli_METHOD_PUT      "PUT"
#define SL_HTTPCli_METHOD_DELETE   "DELETE"
#define SL_HTTPCli_METHOD_CONNECT  "CONNECT"

/* HTTP Request Field Name */
#define SL_HTTPCli_FIELD_NAME_ACCEPT  "Accept"
#define SL_HTTPCli_FIELD_NAME_ACCEPT_CHARSET  "Accept-Charset"
#define SL_HTTPCli_FIELD_NAME_ACCEPT_ENCODING  "Accept-Encoding"
#define SL_HTTPCli_FIELD_NAME_ACCEPT_LANGUAGE  "Accept-Language"
#define SL_HTTPCli_FIELD_NAME_ACCEPT_RANGES "Accept-Ranges"
#define SL_HTTPCli_FIELD_NAME_AGE "Age"
#define SL_HTTPCli_FIELD_NAME_ALLOW  "Allow"
#define SL_HTTPCli_FIELD_NAME_AUTHORIZATION  "Authorization"
#define SL_HTTPCli_FIELD_NAME_CACHE_CONTROL  "Cache-Control"
#define SL_HTTPCli_FIELD_NAME_CONNECTION  "Connection"
#define SL_HTTPCli_FIELD_NAME_CONTENT_ENCODING  "Content-Encoding"
#define SL_HTTPCli_FIELD_NAME_CONTENT_LANGUAGE  "Content-Language"
#define SL_HTTPCli_FIELD_NAME_CONTENT_LENGTH  "Content-Length"
#define SL_HTTPCli_FIELD_NAME_CONTENT_LOCATION  "Content-Location"
#define SL_HTTPCli_FIELD_NAME_CONTENT_MD5  "Content-MD5"
#define SL_HTTPCli_FIELD_NAME_CONTENT_RANGE  "Content-Range"
#define SL_HTTPCli_FIELD_NAME_CONTENT_TYPE  "Content-Type"
#define SL_HTTPCli_FIELD_NAME_COOKIE  "Cookie"
#define SL_HTTPCli_FIELD_NAME_DATE  "Date"
#define SL_HTTPCli_FIELD_NAME_ETAG "ETag"
#define SL_HTTPCli_FIELD_NAME_EXPECT  "Expect"
#define SL_HTTPCli_FIELD_NAME_EXPIRES  "Expires"
#define SL_HTTPCli_FIELD_NAME_FROM  "From"
#define SL_HTTPCli_FIELD_NAME_HOST  "Host"
#define SL_HTTPCli_FIELD_NAME_IF_MATCH  "If-Match"
#define SL_HTTPCli_FIELD_NAME_IF_MODIFIED_SINCE  "If-Modified-Since"
#define SL_HTTPCli_FIELD_NAME_IF_NONE_MATCH  "If-None-Match"
#define SL_HTTPCli_FIELD_NAME_IF_RANGE  "If-Range"
#define SL_HTTPCli_FIELD_NAME_IF_UNMODIFIED_SINCE  "If-Unmodified-Since"
#define SL_HTTPCli_FIELD_NAME_KEEP_ALIVE           "Keep-Alive"
#define SL_HTTPCli_FIELD_NAME_LAST_MODIFIED  "Last-Modified"
#define SL_HTTPCli_FIELD_NAME_LOCATION  "Location"
#define SL_HTTPCli_FIELD_NAME_MAX_FORWARDS  "Max-Forwards"
#define SL_HTTPCli_FIELD_NAME_ORIGIN  "Origin"
#define SL_HTTPCli_FIELD_NAME_PRAGMA  "Pragma"
#define SL_HTTPCli_FIELD_NAME_PROXY_AUTHENTICATE "Proxy-Authenticate"
#define SL_HTTPCli_FIELD_NAME_PROXY_AUTHORIZATION  "Proxy-Authorization"
#define SL_HTTPCli_FIELD_NAME_RANGE  "Range"
#define SL_HTTPCli_FIELD_NAME_REFERER  "Referer"
#define SL_HTTPCli_FIELD_NAME_RETRY_AFTER "Retry-After"
#define SL_HTTPCli_FIELD_NAME_SERVER "Server"
#define SL_HTTPCli_FIELD_NAME_TE  "TE"
#define SL_HTTPCli_FIELD_NAME_TRAILER  "Trailer"
#define SL_HTTPCli_FIELD_NAME_TRANSFER_ENCODING  "Transfer-Encoding"
#define SL_HTTPCli_FIELD_NAME_UPGRADE  "Upgrade"
#define SL_HTTPCli_FIELD_NAME_USER_AGENT  "User-Agent"
#define SL_HTTPCli_FIELD_NAME_VARY "Vary"
#define SL_HTTPCli_FIELD_NAME_VIA  "Via"
#define SL_HTTPCli_FIELD_NAME_WWW_AUTHENTICATE "WWW-Authenticate"
#define SL_HTTPCli_FIELD_NAME_WARNING  "Warning"
#define SL_HTTPCli_FIELD_NAME_X_FORWARDED_FOR  "X-Forwarded-For"
#define SL_HTTPCli_FIELD_NAME_X_ACCOUNT_ID "X-AccountId"

/* HTTP client getResponseField() special return codes */
#define vSL_HTTP_SND_FAILED                     (-1)
#define vSL_HTTP_SUCCESS                        (0)

typedef struct slHTTPCliFieldStruct
{
    const uint8_t *name;      /*!< Field name,  ex: HTTPCli_FIELD_NAME_ACCEPT */
    const uint8_t *value;     /*!< Field value, ex: "text/plain" */
}slHTTPCliFieldStruct_t;

typedef struct slHTTPCliStruct
{
    uint8_t *host;
    uint16_t host_length;
    uint16_t clen;
    int16_t sock_fd;
    SlSockAddrIn_t host_addr;
    const uint8_t * certificatePath;
    uint16_t CertificatePathLength;
    uint8_t sec_method;
    uint32_t sec_cipher;

}slHTTPCliStruct_t;
  

int16_t SlHTTPCliSetSocketOpt(slHTTPCliStruct_t * http_cli);
int16_t SlHTTPCliSndRequest(slHTTPCliStruct_t * http_cli,const uint8_t method[],const uint8_t req_uri[],slHTTPCliFieldStruct_t http_headers[],uint32_t num_of_headers);
int16_t SlHTTPSendBody(slHTTPCliStruct_t * http_cli,const uint8_t body[],uint16_t len);
int32_t SlHttpCligetChunkedData(slHTTPCliStruct_t * cli,uint8_t body[],uint16_t len,BOOLEAN *moreFlag);
int16_t StringCompareByLen(const uint8_t str1[],const uint8_t str2[], uint16_t len);
int16_t SlHttpCliReadFields(slHTTPCliStruct_t * cli,uint8_t buffer[],uint16_t len, BOOLEAN *more_flags);
int16_t SlHttpCliReadRawResponseBody(slHTTPCliStruct_t * cli,uint8_t buffer[],uint16_t len);

void SlHTTPCliDisconnect(slHTTPCliStruct_t * http_cli);

#endif /*__SLHTTTPCLI_H_ */