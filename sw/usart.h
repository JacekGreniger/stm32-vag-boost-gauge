#ifndef USART_H
#define USART_H

extern xQueueHandle xUartQueue;

void UartTransmitTask( void *pvParameters );

#ifdef UART1_DEBUG
  void UartPrint(uint8_t *st);
#else
  #define UartPrint(st)
#endif

void USART1_Init(uint32_t speed);
void USART1_Deinit();
uint8_t USART1_DataAvailable();
uint8_t USART1_GetData();
void USART1_PutData(uint8_t ch);

void USART2_Init(uint32_t speed);
void USART2_Deinit();
uint8_t USART2_DataAvailable();
uint8_t USART2_GetData();
void USART2_PutData(uint8_t ch);
void TXD2(uint8_t state);

#endif
