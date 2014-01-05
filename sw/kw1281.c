#include "stm32f10x.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

#include "usart.h"
#include "kw1281.h"

u8 KW1281BlockCounter;
// nalezy go powiekszyc o 1 przed wyslaniem nowego bloku oraz po otrzymaniu bloku od ECU
// mozna wykorzystac do sprawdzenia czy nie zostala zagubiona jakas ramka

u8 KW1281ProtocolVersion[2];

u8  kw1281_max_init_attempts = 2; // maksymalna ilosc prob polaczenia ze sterownikiem
u8  kw1281_max_byte_transmit_attempts = 1; //wysylanie komunikatu do ecu, maksymalna liczba prób wyslania bajtu w przypadku nieotrzymania zanegowanej odpowiedzi

extern vu16 timerKW;

u8  kw1281_interbyte_delay = 2; //opoznienie przed wyslaniem kolejnych bajtow komunikatu lub opoznienie w wysylaniu zanegowanego potwierdzenia do ecu
u8  kw1281_interbyte_delaymax = 40; //maksymalny czas na przyjscie kolejnego bajtu z ecu lub maksymalny czas oczekiwania na przyjscie zanegowanego potwierdzenia od ecu
u16 kw1281_intermessage_delay = 10; //opoznienie przed wyslaniem nowego komunikatu
u16 kw1281_intermessage_delaymax = 500; //maksymalny czas oczekiwania na przyjscie nowego komunikatu od ecu

#define NULL 0

void delay_ms(uint16_t nCount)
{
  int time;
  for(; nCount != 0; nCount--)
  {
    time = 5000; //1.035ms @ 72MHz
    while(time--)
      __asm__ volatile ("nop");  
  }
}


void KW1281ConfigureTimer4()
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  TIM_OCInitTypeDef  TIM_OCInitStructure;

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
  
  //clocked with 36MHz timers clock is multiplied by 2 so CLK is 72MHz

  /* ---------------------------------------------------------------------------
    TIM4 Configuration: 
    TIM4CLK = 72 MHz, Prescaler = 719 (divide by 720), TIM4 counter clock = 100 kHz (period 0.01ms)
  ----------------------------------------------------------------------------*/
 
  /* Time base configuration */
  TIM_TimeBaseStructure.TIM_Period = 65535;
  TIM_TimeBaseStructure.TIM_Prescaler = 71; 
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
  TIM_Cmd(TIM4, ENABLE);
}

/* ISO9141Init - wyslanie adresu 0x01 z predkoscia 5b oraz odebranie bajtu treningowego oraz dwoch bajtow oznaczajacych ?typ protokolu?
     errors:
     0x10 - wrong protocol - not KW1281 ketwords 0x01 0x8a
     0x7f - zla wartosc bajtu treningowego z  ECU (zla predkosc transmisji)
     0x8f - odczytany zostal inny bajt niz wyslany (halfduplex)
     0xff - timeout
*/

u8 ISO9141Init(u16 * uartSpeed, u8 devAddr) 
{
  u8 c, keyword;
  int i;
  u16 time, t2;
  uint8_t ones = 0;

  USART2_Deinit();

  // wyslanie adresu 0x01 z predkoscia 5 bit/sek, 7O1
  //bit nieparzystosci rowny 0 gdy w wysylanym slowie jest nieparzysta liczba jedynek
  TXD2(0); //bit startu
  timerKW = 200;
  while (timerKW);

  for (i=0; i<7; i++)
  {
    if (devAddr & (1<<i))
    {
      TXD2(1);
      ones++;
    }
    else
    {
      TXD2(0);
    }

    timerKW = 200;
    while (timerKW);
  }

  if (ones & 0x01) //bit nieparzystosci rowny 0 gdy w wysylanym slowie jest nieparzysta liczba jedynek
    TXD2(0);
  else
    TXD2(1);
  timerKW = 200;
  while (timerKW);

  TXD2(1); //bit stopu
  timerKW=200; 
  while (timerKW);
  // bez oczekiwania po ustawieniu 1 na txd wykrywa³ zbocze opadajace od razu
  // przypuszczalnie przeladowywala sie pojemnosc w transceiverze

  timerKW=1000; // 1 sec

  if (0 == (*uartSpeed))
  {
    while ((GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3)) && timerKW); //waiting for start bit
    if (0 == timerKW) 
    {
      return 0xff; 
    }
    TIM_SetCounter(TIM4, 0);
    t2 = timerKW;

    for (i=0; i<4; i++)
    {
      while ((0 == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3)) && timerKW); //waiting for rising edge
      if (0 == timerKW) 
      {
        return 0xff; 
      }
      while ((GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3)) && timerKW); //waiting for falling edge
      if (0 == timerKW) 
      {
        return 0xff; 
      }
    }

    while ((0 == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3)) && timerKW); //waiting for rising edge
    if (0 == timerKW) 
    {
      return 0xff; 
    }

    time = TIM_GetCounter(TIM4);
    
    if (time > 900)
    {
      *uartSpeed = 9600;
    }
    else 
    {
      *uartSpeed = 10400;
    }

    USART2_Init(*uartSpeed);
    timerKW=2; //2ms
    while (timerKW);
    c=USART2_GetData(); //flush buffer
  }
  else
  {
    USART2_Init(*uartSpeed);
    c=USART2_GetData(); //flush buffer

    while (!USART2_DataAvailable() && timerKW);
    if (0 == timerKW) 
    {
      return 0xff; 
    }
 
    c = USART2_GetData(); //odebranie bajtu 0x55 z ECU 
    if (0x55 != c) //jezeli odebrana wartosc jest inna to znaczy, ze jest zla predkosc transmisji
    {
      return 0x7f; //sync err 
    }
  }
  
  timerKW=500; // 0.5 sec

  //odebranie dwoch bajtow oznaczajacych ?wersje protokolu?
  while (!USART2_DataAvailable() && timerKW);
  if (0 == timerKW) 
  {
    return 0xff; 
  }

  KW1281ProtocolVersion[0] = USART2_GetData(); // pierwszy bajt wersji protokolu

  while (!USART2_DataAvailable() && timerKW);
  if (0 == timerKW) 
  {
    return 0xff; 
  }
  KW1281ProtocolVersion[1] = USART2_GetData(); //drugi bajt wersji protokolu

  USART2_PutData(255-KW1281ProtocolVersion[1]); //wyslanie negacji ostatniego otrzymanego bajtu
  
  while (!USART2_DataAvailable() && timerKW);
  if (0 == timerKW) 
  {
    return 0xff; 
  }
  
  c = USART2_GetData(); // odczytanie bajtu który zostal wyslany - halfduplex

  if ((255-KW1281ProtocolVersion[1]) != c) 
  {
    return 0x8f; // odczytany zostal inny bajt niz wyslany
  }
  
  if ((KW1281ProtocolVersion[0] != 0x01))// || (KW1281ProtocolVersion[1] != 0x8A))
  {
    return 0x10;
  }

  timerKW = 0;
  return 0x00;
}

