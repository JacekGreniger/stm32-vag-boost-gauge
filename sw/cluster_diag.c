#include "stm32f10x.h"
#include "stdio.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

#include "filesystem/integer.h"
#include "filesystem/diskio.h"
#include "filesystem/ff.h"


#include "lcd.h"
#include "kw1281.h"
#include "cluster_diag.h"
#include "usart.h"
#include "kw_task.h"
#include "io_thread.h"


void clusterDiag_ProcessGroup3(u8 * frameData)
{
  uint8_t st[8];
  //5 a*(b-100)*0.1 oder 0.1*a*b - 10*a
  int temp;
  temp = frameData[1] * frameData[2] / 10;
  temp = temp - 10 * frameData[1];
  if (temp < 0)
  {
    if (temp < -99)
      temp = -99;
    temp *= -1;
    sprintf(st, "-%02d", temp);
  }
  else
    sprintf(st, "%3d", temp);
  LCDPrintString12x16(82,1, st);

  //5 a*(b-100)*0.1 oder 0.1*a*b - 10*a
  temp = frameData[7] * frameData[8] / 10;
  temp = temp - 10 * frameData[7];
  if (temp < 0)
  {
    if (temp < -99)
      temp = -99;
    temp *= -1;
    sprintf(st, "-%02d", temp);
  }
  else
    sprintf(st, "%3d", temp);
  LCDPrintString12x16(82,5, st);
}

void clusterDiag_ShowScreen()
{
  LCDClear();
  LCDPrintString12x16(10,1, "Water   0");
  LCDPrintString12x16(10,5, "Oil     0");
}

uint8_t clusterDiag_kw() //0 - disconnected by user
{
  KW1281struct_t frame;
  u8 groupNumber = 3;
  u8 requestedGroup;
  u8 sendRequest = 0;
  u8 cnt = 0;
  uint8_t sendEOS = 0; //KW1281_END_OF_SESSION
  KWState_t KWStatus;
  portBASE_TYPE xStatus;
  portTickType xWaitTime = 20 / portTICK_RATE_MS;
  uint8_t idleCnt = 0;

  buttonRightState = 0;
  
  //empty queues
  xStatus = xQueueReceive(xKW1281OutputQueue, &frame, 0);
  xStatus = xQueueReceive(xKW1281OutputQueue, &frame, 0);
  xStatus = xQueueReceive(xKW1281InputQueue, &frame, 0);
  xStatus = xQueueReceive(xKW1281InputQueue, &frame, 0);
  
  KWStart();

  do // petla po nawiazaniu polaczenia z ECU
  {
    frame.title=0xff;
    
    // odebranie ramki od ECU
    xStatus = xQueueReceive(xKW1281OutputQueue, &frame, xWaitTime);
    if (xStatus == pdFALSE)
    {
      KWStatus = KWGetStatus();
      if (KW_WORK == KWStatus)
      {
        cnt++;
        if (cnt > 10)
        {
          sendRequest = 1;
          clusterDiag_ShowScreen();
        }
        else
          continue;
      }
      else if (KW_ERROR == KWStatus)
      {
        break; //error
      }
      else if (KW_DISCONNECTED == KWStatus)
      {
        sendEOS = 1;
        break; //error
      }
      else if (KW_IDLE == KWStatus)
      {
        idleCnt++;
        if (idleCnt > 200) //kw_task in idle state longer than 4 seconds
          break; //error
      }
    }

    if (KW1281_GROUP_RESP == frame.title)
    {
      switch (requestedGroup)
      {
        case 3:
          clusterDiag_ProcessGroup3(frame.data);
          break;
      }

      sendRequest = 1;
    }

    if (xStatus == pdTRUE)
    {
      vPortFree(frame.data);
    }
    
    if (buttonRightState)
    {
      KWDisconnect();
      sendEOS = 1;
      buttonRightState = 0;
      break;
    }
      
    if (sendRequest)
    {
      frame.len = 4; //3
      frame.title = KW1281_GROUP_REQ; //KW1281_ERRORS_REQ
      frame.data = pvPortMalloc(frame.len-3);
      frame.data[0] = groupNumber;
      requestedGroup = groupNumber;
      xStatus = xQueueSendToBack(xKW1281InputQueue, &frame, 0);
    }
  } while (1);

  KWDisconnect();

  if (sendEOS)
    return 0;
  else
    return 255;
}


