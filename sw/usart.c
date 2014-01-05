#include "stm32f10x.h" 
#include "stdio.h"
#include "string.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

#include "usart.h"

#define USART1_TX_PORT GPIOB
#define USART1_TX_PIN  GPIO_Pin_6

#define USART1_RX_PORT GPIOB
#define USART1_RX_PIN  GPIO_Pin_7


xQueueHandle xUartQueue;

#ifdef UART1_DEBUG
void UartPrint(uint8_t *st)
{
  portBASE_TYPE xStatus;
  uint8_t * p = NULL;
  p = pvPortMalloc(strlen(st)+1);
  strcpy(p, st);
  xStatus = xQueueSendToBack( xUartQueue, &p, 0 );
}
#endif

void UartTransmitTask( void *pvParameters )
{
  uint8_t *pSaved = NULL;
  uint8_t *p = NULL;
  portBASE_TYPE xStatus;

  for( ;; )
  {
    xStatus = xQueueReceive( xUartQueue, &p, portMAX_DELAY );
    if( xStatus == pdPASS )
    {
      if (p != NULL)
      {
        pSaved = p;
        while (*p)
          USART1_PutData(*p++);
        vPortFree(pSaved);
      }
    }
  }
}


void USART1_Init(uint32_t speed)
{
  USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;

  /* USART1 Alternate Function mapping */
  GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE); // 0x00000004

  /* Configure USART1 Tx as alternate function push-pull */
  GPIO_InitStructure.GPIO_Pin = USART1_TX_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(USART1_TX_PORT, &GPIO_InitStructure);

  /* Configure USART1 Rx as input floating */
  GPIO_InitStructure.GPIO_Pin = USART1_RX_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(USART1_RX_PORT, &GPIO_InitStructure);

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

  USART_InitStructure.USART_BaudRate = speed;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No ;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  
  USART_Init(USART1, &USART_InitStructure);
  USART_Cmd(USART1, ENABLE);
} 


void USART1_Deinit()
{
  GPIO_InitTypeDef GPIO_InitStructure;

  /* Configure USART1 Tx as input floating */
  GPIO_InitStructure.GPIO_Pin = USART1_TX_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(USART1_TX_PORT, &GPIO_InitStructure);

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, DISABLE);
  USART_Cmd(USART1, DISABLE);
} 


/* Function returns 0xFF when at last one byte is available in receive buffer */
uint8_t USART1_DataAvailable() 
{
  if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
  {
    return 255; 
  }
  else 
  {
    return 0;
  }
}

/* Get one char from receive buffer */
uint8_t USART1_GetData() 
{
  return USART_ReceiveData(USART1);
}


void USART1_PutData(uint8_t ch) 
{ 
  /* Write a character to the USART */
  USART_SendData(USART1, (uint8_t) ch);
  while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
} 


/*     USART 2   */

void TXD2(uint8_t state)
{
  if (state == 0)
  {
    GPIO_ResetBits(GPIOA, GPIO_Pin_2); //USART2 tx PA2
  }
  else
  {
    GPIO_SetBits(GPIOA, GPIO_Pin_2); //USART2 tx PA2
  }
}

void USART2_Init(uint32_t speed)
{
  USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;

  /* Configure USART2 Tx (PA2) as alternate function push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  /* Configure USART2 Rx (PA3) as input floating */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

  USART_InitStructure.USART_BaudRate = speed;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No ;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  
  USART_Init(USART2, &USART_InitStructure);
  USART_Cmd(USART2, ENABLE);
} 


void USART2_Deinit()
{
  GPIO_InitTypeDef GPIO_InitStructure;

  USART_DeInit(USART2);
  USART_Cmd(USART2, DISABLE);

  /* Configure USART2 Tx (PA2) as output push-pull */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
    
  /* Configure USART2 Rx (PA3) as input floating */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_SetBits(GPIOA, GPIO_Pin_2);
} 


/* Function returns 0xFF when at last one byte is available in receive buffer */
uint8_t USART2_DataAvailable() 
{
  if (USART_GetFlagStatus(USART2, USART_FLAG_RXNE) != RESET)
  {
    return 255; 
  }
  else 
  {
    return 0;
  }
}


/* Get one char from receive buffer */
uint8_t USART2_GetData() 
{
  return USART_ReceiveData(USART2);
}


void USART2_PutData(uint8_t ch) 
{ 
  /* Write a character to the USART */
  USART_SendData(USART2, (uint8_t) ch);
  while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
} 

