#include <stdint.h>

#include "Board.h"
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include "ti/drivers/UART.h"
#include "ti/drivers/uart/UARTCC32XXDMA.h"

#include <ti/devices/cc32xx/inc/hw_types.h>
#include <ti/devices/cc32xx/driverlib/prcm.h>

#include "SysDef.h"
#include "EventQ.h"
#include "SysTask.h"
#include "WifiTask.h"
#include "SPI_Comm.h"
#include "TimerTask.h"
#include "SPI_Comm.h"
#include "PowerManagement.h"

#include <xdc/std.h>
#include <ti/sysbios/BIOS.h>
#include <xdc/runtime/Types.h>
#include <xdc/runtime/System.h>

#include <xdc/runtime/System.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>

#define DEBUG_UART_BAUD_RATE                        921600
#define DEBUG_UART_READ_WRITE_TIME_OUT              1000

UART_Handle              DebugUartHandle;

uint32_t wakeUpCause;

void CreateNWPTask(void);

Types_FreqHz SystemFrequency;

void DebugDriverInit(void);

int main(void)
{
  //Initialize board support package 
  Board_initGeneral();
  //Get the reset cause from the last run
  wakeUpCause = PRCMHibernateWakeupCauseGet();
  //Initialize the I/O module
  GPIO_init();
  //Initializes the SPI module
  SPI_init();
  //Initializes the UART module
  UART_init();
  //Allocate Queue memory for dynamic allocation
  CreatMemmoryPoleForQueue();
  //Create Network Processor Task for periodic event
  CreateNWPTask(); 
  //Create System Task for periodic event
  SysTaskInit();
  // Configure RTC to use
  PRCMRTCInUseSet();
  //Initialize external watch dog configuration
  SysWatchdogInit();
  //Get system frequency
  BIOS_getCpuFreq(&SystemFrequency);
  //Disable power module to restrain it going to sleep
  Power_disablePolicy();
  
#ifdef REDIRECT_STDIO
  //Initialize the Debug UART Port
  DebugDriverInit();
#endif /* REDIRECT_STDIO */

  //Start the RTOS execution
  BIOS_start();
  
  return (0);
}

//==============================================================================
//
//   void DebugDriverInit(void)
//
//   Author:   Tayyab Tahir 
//   Date:     2018/07/26   
//
//!  This function Initializes the UART driver for Debug Log
//
//==============================================================================
void DebugDriverInit(void)
{ 
  UART_Params      debugUartParams;
  UART_Params_init(&debugUartParams);
  debugUartParams.baudRate  = DEBUG_UART_BAUD_RATE;
  debugUartParams.dataLength = UART_LEN_8;
  debugUartParams.parityType = UART_PAR_NONE;
  debugUartParams.stopBits = UART_STOP_ONE;
  debugUartParams.writeDataMode = UART_DATA_BINARY;
  debugUartParams.readDataMode = UART_DATA_BINARY;
  debugUartParams.readReturnMode = UART_RETURN_FULL;
  debugUartParams.readMode = UART_MODE_BLOCKING;
  debugUartParams.readEcho = UART_ECHO_OFF;
  debugUartParams.readTimeout = DEBUG_UART_READ_WRITE_TIME_OUT; 
  debugUartParams.writeTimeout = DEBUG_UART_READ_WRITE_TIME_OUT;
  DebugUartHandle = UART_open(Board_DEBUG_UART, &debugUartParams);
  //remove uart receive from LPDS dependency
  UART_control(DebugUartHandle, UART_CMD_RXDISABLE, NULL);
}
// Need to compile in Release Mode
#ifdef NDEBUG
size_t __write(int x, const unsigned char *y, size_t z)
{
    
#ifdef REDIRECT_STDIO
    UARTCC32XXDMA_Object *object = DebugUartHandle->object;
    if( (NULL != DebugUartHandle) && (object->writeSize == 0) )
    {
        UART_write(DebugUartHandle, y, z);
    }
#endif /* REDIRECT_STDIO */
    UART_control(DebugUartHandle, UART_CMD_RXDISABLE, NULL);
    return z;
}
#endif /* NDEBUG */

