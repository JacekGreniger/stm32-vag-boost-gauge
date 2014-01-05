#include "stm32f10x.h"
#include "stdio.h"
#include "lcd.h"
#include "skoda_logo.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

#include "filesystem/integer.h"
#include "filesystem/diskio.h"
#include "filesystem/ff.h"

#include "kw1281.h"
#include "usart.h"
#include "ecu_diag.h"
#include "cluster_diag.h"
#include "io_thread.h"
#include "kw_task.h"

#define mainECHO_TASK_PRIORITY	    ( tskIDLE_PRIORITY + 1 )
#define mainINTEGER_TASK_PRIORITY   ( tskIDLE_PRIORITY )
#define mainGEN_QUEUE_TASK_PRIORITY ( tskIDLE_PRIORITY )


/*******************************************************************************
* Function Name  : NVIC_Configuration
* Description    : Configures Vector Table base location.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void NVIC_Configuration(void)
{
#ifdef  VECT_TAB_RAM  
  /* Set the Vector Table base location at 0x20000000 */ 
  NVIC_SetVectorTable(NVIC_VectTab_RAM, 0x0); 
#else  /* VECT_TAB_FLASH  */
  /* Set the Vector Table base location at 0x08000000 */ 
  NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);   
#endif
  
  /* Configure four bits for preemption priority */
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
}


/*******************************************************************************
* Function Name  : RCC_Configuration
* Description    : Configures the different system clocks.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void RCC_Configuration(void)
{    
  ErrorStatus HSEStartUpStatus;

  /* RCC system reset(for debug purpose) */
  RCC_DeInit();

  /* Enable HSE */
  RCC_HSEConfig(RCC_HSE_ON);

  /* Wait till HSE is ready */
  HSEStartUpStatus = RCC_WaitForHSEStartUp();

  if(HSEStartUpStatus == SUCCESS)
  {
    /* Enable Prefetch Buffer */
    FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);

    /* Flash 2 wait state */
    FLASH_SetLatency(FLASH_Latency_2);
 
    /* HCLK = SYSCLK */
    RCC_HCLKConfig(RCC_SYSCLK_Div1); 
  
    /* PCLK2 = HCLK */
    RCC_PCLK2Config(RCC_HCLK_Div1); 

    /* PCLK1 = HCLK/2 */
    RCC_PCLK1Config(RCC_HCLK_Div2);

    /* PLLCLK = 8MHz * 9 = 72 MHz */
    RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);

    /* Enable PLL */ 
    RCC_PLLCmd(ENABLE);

    /* Wait till PLL is ready */
    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
    {
    }

    /* Select PLL as system clock source */
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    /* Wait till PLL is used as system clock source */
    while(RCC_GetSYSCLKSource() != 0x08);
  }
  
  /* Enable DMA1 clock */
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

  /* Enable GPIOA and USART1 clocks */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
}


/*******************************************************************************
* Function Name  : GPIO_Configuration
* Description    : Configures the different GPIO ports.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void GPIO_Configuration(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; //USART2 tx PA2
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_SetBits(GPIOA, GPIO_Pin_2);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_14;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
}


void Timer3Init()
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  TIM_OCInitTypeDef  TIM_OCInitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

  //clocked with 36MHz timers clock is multiplied by 2 so CLK is 72MHz

  /* ---------------------------------------------------------------------------
    TIM3 Configuration: 
    TIM3CLK = 72 MHz, Prescaler = 71 (divide by 72), TIM3 counter clock = 1000 kHz (period 0.001ms)
  ----------------------------------------------------------------------------*/
 
  /* Time base configuration */
  TIM_TimeBaseStructure.TIM_Period = 65535;
  TIM_TimeBaseStructure.TIM_Prescaler = 71;
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
  
  /* Output Compare Timing Mode configuration: Channel1 */
  TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Inactive; //TIM_OCMode_Toggle;
  TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;
  TIM_OCInitStructure.TIM_Pulse = 0;
  /*TIM_Pulse configures the pulse value to be loaded into the Capture Compare Register. This
member must be a number between 0x0000 and 0xFFFF.*/
  TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
  
  TIM_OC1Init(TIM3, &TIM_OCInitStructure);

  TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Disable);
  
  /* TIM IT enable */
  TIM_ITConfig(TIM3, TIM_IT_CC2, DISABLE);
  TIM_ITConfig(TIM3, TIM_IT_CC1, ENABLE);

  TIM_Cmd(TIM3, ENABLE);

  // Enable the TIM3 global Interrupt
  NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 14;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}