// ***************************
// IMPROVED KW1281 functions
// ***************************


/* KW1281SendByteReceiveAnswer - wysyla bajt b oraz weryfikuje na zanegowana odpowiedz od ECU
     errors:
     0x00 - all ok
     0x1f; // nie odczytano wyslanego bajtu (przekroczony czas 2ms)
     0x2f; //odczytany zostal inny bajt niz wyslany (halfduplex)
     0x7f - bledne potwierdzenie bajtu od ecu
     0xff - trzy proby wyslania bajtu nieudane (przekroczony czas t2max
     
     kw1281_max_byte_transmit_attempts = 3;
     kw1281_interbyte_delaymax = 40; 
*/
u8 KW1281SendByteReceiveAnswer (u8 b) 
{
  u8 c;
  u8 cnt;
  
  cnt = kw1281_max_byte_transmit_attempts; // trzy proby
  while (cnt--)
  {
    USART2_PutData(b); // wyslanie bajtu do ECU

//    timerKW = 2; //2ms na odebranie wyslanego bajtu
//    while (!USART2_DataAvailable() && timerKW);
//    if (0 == timerKW) 
//    {
//      return 0x1f; // nie odczytano wyslanego bajtu
//    }
    while (!USART2_DataAvailable());
    c = USART2_GetData(); // odczytanie bajtu który zostal wyslany - halfduplex
    if (b != c) 
    {
      return 0x2f; //odczytany zostal inny bajt niz wyslany (halfduplex)
    }
  
    timerKW = kw1281_interbyte_delaymax;
    while (!USART2_DataAvailable() && timerKW);
    if (0 == timerKW) 
    {
      continue; // nie otrzymano zanegowanej odpowiedzi w przeciagu delay_kw1281_t2max
    }
  
    c = USART2_GetData(); // odczytanie zanegowanej odpowiedzi od ecu
    if ((255-b) != c) 
    {
      return 0x7f; //bledne potwierdzenie bajtu od ecu
    }
  
    return 0x00; // all ok
  }
  return 0xff; // trzy proby nieudane
}


/* KW1281SendBlock - wysyla blok danych do ecu
     errors:
     0x00 - all ok
     0x1f; // nie odczytano wyslanego bajtu (przekroczony czas 2ms)
     0x2f; //odczytany zostal inny bajt niz wyslany (halfduplex)
     
     u8 kw1281_interbyte_delay = 2;
*/  
u8 KW1281SendBlock(KW1281struct_t * block)
{
  u8 i;
  u8 resp;

  resp = KW1281SendByteReceiveAnswer(block->len); // block length
  if (resp > 0)
  {
    return resp;
  }
  
  delay_ms(kw1281_interbyte_delay);
  
  resp = KW1281SendByteReceiveAnswer(block->cnt); //block counter
  if (resp > 0)
  {
    return resp;
  }

  delay_ms(kw1281_interbyte_delay);

  resp = KW1281SendByteReceiveAnswer(block->title); //block title
  if (resp > 0)
  {
    return resp;
  }

  delay_ms(kw1281_interbyte_delay);

  if ( ((block->len) > 3) && (block->data != NULL) ) //block data
  {
    for (i=0; i < ((block->len)-3); i++) 
    {
      resp = KW1281SendByteReceiveAnswer( *( block->data +i) ); //block data
      if (resp > 0)
      {
        return resp;
      }
      
      delay_ms(kw1281_interbyte_delay);
    }
  }
  
  USART2_PutData(KW1281_BLOCK_END); //block end
  
//  timerKW = 2; //2ms na odebranie wyslanego bajtu
//  while (!USART2_DataAvailable() && timerKW);
//  if (0 == timerKW) 
//  {
//    return 0x1f; // nie odczytano wyslanego bajtu
//  }
  while (!USART2_DataAvailable());
  resp = USART2_GetData();
  if (KW1281_BLOCK_END != resp) 
  {
    return 0x2f; //odczytany zostal inny bajt niz wyslany (halfduplex)
  }
  
  return 0x00; //wyslanie bloku zakonczylo sie powodzeniem
}


