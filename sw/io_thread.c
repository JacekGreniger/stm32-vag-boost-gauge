#include "stm32f10x.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

#include "filesystem/integer.h"
#include "filesystem/diskio.h"
#include "filesystem/ff.h"

#include "usart.h"
#include "io_thread.h"

FATFS Fatfs[_DRIVES];    // File system object for each logical drive 
cardStatus_t cardStatus = {0,0};

uint8_t buttonRightState = 0;
uint8_t buttonLeftState = 0;

#define BUTTON_RIGHT_PIN GPIO_Pin_6
#define BUTTON_RIGHT_PORT GPIOA

#define BUTTON_LEFT_PIN GPIO_Pin_0
#define BUTTON_LEFT_PORT GPIOB

#define MMC_CARD_DETECT_PIN GPIO_Pin_8
#define MMC_CARD_DETECT_PORT GPIOA


void IOConfigure()
{
  GPIO_InitTypeDef GPIO_InitStructure;

  GPIO_InitStructure.GPIO_Pin = BUTTON_RIGHT_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(BUTTON_RIGHT_PORT, &GPIO_InitStructure);  

  GPIO_InitStructure.GPIO_Pin = BUTTON_LEFT_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(BUTTON_LEFT_PORT, &GPIO_InitStructure);  

  GPIO_InitStructure.GPIO_Pin = MMC_CARD_DETECT_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(MMC_CARD_DETECT_PORT, &GPIO_InitStructure);  
}


void IOTask(void *pvParameters )
{
  portTickType xLastWakeTime;
  const portTickType xFrequency = 10;
  uint8_t buttonRightTimer;
  uint8_t keyRightPressed = 0;

  uint8_t buttonLeftTimer;
  uint8_t keyLeftPressed = 0;

  /* The parameters are not used. */
  ( void ) pvParameters;

  UartPrint("IOTask(): started\n");

  // Initialise the xLastWakeTime variable with the current time.
  xLastWakeTime = xTaskGetTickCount();

  for(;;)
  {
    // Wait for the next cycle.
    vTaskDelayUntil( &xLastWakeTime, xFrequency );

    buttonRightTimer = (buttonRightTimer<91)?buttonRightTimer+1:buttonRightTimer;

    if (GPIO_ReadInputDataBit(BUTTON_RIGHT_PORT, BUTTON_RIGHT_PIN) == 0) 
    {
      if (keyRightPressed == 0)
      {
        keyRightPressed = 1;
        buttonRightTimer = 0;
      }
      else if ((buttonRightTimer > 80) && (buttonRightTimer < 128))
      {
        buttonRightState = 2;
        UartPrint(" -> right=2\n");
        buttonRightTimer = 128;
      }
    }
    else if ((GPIO_ReadInputDataBit(BUTTON_RIGHT_PORT, BUTTON_RIGHT_PIN) > 0) && (keyRightPressed))
    {
      keyRightPressed = 0;
      if ((buttonRightTimer > 4) &&(buttonRightTimer < 60))
      {
        buttonRightState = 1;
        UartPrint(" -> right=1\n");
      }
      else if ((buttonRightTimer > 80) && (buttonRightTimer < 128))
      {
        buttonRightState = 2;
        UartPrint(" -> right=2\n");
      }
    }


    buttonLeftTimer = (buttonLeftTimer<120)?buttonLeftTimer+1:buttonLeftTimer;

    if (GPIO_ReadInputDataBit(BUTTON_LEFT_PORT, BUTTON_LEFT_PIN) == 0) 
    {
      if (keyLeftPressed == 0)
      {
        keyLeftPressed = 1;
        buttonLeftTimer = 0;
      }
      else if ((buttonLeftTimer > 80) && (buttonLeftTimer < 128))
      {
        buttonLeftState = 2;
        UartPrint(" -> left=2\n");
        buttonLeftTimer = 128;
      }
    }
    else if ((GPIO_ReadInputDataBit(BUTTON_LEFT_PORT, BUTTON_LEFT_PIN) > 0) && (keyLeftPressed))
    {
      keyLeftPressed = 0;
      if ((buttonLeftTimer > 4) &&(buttonLeftTimer < 60))
      {
        buttonLeftState = 1;
        UartPrint(" -> left=1\n");
      }
      else if ((buttonLeftTimer > 80) && (buttonLeftTimer < 128))
      {
        buttonLeftState = 2;
        UartPrint(" -> left=2\n");
      }
    }

    if ((GPIO_ReadInputDataBit(MMC_CARD_DETECT_PORT, MMC_CARD_DETECT_PIN) == 0) && (cardStatus.inserted == 0)) //card inserted
    {
      cardStatus.inserted = 1;
      UartPrint(" -> card inserted\n");
      CardInserted();
  
      if (FR_OK != f_mount(0, &Fatfs[0]))
      {
        UartPrint(" -> mount error\n");
        CardRemoved();
      }
      else
      {
        cardStatus.initialized = 1;
      }
    }

    if ((GPIO_ReadInputDataBit(MMC_CARD_DETECT_PORT, MMC_CARD_DETECT_PIN) > 0) && (cardStatus.inserted == 1)) //card removed
    {
      cardStatus.inserted = 0;
      cardStatus.initialized = 0;
      CardRemoved();
      UartPrint(" -> card removed\n");
    }

  }
}