#ifdef  DEBUG
/*******************************************************************************
* Function Name  : assert_failed
* Description    : Reports the name of the source file and the source line number
*                  where the assert_param error has occurred.
* Input          : - file: pointer to the source file name
*                  - line: assert_param error line source number
* Output         : None
* Return         : None
*******************************************************************************/
void assert_failed(u8* file, u32 line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif


//delay(10000); //2.070ms @ 72MHz
void delay(int time)
{
  while(time--)
  __asm__ volatile ("nop");  
}


//longdelay(96); //0.2s @ 72MHz
void longdelay(unsigned long t) 
{
  while (t--) 
  {
    delay(10000);
  }
}


int putchar(const int ch)
{
  //USART1_PutData(ch);
  LCDPrintChar(ch);
  return ch;
}


#define POWER_OFF_DELAY 10
#define CONNECT_DELAY 5
#define RECONNECT_DELAY 3

static void MainTaskNew(void *pvParameters )
{
  u8 cnt = CONNECT_DELAY;
  u16 uartSpeed = 0;
  uint8_t result;
  uint8_t mode = 1;
  uint8_t attemptsLeft = 2;
  uint8_t time;
  uint8_t refreshFullScreen = 1;

  UartPrint("MainTaskNew(): started\n");

  buttonRightState = 0;
  buttonLeftState = 0;

  while(1)
  {
    if (refreshFullScreen)
    {
      (void)USART2_GetData();
      LCDClear();
      LCDShowBitmap((u8*)skoda_logo);

      LCDSetTextPos(0,7);
      LCDSetNegative();
      if (0==mode)
        printf("Off    ");
      else if (1==mode)
        printf("ECU    ");
      else if (2==mode)
        printf("Cluster");
      LCDSetNormal();
      refreshFullScreen = 0;
    }

    if (0 == mode)
    {
      while ((!buttonRightState))
        vTaskDelay(500 / portTICK_RATE_MS);
    }

    while ((cnt > 0) && (!buttonRightState))
    {
      LCDSetTextPos(19,0);
      LCDSetNegative();
      printf("%2d", cnt);
      LCDSetNormal();
      time = 10; // *100ms = 1sec
      do
      {
        vTaskDelay(100 / portTICK_RATE_MS);
        --time;
      } while ((time > 0) && (!buttonRightState));
      --cnt;
    }   

    if (buttonRightState)
    {
      mode = (mode<2)?mode+1:0;
      if (0==mode)
        cnt = 0;
      else
        cnt = CONNECT_DELAY;

      attemptsLeft = 2;
      refreshFullScreen = 1;
      buttonRightState = 0;
      continue;
    }

    LCDClear();

    LCDSetTextPos(0,3);
    printf("connecting...");

    result = (mode==1)?ISO9141Init(&uartSpeed, 0x01):ISO9141Init(&uartSpeed, 0x17);

    if (0 == result)
    {
      UartPrint("MainTaskNew(): connected\n");

      LCDSetTextPos(0,3);
      printf("connected @ %d", uartSpeed);
      
      if (mode==1)
        result = ecuDiag_kw();
      else
        result = clusterDiag_kw();

      LCDClear();
      LCDSetTextPos(0,3);
      if (result)
      {
        UartPrint("MainTaskNew(): error during connection\n");

        printf("error");
        vTaskDelay(2000 / portTICK_RATE_MS);
        cnt = POWER_OFF_DELAY;
        mode = 0;
      }
      else if (0==result) //terminated by user
      {
        UartPrint("MainTaskNew(): disconnected\n");

        mode = (1==mode)?2:1; //set another mode
        attemptsLeft = 2;
        cnt = RECONNECT_DELAY;
      }
    }
    else
    {
      UartPrint("MainTaskNew(): connection failed\n");

      LCDSetTextPos(14,3);
      printf("failed");
      vTaskDelay(2000 / portTICK_RATE_MS);

      if (--attemptsLeft == 0)
      {
        mode = 0;
        cnt = POWER_OFF_DELAY;
      }
      else
        cnt = RECONNECT_DELAY;
    }

    refreshFullScreen = 1;
  } //main task loop
}


int main(void)
{
  RCC_Configuration();
  GPIO_Configuration();
  IOConfigure();
  NVIC_Configuration();
  Timer3Init();
  KW1281ConfigureTimer4();
  LCDPrepareDMA();
  LCDInit();

#ifdef UART1_DEBUG
  USART1_Init(115200);

  USART1_PutData(10);
  USART1_PutData('V');
  USART1_PutData('A');
  USART1_PutData('G');
  USART1_PutData(10);

  xUartQueue = xQueueCreate( 20, sizeof( uint8_t *) );
  xTaskCreate( UartTransmitTask, ( signed char * ) "utx", configMINIMAL_STACK_SIZE, NULL, mainECHO_TASK_PRIORITY, NULL );
#endif

  xTaskCreate( IOTask, ( signed char * ) "but", configMINIMAL_STACK_SIZE, NULL, mainECHO_TASK_PRIORITY+4, NULL );
  xTaskCreate( MainTaskNew, ( signed char * ) "mt", configMINIMAL_STACK_SIZE+512, NULL, mainECHO_TASK_PRIORITY+2, NULL );

  KWTaskInit(mainECHO_TASK_PRIORITY+3);

  /* Start the scheduler. */
  vTaskStartScheduler();

  while (1)
  {
  }
}


void vApplicationStackOverflowHook( xTaskHandle *pxTask, signed char *pcTaskName )
{
	/* This function will get called if a task overflows its stack.   If the
	parameters are corrupt then inspect pxCurrentTCB to find which was the
	offending task. */

	( void ) pxTask;
	( void ) pcTaskName;

	for( ;; );
}

/*
Using API functions within interrupts
Do not use API functions within interrupt service routines unless the name of the API function ends with "...FromISR()".

Cortex M3 users - please note - this is the cause of 95% of support requests on Cortex M3 devices:

API functions must not be called from an interrupt if the interrupt has a priority above the priority set by configMAX_SYSCALL_INTERRUPT_PRIORITY. Note carefully the following points when setting an interrupt priority:

    configMAX_SYSCALL_INTERRUPT_PRIORITY is defined in FreeRTOSConfig.h. On Cortex M3 devices, a numerically low interrupt priority value represents a logically high interrupt priority. Do not leave an interrupt priority unassigned because it will use priority 0 by default. Priority 0 is the highest possible interrupt priority and will be above configMAX_SYSCALL_INTERRUPT_PRIORITY.

    Take care when specifying a priority as different Cortex M3 implementations use a different number of priority bits.

    Internally the Cortex M3 uses the 'n' most significant bits of a byte to represent an interrupt priority, where 'n' is implementation defined as noted above. ARM and various Cortex M3 licensees provide library functions to allow interrupt priorities to be assigned, but some expect the priority to be shifted into the most significant bits before the library function is called, whereas others will perform the shift internally.

    The bits that define an interrupt priority are split between those that represent the preemption priority, and those that represent the sub priority. For greatest simplicity and compatibility, ensure that all the priority bits are assigned as 'preemption priority' bits. 
*/

