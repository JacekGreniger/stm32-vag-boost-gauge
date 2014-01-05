#include "stm32f10x.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

#include "filesystem/integer.h"
#include "filesystem/diskio.h"
#include "filesystem/ff.h"

#include "io_thread.h"
#include "usart.h"

const uint8_t * speed_filename = "SPEED.TXT";

uint16_t ReadSpeedFile()
{
  FIL speed_file;
  FRESULT res;
  uint16_t speed;
  uint8_t textbuf[10];
  uint8_t * s;

  res = f_open(&speed_file, speed_filename, FA_OPEN_EXISTING | FA_READ);
  if (res != FR_OK) 
    return 0;

  s = f_gets(textbuf, sizeof(textbuf), &speed_file);
  if (NULL == s)
  {
    f_close(&speed_file);
    return 0;
  }

  s = strtok(textbuf, ", .");
  if ((NULL == s) || (strlen(s) < 4) || (strlen(s) > 5))
  {
    f_close(&speed_file);
    return 0;
  }

  speed = atoi(s);
  if ((speed < 7000) && (speed > 10400))
  {
    speed = 0;
  }
  res = f_close(&speed_file);
  if (res != FR_OK) { return 0; }
  
  return speed;
}

char uartSt[80];

void WriteFile()
{
  FIL log_file;
  FRESULT res;
  static uint8_t fileNumber = 0;
  char s[12] = "plik.";
  uint16_t i;
  char s2[5];

  sprintf(s2, "%03d", fileNumber++);
  
  strcat(s, s2);
  
  sprintf(uartSt, "Write file: %s\r\n", s);
  UartPrint(uartSt);
  
  res = f_open(&log_file, s, FA_CREATE_ALWAYS | FA_WRITE);
  if (res != FR_OK) 
  { 
    UartPrint("f_open() error\n");
    return; 
  }
  
  i = f_printf(&log_file, "Test 123\n");
  i = f_printf(&log_file, "Test 345\n");
  i = f_printf(&log_file, "Test 678\n");
  
  res = f_close(&log_file);
  if (res != FR_OK) 
  { 
    UartPrint("f_close() error\n");
    return; 
  }
}


typedef enum 
{
  VAG_STATE_IDLE,
  VAG_STATE_START_LOGGING,
  VAG_STATE_LOGGING,
  VAG_STATE_STOP_LOGGING,
  VAG_STATE_STOPPED,
  VAG_STATE_TERMINATED
} VAG_State_t;

void vag_main()
{
  UartPrint("\n*** SD card test\n");

  if (!cardStatus.initialized)
  {
    UartPrint("card not initialized\n");
    return;
  }
  
  uint16_t res;
  
  res = ReadSpeedFile();
  if (0 == res)
  {
    UartPrint("read again\n");
    res = ReadSpeedFile();
  }

  sprintf(uartSt, "speed = %d\r\n", res);
  UartPrint(uartSt);
  
  WriteFile();
  
//  if (FR_OK != f_mount(0, NULL))
//  {
//    printf("unmount error\r\n");
//    CardRemoved();
//    return;
//  }
}

