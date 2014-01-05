#include "stm32f10x.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

xQueueHandle xKW1281InputQueue;
xQueueHandle xKW1281OutputQueue;

void KWThreadInit()
{
  xKW1281InputQueue = xQueueCreate( 5, sizeof(KW1281struct_t) );
  xKW1281OutputQueue = xQueueCreate( 5, sizeof(KW1281struct_t) );


}

typedef enum
{
  KW_IDLE,
  KW_START_REQ,
  KW_WORK,
  KW_STOP_REQ,
  KW_ERROR,
  KW_DISCONNECTED
} KWState_t;

KWState_t KWState = KW_IDLE;

void KWStart()
{
  KWState = KW_START_REQ;
}

void KWStop()
{
  KWState = KW_STOP_REQ;
}

void KWTask(void *pvParameters)
{

  while (1)
  {
    while (KWState != KW_START_REQ)
      taskYIELD();

    KWState = KW_WORK;
    while (KWState == KW_WORK)
    {


  KW1281struct_t frame;
  uint8_t KW1281data[128];
  uint8_t i;
  uint8_t disconnected=0;
  uint8_t result = 0x00;
  uint8_t unansweredframe;
  uint8_t groupNumber;
  uint8_t requestedGroup;
  uint8_t sendRequest = 0;
  uint8_t cnt = 0;
  uint8_t sendEOS = 0; //KW1281_END_OF_SESSION

  frame.data = KW1281data;


      frame.title=0xff;
      unansweredframe = 1;
    
      // odebranie ramki od ECU
      result = KW1281ReceiveBlock(&frame);
      if (result) 
      {
        //error
        disconnected=1;
        KWState = KW_ERROR;
        break;
      }

      if (sendEOS)
      {
        break;
      }

      if (KW1281_GROUP_RESP == frame.title)
      {
 
      }

      sendRequest = 1;
      unansweredframe = 0;
      }
      else if (KW1281_ACK == frame.title) // odebranie bloku ACK - mozna wyslac nowa komende 
      { 
        cnt++;
        if (cnt > 10)
        {
          sendRequest = 1;
        }
      }
    
      if (2 == buttonState)
        sendEOS = 1;
      
      // transmission TESTER -> ECU
      delay_ms(kw1281_intermessage_delay); //opoznienie przed wyslaniem nowego komunikatu = 100ms

      if (sendEOS)
      {
        frame.len = 3;
        frame.title = KW1281_END_OF_SESSION;
        KW1281BlockCounter++; // inkrementacja lokalnie przechowywanej wartosci Block Counter
        frame.cnt = KW1281BlockCounter;
      
        result = KW1281SendBlock(&frame); //wyslanie bloku do ECU          
        if (result) 
        {
          //trans_GROUP_REQ_block
          disconnected=1;
          break;
        }
      }
      else if (sendRequest)
      {
        frame.len = 4; //3
        frame.title = KW1281_GROUP_REQ; //KW1281_ERRORS_REQ
        KW1281BlockCounter++; // inkrementacja lokalnie przechowywanej wartosci Block Counter
        frame.cnt = KW1281BlockCounter;
        KW1281data[0] = groupNumber;
        requestedGroup = groupNumber;
        unansweredframe=0;
      
        result = KW1281SendBlock(&frame); //wyslanie bloku do ECU          
        if (result) 
        {
          //trans_GROUP_REQ_block
          disconnected=1;
          break;
        }
      }
      else if (unansweredframe) 
      {
        result = KW1281SendACK();
        unansweredframe=0;
        if (result) 
        {
          //error trans_ACK_block
          disconnected=1;
          break;
        }  
      }
    }
  }
}