/* KW1281ReceiveByteSendAnswer - odbiera bajt b oraz wysyla jego zanegowana wartosc do ECU
     errors:
     0x00 - all ok
     0x1e; // nie odczytano wyslanego bajtu (przekroczone 2ms)
     0x2e; //odczytany zostal inny bajt niz wyslany (halfduplex)
     0xde - brak odpowiedzi od ecu (przekroczony czas kw1281_interbyte_delaymax)
     
     kw1281_interbyte_delaymax = 40;
     kw1281_interbyte_delay = 2;
*/
u8 KW1281ReceiveByteSendAnswer (u8 * b) 
{
  u8 resp;
  u16 i;
  
  timerKW = kw1281_interbyte_delaymax;
  while (!USART2_DataAvailable() && timerKW);
  if (0 == timerKW) 
  {
    return 0xde; //brak odpowiedzi od ecu
  }

  *b = USART2_GetData(); //odebranie bajtu z ECU

  delay_ms(kw1281_interbyte_delay);

  USART2_PutData(255-(*b)); // wyslanie negacji odebranego bajtu

//  timerKW = 2;
//  while (!USART2_DataAvailable() && timerKW);
//  if (0 == timerKW) 
//  {
//    return 0x1e; 
//  }

  while (!USART2_DataAvailable());
  resp = USART2_GetData(); // odczytanie bajtu który zostal wyslany - halfduplex
  
  if ((255-(*b)) != resp) {
    return 0x2e; // odczytany zostal inny bajt niz wyslany
  }
  
  return 0x00;
}

/* KW1281ReceiveBlock - odebranie bloku z ecu

errors:
    0x00 - all ok
    0xdd - brak odpowiedzi od ecu (poczatek ramki), przekroczony t3max, mozna sprobowac ponownie wyslac komunikat
    0xed - bledna wartosc bajtu konczacego blok 
    0xfd - nie przyszedl bajt konczacy blok

u16 kw1281_intermessage_delaymax = 500;
*/
u8 KW1281ReceiveBlock(KW1281struct_t * block) 
{
  u8 i;
  u8 resp;
  
  timerKW = kw1281_intermessage_delaymax;
  while (!USART2_DataAvailable() && timerKW);
  if (0 == timerKW) 
  {
    return 0xdd; //brak odpowiedzi od ecu
  }
  
  resp = KW1281ReceiveByteSendAnswer( &(block->len) ); //odebranie Block Length
  if (resp > 0) 
  {
    return resp;
  }

  resp = KW1281ReceiveByteSendAnswer( &(block->cnt) ); //odebranie Block Counter
  if (resp > 0) 
  {
    return resp;
  }
 
  resp = KW1281ReceiveByteSendAnswer( &(block->title) );
  if (resp > 0) 
  {
    return resp;
  }
  
  if ( (block->len) > 3 ) //block data
  {
    for (i=0; i < ((block->len)-3); i++) 
    {
      resp = KW1281ReceiveByteSendAnswer( block->data + i); //block data
      if (resp > 0) 
      {
        return resp;
      }
    }
  }

  timerKW = kw1281_interbyte_delaymax;
  while (!USART2_DataAvailable() && timerKW);
  if (0 == timerKW) 
  {
    return 0xfd; 
  }
  
  i = USART2_GetData(); // odczytanie ostatniego bajtu bloku o wartosci 0x03 - bez wysylania potwierdzenia
  if (0x03 != i)
  {
    return 0xed;
  }

  return 0x00; //wyslanie bloku zakonczylo sie powodzeniem
}


/* KW1281SendACK - wyslanie bloku ACK do ECU
   errors:
   0x2f - malloc
*/
u8 KW1281SendACK () 
{
  KW1281struct_t ack_block;
  u8 resp;
  
  ack_block.len = 3;
  ack_block.title = KW1281_ACK;
  KW1281BlockCounter++; // inkrementacja lokalnie przechowywanej wartosci Block Counter
  ack_block.cnt = KW1281BlockCounter;
  ack_block.data = NULL; // blok danych jest pusty przy wysylaniu ACK
  
  resp = KW1281SendBlock(&ack_block); //wyslanie bloku do ECU
  return resp;
}
