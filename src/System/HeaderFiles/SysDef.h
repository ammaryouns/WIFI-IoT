#ifndef __SYS_DEF_H
#define __SYS_DEF_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus*/
  
#include <stdint.h>
#include <stdbool.h>

#include <xdc/std.h>

#ifndef NDEBUG

//#include <xdc/runtime/System.h>

#endif /* NDEBUG */

#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>    
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/heaps/HeapBuf.h>
#include <ti/sysbios/heaps/HeapMultiBuf.h>
#include "Board.h"
  
#define MAX_INST_COMM_INTERVAL                  30u
    
//Cornell Firmware version
#define FIRMWARE_VERSION_MAJOR                  1u           //!< Firmware version major (in v1.2.3 - 1 is major)
#define FIRMWARE_VERSION_MINOR                  96u           //!< Firmware version minor (in v1.2.3 - 2 is minor)  
#define FIRMWARE_VERSION_BUILD                  19u           //!< Firmware version build (in v1.2.3 - 3 is build)
#define OS_TICKS_PER_SECOND                     1000  
  
#ifdef NDEBUG

#ifdef REDIRECT_STDIO

#define ENABLE_WATCHDOG_DEBUG_LOG               0
#define ENABLE_WIFI_DEBUG_LOG                   1
#define ENABLE_WIFI_SCAN_DEBUG_LOG              0
#define ENABLE_TIMER_TASK_DEBUG_LOG             1
#define ENABLE_SYS_DEBUG_LOG                    1
#define ENABLE_STARTUP_DEBUG_LOG                1
#define ENABLE_GPS_DEBUG_LOG                    0
#define ENABLE_EVENTS_DEBUG_LOG                 0
#define ENABLE_POWER_MANAGEMENT_LOG             0
#define ENABLE_FIRMWARE_DOWNLOAD_LOG            0
#define ENABLE_ADC_LOG                          0
#define ENABLE_MFG_TEST_DEBUG_LOG               0
    
#define SYSTEM_PRINT(Module, format, ...)       do \
                                                { \
                                                    if(Module)\
                                                    {\
                                                      System_printf(format, ##__VA_ARGS__); \
                                                    }\
                                                }while(0)
        
#define SYSTEM_FLUSH(Module)                    do \
                                                {\
                                                    if(Module)\
                                                    {\
                                                        System_flush(); \
                                                    }\
                                                }while(0)

#else /* REDIRECT_STDIO */

#define ENABLE_WATCHDOG_DEBUG_LOG               0                                                     
#define ENABLE_WIFI_DEBUG_LOG                   0
#define ENABLE_WIFI_SCAN_DEBUG_LOG              0
#define ENABLE_TIMER_TASK_DEBUG_LOG             0
#define ENABLE_SYS_DEBUG_LOG                    0
#define ENABLE_STARTUP_DEBUG_LOG                0
#define ENABLE_GPS_DEBUG_LOG                    0
#define ENABLE_EVENTS_DEBUG_LOG                 0
#define ENABLE_POWER_MANAGEMENT_LOG             0
#define ENABLE_ADC_LOG                          0
#define ENABLE_MFG_TEST_DEBUG_LOG               0
    
#define SYSTEM_PRINT(Module, format, ...)       ((void)0)
#define SYSTEM_FLUSH(Module)                    ((void)0)

#endif /* REDIRECT_STDIO */
    
#else /* NDEBUG */

#define ENABLE_WATCHDOG_DEBUG_LOG               0 
#define ENABLE_WIFI_DEBUG_LOG                   1
#define ENABLE_WIFI_SCAN_DEBUG_LOG              0                                                    
#define ENABLE_TIMER_TASK_DEBUG_LOG             1
#define ENABLE_STARTUP_DEBUG_LOG                1
#define ENABLE_SYS_DEBUG_LOG                    0
#define ENABLE_GPS_DEBUG_LOG                    0
#define ENABLE_EVENTS_DEBUG_LOG                 0
#define ENABLE_POWER_MANAGEMENT_LOG             0
#define ENABLE_FIRMWARE_DOWNLOAD_LOG            0
#define ENABLE_ADC_LOG                          0
#define ENABLE_MFG_TEST_DEBUG_LOG               0
    
#define SYSTEM_PRINT(Module, format, ...)       do \
                                                { \
                                                    if(Module)\
                                                    {\
                                                      System_printf(format, ##__VA_ARGS__); \
                                                     }\
                                                 } while(0)

#define SYSTEM_FLUSH(Module)                    do \
                                                {\
                                                     if(Module)\
                                                     {\
                                                         System_flush(); \
                                                     }\
                                                 }while(0)
#endif /* NDEBUG */
                                                         

typedef    bool         BOOLEAN;
typedef    float        (float32_t);
typedef    double       (float64_t);
typedef    long double  (float128_t);

typedef enum
{
    MSG_SENDER_SYS_TASK=0,
    MSG_SENDER_WIFI,
    MSG_SENDER_NWP,
    MSG_SENDER_IRQ,
    MSG_SENDER_TIMER
}MSG_SENDR_t;

typedef struct
{
    MSG_SENDR_t sender;
}MsjSenderInfo_t;

#ifdef __cplusplus
}
#endif /* __cplusplus*/

#endif /* __SYSDEF_H */