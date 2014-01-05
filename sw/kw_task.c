#include "stm32f10x.h"
#include "stdio.h"
#include "string.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

#include "kw1281.h"
#include "kw_task.h"
#include "usart.h"

xQueueHandle xKW1281InputQueue;
xQueueHandle xKW1281OutputQueue;
KWState_t KWState = KW_IDLE;
KWState_t KWRequest = KW_IDLE;

void KWStart()
{
  KWState = KW_IDLE;
  KWRequest = KW_START_REQ;
}


void KWDisconnect()
{
  KWRequest = KW_DISCONNECT_REQ;
}


KWState_t KWGetStatus()
{
  return KWState;
}


void KWTask(void *pvParameters)
{
  KW1281struct_t frame;
  uint8_t KW1281data[128];
  uint8_t *KW1281data_temp;
  uint8_t result = 0x00;
  portBASE_TYPE xStatus;
  portTickType xWaitTime = 50 / portTICK_RATE_MS;

  UartPrint("KWTask(): started\n");

  while (1)
  {
    while (KWRequest != KW_START_REQ)
    {
      vTaskDelay(10 / portTICK_RATE_MS);
    }

    KWRequest = KW_NO_REQ;
    KWState = KW_WORK;
    
    do
    {
      frame.title = 0xff;
      frame.data = KW1281data;
    
      // odebranie ramki od ECU
      result = KW1281ReceiveBlock(&frame);
      if (result) 
      {
        KWState = KW_ERROR;
        break;
      }
      KW1281BlockCounter = frame.cnt;

      if ((KW1281_ACK != frame.title)/* && (KW1281_NO_ACK != frame.title)*/)
      {
        if (frame.len > 3)
        {
          KW1281data_temp = pvPortMalloc(frame.len-3);
          memmove(KW1281data_temp, frame.data, frame.len-3);
          frame.data = KW1281data_temp;
        }
        else
          frame.data = NULL;

        xStatus = xQueueSendToBack(xKW1281OutputQueue, &frame, 0);
      }
     
      // transmission TESTER -> ECU
      vTaskDelay(kw1281_intermessage_delay / portTICK_RATE_MS); //opoznienie przed wyslaniem nowego komunikatu

      if (KWRequest == KW_DISCONNECT_REQ)
      {
        frame.len = 3;
        frame.title = KW1281_END_OF_SESSION;
        KW1281BlockCounter++; // inkrementacja lokalnie przechowywanej wartosci Block Counter
        frame.cnt = KW1281BlockCounter;
        frame.data = KW1281data;
      
        result = KW1281SendBlock(&frame); //wyslanie bloku do ECU          
        if (result) 
        {
          KWState = KW_ERROR;
          break;
        }
        KWRequest = KW_NO_REQ;
        KWState = KW_DISCONNECTED;
      }
      else
      {
        xStatus = xQueueReceive(xKW1281InputQueue, &frame, xWaitTime);
        if (xStatus == pdTRUE)
        {
          KW1281BlockCounter++; // inkrementacja lokalnie przechowywanej wartosci Block Counter
          frame.cnt = KW1281BlockCounter;
      
          result = KW1281SendBlock(&frame); //wyslanie bloku do ECU          
          vPortFree(frame.data);
          if (result) 
          {
            KWState = KW_ERROR;
            break;
          }
        }
        else 
        {
          result = KW1281SendACK();
          if (result) 
          {
            KWState = KW_ERROR;
            break;
          }
        }
      }
    } while (KWState == KW_WORK);
  } // while (1)
}


void KWTaskInit(uint8_t prio)
{
  xKW1281InputQueue = xQueueCreate( 5, sizeof(KW1281struct_t) );
  xKW1281OutputQueue = xQueueCreate( 5, sizeof(KW1281struct_t) );

  xTaskCreate( KWTask, ( signed char * ) "kwtask", configMINIMAL_STACK_SIZE+512, NULL, prio, NULL );
}

