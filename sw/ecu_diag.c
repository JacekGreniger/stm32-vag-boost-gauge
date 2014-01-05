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

#include "lcd.h"
#include "usart.h"
#include "ecu_diag.h"
#include "kw1281.h"
#include "kw_task.h"
#include "io_thread.h"
#include "file_operations.h"

#define DEG_SYMBOL 128
#define LAMBDA_SYMBOL 129
#define BAR_SIZE 100 //101 pixels wide

uint16_t maxInjVal = 0;
uint8_t maxInjCnt = 0;
uint16_t maxMAFVal = 0;
uint8_t maxMAFCnt = 0;

uint8_t frameBuffer[1056];
uint8_t lineBuffer[BAR_SIZE+1];

uint8_t config_prev[3][4] = {{0,0,0,0},{0,0,0,0},{0,0,0,0}};

#define FIRST_DIAG_MODE DIAG_MODE_LAMBDA_BOOST


void ProcessGroup32(uint8_t * frameData)
{
  float f; // 20 a*(b-128)/128
  u16 i,j;
  LCDSetTextPos(4,7);

  f = (float)frameData[1]* (frameData[2] - 128.0);
  f = f/128.0;
  if (f < 0)
  {
    LCDPrintChar('-');
    f = f * (-1.0);
  }
  else
  {
    LCDPrintChar(' ');
  }
  i = f;
  f = f - i;
  f = f * 100;
  j = f;
  printf("%2d.%02d", i, j);

  LCDSetTextPos(14,7);
  f = (float)frameData[4]* (frameData[5] - 128.0);
  f = f/128.0;
  if (f < 0)
  {
    LCDPrintChar('-');
    f = f * (-1.0);
  }
  else
  {
    LCDPrintChar(' ');
  }
  i = f;
  f = f - i;
  f = f * 100;
  j = f;
  printf("%2d.%02d", i, j);
}


void ShowScale33()
{
  memset(lineBuffer, 0x03, BAR_SIZE+1);
 
  lineBuffer[0] = 0x0F;
  lineBuffer[25] = 0x0F;
  lineBuffer[49] = 0x0F;
  lineBuffer[50] = 0x0F;
  lineBuffer[51] = 0x0F;
  lineBuffer[75] = 0x0F;
  lineBuffer[100] = 0x0F;
  
  LCDSetPosition(12,4);
  LCDDMATransfer(lineBuffer, BAR_SIZE+1);
}


void ShowBar33(uint8_t val)
{
  int i;
  uint8_t pattern = 0xAA;

  if (val > BAR_SIZE)
  {
    val = BAR_SIZE;
  } 

  memset(lineBuffer, 0x00, BAR_SIZE+1);
  lineBuffer[49] = 0xff;
  lineBuffer[50] = 0xff;
  lineBuffer[51] = 0xff; 

  if (val < 50)
  {
    for (i=val; i<50; i++)
    {
      lineBuffer[i] = pattern;
      pattern = (0xAA==pattern)?0x55:0xAA;
    }
  }
  else if (val >= 50)
  {
    for (i=50; i<=val; i++)
      lineBuffer[i] = 0xFF;
  }

  LCDDMATransfer(lineBuffer, BAR_SIZE+1);
}


void ProcessGroup33(uint8_t * frameData)
{
  u16 probeVoltage;
  u16 val;
  probeVoltage = frameData[4] * frameData[5];
  LCDSetTextPos(6,2);
  printf("%4d",probeVoltage);

  if (probeVoltage > 3000)
  {
    probeVoltage = 3000;
  }
  val = probeVoltage / 30;
  val = 100-val;

  LCDSetPosition(12,3);
  ShowBar33(val);
}


void ShowScale31()
{
  memset(lineBuffer, 0x00, BAR_SIZE+1);
 
  lineBuffer[0] = 0x03;
  lineBuffer[10] = 0x03;
  lineBuffer[30] = 0x03;
  lineBuffer[49] = 0x03;
  lineBuffer[50] = 0x03;
  lineBuffer[51] = 0x03;
  lineBuffer[70] = 0x03;
  lineBuffer[90] = 0x03;
  lineBuffer[100] = 0x03;

  BufferPrintCharSmall(&lineBuffer[6], '0');
  BufferPrintCharSmall(&lineBuffer[9], '.');
  BufferPrintCharSmall(&lineBuffer[12], '8');

  BufferPrintCharSmall(&lineBuffer[26], '0');
  BufferPrintCharSmall(&lineBuffer[29], '.');
  BufferPrintCharSmall(&lineBuffer[32], '9');

  BufferPrintCharSmall(&lineBuffer[45], '1');
  BufferPrintCharSmall(&lineBuffer[49], '.');
  BufferPrintCharSmall(&lineBuffer[53], '0');

  BufferPrintCharSmall(&lineBuffer[66], '1');
  BufferPrintCharSmall(&lineBuffer[69], '.');
  BufferPrintCharSmall(&lineBuffer[70], '1');

  BufferPrintCharSmall(&lineBuffer[86], '1');
  BufferPrintCharSmall(&lineBuffer[89], '.');
  BufferPrintCharSmall(&lineBuffer[92], '2');

  LCDSetPosition(12,4);
  LCDDMATransfer(lineBuffer, BAR_SIZE+1);
}


void ShowBar31(uint8_t val)
{
  int i;
  uint8_t pattern = 0xAA;

  //val = 75;
  if (val > BAR_SIZE)
  {
    val = BAR_SIZE;
  } 

  memset(lineBuffer, 0x80, BAR_SIZE+1);
  lineBuffer[49] |= 0xff;
  lineBuffer[50] |= 0xff;
  lineBuffer[51] |= 0xff; 

  if (val < 50)
  {
    for (i=val; i<=51; i++)
    {
      lineBuffer[i] |= 0xFF;
    }
  }
  else if (val > 51)
  {
    for (i=50; i<=val; i++)
    {
      lineBuffer[i] |= pattern;
      pattern = (0xAA==pattern)?0x55:0xAA;
    }
  }

  LCDDMATransfer(lineBuffer, BAR_SIZE+1);
}


void ProcessGroup31(uint8_t * frameData)
{
  u16 lambda;
  uint8_t lambda_int;
  u16 lambda_frac;
  int16_t i;

  //reg 11: 0.0001*a*(b-128)+1  -
  i = frameData[1] * (frameData[2] - 128.0);
  i = (i/10) + 1000;
  lambda = i;

  lambda_int = lambda/1000;
  lambda_frac = (lambda%1000)/10;
  LCDSetTextPos(7,2);
  printf("%c %d.%02d", LAMBDA_SYMBOL, lambda_int, lambda_frac);

  if (lambda > 1250)
  {
    lambda = 1250;
  }
  else if (lambda < 750)
  {
    lambda = 750;
  }

  lambda = (lambda - 750)/5; //now range is 0-100

  LCDSetPosition(12,3);
  ShowBar31(lambda);
}


void ProcessGroup31Graph(uint8_t * frameData, uint8_t * clearFrameBuffer)
{
  uint8_t * frameBuffer_p;
  uint8_t i,j;
  u16 val;
  u16 lambda;
  int16_t i16;

  //reg 11: 0.0001*a*(b-128)+1  -
  i16 = frameData[1] * (frameData[2] - 128.0);
  i16 = (i16/10) + 1000;
  lambda = i16;
  val = lambda;

  if (val > 1300)
    val = 1300;
  else if (val < 700)
    val = 700;
  val = (val-700)/10;

  if (*clearFrameBuffer)
  {
    memset(frameBuffer, 0, 1056);
    memset(frameBuffer+528, 1 <<(33%8), 128); 
    *clearFrameBuffer = 0;
  }

  for (j=0; j<8; j++)
  {
    frameBuffer_p = frameBuffer;
    frameBuffer_p += 132*j;

    for (i=0; i<127; i++)
    {
      *frameBuffer_p = *(frameBuffer_p+1);
      ++frameBuffer_p;
    }
  }
  for (j=0; j<8; j++)
  {
    frameBuffer[127+j*132] = 0;
  }
   
  val += 3;
  frameBuffer[127+(val/8)*132] = 1 <<(val%8); 
  frameBuffer[528+127] |= 0x02;

  LCDSetPosition(0,0);
  LCDDMATransfer(frameBuffer, 1056);
  LCDSetTextPos(0,0);

  uint8_t lambda_int;
  u16 lambda_frac;

  lambda_int = lambda/1000;
  lambda_frac = (lambda%1000)/10;
  printf("%c %d.%02d", LAMBDA_SYMBOL, lambda_int, lambda_frac);
}


void ShowScale115(uint8_t val1, uint8_t val2)
{
  int i;
  uint8_t pos;

  memset(lineBuffer, 0x80, BAR_SIZE+1);
  if (val1 > BAR_SIZE)
  {
    val1 = BAR_SIZE;
  }
  
  lineBuffer[0] = 0xE0;
  lineBuffer[20] = 0xE0;
  lineBuffer[40] = 0xE0;
  lineBuffer[60] = 0xE0;
  lineBuffer[80] = 0xE0;
  lineBuffer[100] = 0xE0;
  
  for (i=0; i<=val1; i++)
  {
    lineBuffer[i] |= 0x0F;
  }
  val2 = (val2<1)?1:val2;
  val2 = (val2>99)?99:val2;
  lineBuffer[val2-1] |= 0x60;
  lineBuffer[val2] |= 0x70;
  lineBuffer[val2+1] |= 0x60;
  lineBuffer[val2-1] &= 0x7F;
  lineBuffer[val2] &= 0x7F;
  lineBuffer[val2+1] &= 0x7F;

  LCDDMATransfer(lineBuffer, BAR_SIZE+1);
}


void ProcessGroup115(uint8_t * frameData)
{
  u16 pressure1, pressure2;
  u16 val1, val2;

  pressure1 = frameData[7] * frameData[8];
  pressure1 /= 25;
  
  pressure2 = frameData[10] * frameData[11];
  pressure2 /= 25;

  //pressure1 = 1450;
  //boost format x.xx bar
  val1 = pressure1/1000;
  val2 = (pressure1%1000)/10;
  LCDSetTextPos(7,5);  
  printf("%01d.%02d", val1, val2);

  val1 = (pressure1 >= 1000)?pressure1:1000;
  val1 = (val1 > 2000)?2000:val1;
  val1 -= 1000;
  val1 /= 10;
  val2 = (pressure2 >= 1000)?pressure2:1000;
  val2 = (val2 > 2000)?2000:val2;
  val2 -= 1000;
  val2 /= 10;

  LCDSetPosition(12,6);
  ShowScale115(val1,val2);
}


#define MAX_VALUE_HOLD 30

void ProcessGroup2_4(uint8_t groupNumber, uint8_t * frameData)
{
  u16 rpm;
  u16 load;

  if (2 == groupNumber)
  {
    rpm = frameData[1] * frameData[2];
    rpm /= 5;
  
    LCDSetTextPos(4,2);
    printf("%4d", rpm);
  
    load = 100 * frameData[5];
    load /= (frameData[4]>0)?frameData[4]:1;
    LCDSetTextPos(16,2);
    printf("%3d", load);
  
    u16 inj, ms, us;
    inj = frameData[7] * frameData[8]; // in 10us
    ms = inj/100;
    us = (inj%100)/10;
    LCDSetTextPos(5,3);
    printf("%2d.%1d", ms, us);

    if (ms > 15)
    {
      if (ms >= maxInjVal)
      {
        maxInjVal = ms;
        maxInjCnt = MAX_VALUE_HOLD;
        LCDSetTextPos(15,3);
        LCDSetNegative();
        printf("%2d ms", ms);
        LCDSetNormal();
      }
      else if (maxInjCnt)
        --maxInjCnt;
    }
    else if (maxInjCnt)
      --maxInjCnt;

    if ((0 == maxInjCnt) && (maxInjVal > 0))
    {
      maxInjVal = 0;
      LCDSetTextPos(15,3);
      printf("     ");
    }

    float flow = (frameData[11]*1.421) + (frameData[10]/182.0);
    u16 g, mg;
    g = flow;
    flow = flow-g;
    flow = 10*flow;
    mg = flow;
    LCDSetTextPos(4,4);
    printf("%3d.%1d", g, mg);

    if (g > 100)
    {
      if (g >= maxMAFVal)
      {
        maxMAFVal = g;
        maxMAFCnt = MAX_VALUE_HOLD;
        LCDSetTextPos(14,4);
        LCDSetNegative();
        printf("%3d g/s", g);
        LCDSetNormal();
      }
      else if (maxMAFCnt)
        --maxMAFCnt;
    }
    else if (maxMAFCnt)
      --maxMAFCnt;

    if ((0 == maxMAFCnt) && (maxMAFVal > 0))
    {
      maxMAFVal = 0;
      LCDSetTextPos(14,4);
      printf("       ");
    }
  }
  else if (4 == groupNumber)
  {
    //21 0.001*a*b
    u16 i, v, mv;
    i = frameData[4] * frameData[5];
    v = i / 1000;
    mv = i - 1000*v;
    mv /= 10;
    LCDSetTextPos(8,5);
    printf("%2d.%02d", v, mv);

    //5 a*(b-100)*0.1 oder 0.1*a*b - 10*a
    int temp;
    temp = frameData[7] * frameData[8] / 10;
    temp = temp - 10 * frameData[7];
    LCDSetTextPos(6,6);
    if (temp < 0)
    {
      if (temp < -99)
        temp = -99;
      temp = temp * (-1);
      printf("-%02d%c", temp, DEG_SYMBOL);
    }
    else
    {
      printf("%3d%c", temp, DEG_SYMBOL);
    }

    temp = frameData[10] * frameData[11] / 10;
    temp = temp - 10 * frameData[10];
    LCDSetTextPos(17,6);
    if (temp < 0)
    {
      if (temp < -99)
        temp = -99;
      temp = temp * (-1);
      printf("-%02d%c", temp, DEG_SYMBOL);
    }
    else
    {
      printf("%3d%c", temp, DEG_SYMBOL);
    }
  }
}


void ShowDebugGroup(uint8_t * frameData)
{
  LCDSetTextPos(0,2);
  printf("%02x %02x %02x", frameData[0], frameData[1], frameData[2]);

  LCDSetTextPos(0,3);
  printf("%02x %02x %02x", frameData[3], frameData[4], frameData[5]);
  
  LCDSetTextPos(0,4);
  printf("%02x %02x %02x", frameData[6], frameData[7], frameData[8]);

  LCDSetTextPos(0,5);
  printf("%02x %02x %02x", frameData[9], frameData[10], frameData[11]);
}


void ProcessGroup20(uint8_t * frameData)
{
  const uint8_t ignRetXPos[4] = {1,13,1,13};
  const uint8_t ignRetYPos[4] = {3,3,4,4};

  int i;
  float f;

  for (i = 0; i < 4; i++)
  {
    //34  (b-128)*0.01*a  	kW
    f = (frameData[(3*i)+2] - 128.0) * frameData[(3*i)+1];
    f *= 0.01;

    LCDSetTextPos(ignRetXPos[i], ignRetYPos[i]);
    printf("%d: ", i+1);
    if ((f > 0.1) || (f < -0.1))
      LCDSetNegative();
    printf("%4.1f%c", f, DEG_SYMBOL); 
    LCDSetNormal();
  }
}


void ProcessGroup15(uint8_t * frameData) //misfire
{
  const uint8_t ignRetXPos[4] = {2,7,12,17};
  const uint8_t ignRetYPos[4] = {7,7,7,7};

  int i;
  uint16_t val_u16;
  
  for (i = 0; i < 3; i++)
  {
    //54 	a*256+b  	Count
    val_u16 = frameData[(3*i)+1] * 256;
    val_u16 += frameData[(3*i)+2];
    if (val_u16 > 99)
      val_u16 = 99;

    LCDSetTextPos(ignRetXPos[i], ignRetYPos[i]);
    if ((val_u16 > 0) && (frameData[11] != 0x68))
      LCDSetNegative();
    printf("%2d", val_u16);
    LCDSetNormal();
  }
}


void ProcessGroup16(uint8_t * frameData) //misfire
{
  const uint8_t ignRetXPos[4] = {2,7,12,17};
  const uint8_t ignRetYPos[4] = {7,7,7,7};

  uint16_t val_u16;

  //54 	a*256+b  	Count
  val_u16 = frameData[1] * 256;
  val_u16 += frameData[2];
  if (val_u16 > 99)
    val_u16 = 99;

  LCDSetTextPos(ignRetXPos[3], ignRetYPos[3]);
  if ((val_u16 > 0) && (frameData[11] != 0x68))
    LCDSetNegative();
  printf("%2d", val_u16);
  LCDSetNormal();
}


void ecuDiag_ShowScreen(diagMode_t mode)
{
  LCDClear();
  LCDSetTextPos(0,0);

  switch (mode)
  {
    case DIAG_MODE_LAMBDA_BOOST:
      printf("%c / boost", LAMBDA_SYMBOL);

      LCDSetTextPos(0,3);
      LCDPrintChar('R');
      LCDSetTextPos(20,3);
      LCDPrintChar('L');

      ShowScale31();

      LCDSetTextPos(12,5);  
      printf("bar");

      LCDSetPosition(8,7);
      LCDPrintCharSmall('0');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('0');
      LCDSetPosition(28,7);
      LCDPrintCharSmall('0');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('2');
      LCDSetPosition(48,7);
      LCDPrintCharSmall('0');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('4');
      LCDSetPosition(68,7);
      LCDPrintCharSmall('0');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('6');
      LCDSetPosition(88,7);
      LCDPrintCharSmall('0');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('8');
      LCDSetPosition(108,7);
      LCDPrintCharSmall('1');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('0');

      break;

    case DIAG_MODE_AFR_BOOST:
      printf("AFR / boost");

      LCDSetTextPos(0,3);
      LCDPrintChar('L');
      LCDSetTextPos(20,3);
      LCDPrintChar('R');

      ShowScale33();
/*
      LCDSetPosition(56,4);
      LCDPrintCharSmall('1');
      LCDWriteData(0x00);
      LCDPrintCharSmall('4');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('7');
*/
      LCDSetTextPos(11,2);
      printf("mV");

      //LCDSetTextPos(14,5);
      //printf("mbar");

      LCDSetPosition(8,7);
      LCDPrintCharSmall('0');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('0');
      LCDSetPosition(28,7);
      LCDPrintCharSmall('0');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('2');
      LCDSetPosition(48,7);
      LCDPrintCharSmall('0');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('4');
      LCDSetPosition(68,7);
      LCDPrintCharSmall('0');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('6');
      LCDSetPosition(88,7);
      LCDPrintCharSmall('0');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('8');
      LCDSetPosition(108,7);
      LCDPrintCharSmall('1');
      LCDPrintCharSmall('.');
      LCDPrintCharSmall('0');
      break;
    
    case DIAG_MODE_DIAG:
      printf("Diag");
      LCDSetTextPos(0,2);
      printf("RPM");
      LCDSetTextPos(11,2);
      printf("Load");
      LCDSetTextPos(20,2);
      printf("%%");
      LCDSetTextPos(0,3);
      printf("Inj");
      LCDSetTextPos(10,3);
      printf("ms");
      maxInjVal = 0;
      maxInjCnt = 0;
      LCDSetTextPos(0,4);
      printf("MAF");
      LCDSetTextPos(10,4);
      printf("g/s");
      maxMAFVal = 0;
      maxMAFCnt = 0;

      LCDSetTextPos(0,5);
      printf("Supply");
      LCDSetTextPos(14,5);
      printf("V");
      LCDSetTextPos(0,6);
      printf("Water");
      LCDSetTextPos(13,6);
      printf("Air");
      LCDSetTextPos(0,7);
      printf("Add");
      LCDSetTextPos(10,7);
      printf("%%");
      LCDSetTextPos(12,7);
      printf("*");
      LCDSetTextPos(20,7);

      printf("%%");
      break;

    case DIAG_MODE_IGNITION:
      printf("Ignition"); 
  
      LCDSetTextPos(0, 2);
      printf("Timing retard:");
               
      LCDSetTextPos(0, 6);
      printf("Misfire:");      
      break;

    case DIAG_MODE_LOGGER:
      printf("VAGlogger"); 
      break;

    default:
      break;
  }

  if ((mode != DIAG_MODE_AFR_GRAPH) ||
      (mode != DIAG_MODE_LAMBDA_GRAPH))
  {
    LCDSetTextPos(0,1);
    LCDPrintLine(0x02,128);
  }
}


uint8_t VAGlogger()
{
  KW1281struct_t frame;
  KWState_t KWStatus;
  portBASE_TYPE xStatus;
  portTickType xWaitTime = 20 / portTICK_RATE_MS;
  uint8_t idleCnt = 0;
  static uint8_t selectedConfig = 0;
  uint8_t updateScreen = 1;
  uint8_t i;
  int file_number = 0;
  FIL log_file;
  int result = 0;
  char st[12];
  uint8_t logSaved = 0;
  static uint8_t fuelTypeLPG = 1;

  UartPrint("VAGlogger(): started\n");
  buttonRightState = 0;
  buttonLeftState = 0;

  xStatus = xQueueReceive(xKW1281OutputQueue, &frame, 1000 / portTICK_RATE_MS);

  if (xStatus == pdTRUE)
  {
    vPortFree(frame.data);
  }
  else
  {
    UartPrint("VAGlogger(): warning, frame not received\n");
    //return 4; //error
  }

  memset(config, 0, 12);

  if (!ReadConfig())
  {
    UartPrint("VAGlogger(): Cannot read config.txt, default config\n");
    config[0][0] = 3;
    config[0][1] = 3;
    config[0][2] = 115;
    config[0][3] = 118;

    config[1][0] = 3;
    config[1][1] = 2;
    config[1][2] = 31;
    config[1][3] = 20;

    config[2][0] = 2;
    config[2][1] = 3;
    config[2][2] = 31;
    config[2][3] = 0;
  }

  if (memcmp(config, config_prev, 12))
  {
    selectedConfig = 0;
    memmove(config_prev, config, 12);
  }

  while (1 != buttonRightState)
  {
    if (updateScreen)
    {
      LCDSetPosition(2, 2);
      LCDWriteData(0b11110000);
      for (i = 0; i<120; i++)
        LCDWriteData(0b00010000);
      LCDWriteData(0b11110000);

      LCDSetPosition(2, 4);
      LCDWriteData(0b00000111);
      for (i = 0; i<120; i++)
        LCDWriteData(0b00000100);
      LCDWriteData(0b00000111);

      LCDSetPosition(2, 3);
      LCDWriteData(0b11111111);

      LCDSetPosition(123, 3);
      LCDWriteData(0b11111111);

      LCDSetPosition(26, 2);
      LCDWriteData(0b11110000);
      LCDSetPosition(26, 3);
      LCDWriteData(0b11111111);
      LCDSetPosition(26, 4);
      LCDWriteData(0b00000111);


      LCDSetTextPos(2,3);
      printf("%d", selectedConfig+1);

      LCDSetTextPos(5,3);
      for (i=1; i<=3; i++)
      {
        if (i <= config[selectedConfig][0])
          printf(" %3d ", config[selectedConfig][i]);
        else
          printf("     ");
      }

      LCDSetTextPos(0,5);
      printf("     select set       ");

      if (logSaved)
      {
        LCDSetTextPos(0,7);
        printf("   saved as ");
        printf("%c%c%c%s", '0' + file_number/100, '0' + (file_number%100)/10, '0' + file_number%10, ext);
        logSaved = 0;
      }
      else
      {
        LCDSetTextPos(0,7);
        printf("                   ");
      }

      LCDSetTextPos(16,0);
      printf("%s", fuelTypeLPG?"LPG":"Pb ");

      updateScreen = 0;
    }

    if (!cardStatus.initialized)
    {
      UartPrint("Memory card not initialized\n");
      buttonRightState = 1;
      continue;
    }

    if (2 == buttonRightState)
    {
      fuelTypeLPG = !fuelTypeLPG;
      updateScreen = 1;
      buttonRightState = 0;
    }

    if (2 == buttonLeftState)
    {
      if ((selectedConfig < 2) && (config[selectedConfig+1][0] > 0))
        selectedConfig += 1;
      else
        selectedConfig = 0;

      updateScreen = 1;
      buttonLeftState = 0;
    }

    if (1 == buttonLeftState)
    {
      buttonLeftState = 0;
      updateScreen = 1;


      file_number = GetNextFileNumber();
      if (!file_number)
      {
        LCDSetTextPos(0,5);
        printf("  memory card error   ");
        vTaskDelay(2000 / portTICK_RATE_MS);
        UartPrint("VAGlogger(): cannot get next file number\n");
        continue;
      }
      
      result = CreateLogFile(file_number, &log_file);
      if (!result)
      {
        LCDSetTextPos(0,5);
        printf("  memory card error   ");
        vTaskDelay(2000 / portTICK_RATE_MS);
        UartPrint("VAGlogger(): cannot create log file\n");
        continue;
      }

      //write header to log file
      if (fuelTypeLPG)
        result = f_puts("Fuel type: LPG\n\n", &log_file);
      else
        result = f_puts("Fuel type: Pb\n\n", &log_file);

      if (result < 0)
      {
        LCDSetTextPos(0,5);
        printf("  memory card error   ");
        vTaskDelay(2000 / portTICK_RATE_MS);
        UartPrint("VAGlogger(): cannot write fuel type\n");
        continue;
      }

      LCDSetTextPos(0,5);
      printf("logging in progress...");

      LCDSetTextPos(0,7);
      printf("  saving to ");
      printf("%c%c%c%s", '0' + file_number/100, '0' + (file_number%100)/10, '0' + file_number%10, ext);

      result = kw1281_logger(&config[selectedConfig][0], &log_file);
      ClearProgress();
      if (result)
      {
        UartPrint("VAGlogger(): kw1281_logger() returned with error = ");
        sprintf(st, "%d\n", result);
        UartPrint(st);
        LCDSetTextPos(0,5);
        if (1 == result)
          printf(" communication error  ");
        else if (2 == result)
          printf("  memory card error   ");
        vTaskDelay(2000 / portTICK_RATE_MS);
      }

      result = CloseLogFile(&log_file);
      if (!result)
      {
        UartPrint("VAGlogger(): cannot close log file\n");
        continue;
      }

      logSaved = 1;
    }

    frame.title = 0xff;

    xStatus = xQueueReceive(xKW1281OutputQueue, &frame, xWaitTime);
    if (xStatus == pdFALSE)
    {
      KWStatus = KWGetStatus();
      if (KW_WORK == KWStatus)
      {
        continue;
      }
      else
      {
        LCDSetTextPos(0,5);
        printf(" communication error  ");
        vTaskDelay(2000 / portTICK_RATE_MS);
        if (KW_ERROR == KWStatus)
          return 1; //error
        else if (KW_DISCONNECTED == KWStatus)
          return 2; //error
        else if (KW_IDLE == KWStatus)
          return 3; //error
      }
    }
    else
    {
      UartPrint("unexpected kw1281 frame\n");
      vPortFree(frame.data);
    }
  }

  return 0;
}



uint8_t ecuDiag_kw() //0 - disconnected by user
{
  KW1281struct_t frame;
  uint8_t groupNumber;
  uint8_t requestedGroup;
  uint8_t sendRequest = 0;
  uint8_t cnt = 0;
  uint8_t displayModeChanged = 0;
  diagMode_t mode = FIRST_DIAG_MODE;
  uint8_t clearGraph;
  uint8_t sendEOS = 0; //KW1281_END_OF_SESSION
  KWState_t KWStatus;
  portBASE_TYPE xStatus;
  portTickType xWaitTime = 20 / portTICK_RATE_MS;
  uint8_t idleCnt = 0;
  uint8_t vagloggerStatus = 0;
  char st[12];

  buttonRightState = 0;

  UartPrint("ecuDiag_kw(): started\n");

  switch (mode)
  {
    case DIAG_MODE_LAMBDA_BOOST:
      groupNumber = 31;
      break;

    case DIAG_MODE_AFR_BOOST:
      groupNumber = 33;
      break;
                    
    case DIAG_MODE_DIAG:
      groupNumber = 2;
      break;
          
    case DIAG_MODE_IGNITION:
      groupNumber = 20;
      break;

    case DIAG_MODE_AFR_GRAPH:
      groupNumber = 33;
      clearGraph = 1;
      break;

    case DIAG_MODE_LAMBDA_GRAPH:
      groupNumber = 31;
      clearGraph = 1;
      break;

    case DIAG_MODE_LOGGER:
      groupNumber = 0;
      break;
  }    

  //empty queues
  xStatus = xQueueReceive(xKW1281OutputQueue, &frame, 0);
  xStatus = xQueueReceive(xKW1281OutputQueue, &frame, 0);
  xStatus = xQueueReceive(xKW1281InputQueue, &frame, 0);
  xStatus = xQueueReceive(xKW1281InputQueue, &frame, 0);
  
  KWStart();

  do // petla po nawiazaniu polaczenia z ECU
  {
    if (1 == buttonRightState) //next mode
    {
      switch (mode) 
      {
        case DIAG_MODE_LAMBDA_BOOST:
          mode = DIAG_MODE_DIAG;
          break;

        case DIAG_MODE_AFR_BOOST:
          mode = DIAG_MODE_DIAG;
          break;

        case DIAG_MODE_DIAG:
          mode = DIAG_MODE_IGNITION;
          break;

        case DIAG_MODE_IGNITION:
          //mode = DIAG_MODE_LAMBDA_GRAPH;
          if (cardStatus.initialized)
            mode = DIAG_MODE_LOGGER;
          else
            mode = FIRST_DIAG_MODE;
          break;

        case DIAG_MODE_LAMBDA_GRAPH:
          if (cardStatus.initialized)
            mode = DIAG_MODE_LOGGER;
          else
            mode = FIRST_DIAG_MODE;
          break;

        case DIAG_MODE_LOGGER:
          mode = FIRST_DIAG_MODE;
          break;

        default:
          break;
      }

      ecuDiag_ShowScreen(mode);

      switch (mode)
      {
        case DIAG_MODE_LAMBDA_BOOST:
          groupNumber = 31;
          break;
                    
        case DIAG_MODE_DIAG:
          groupNumber = 2;
          break;
          
        case DIAG_MODE_IGNITION:
          groupNumber = 20;
          break;

        case DIAG_MODE_AFR_GRAPH:
          groupNumber = 33;
          clearGraph = 1;
          break;

        case DIAG_MODE_LAMBDA_GRAPH:
          groupNumber = 31;
          clearGraph = 1;
          break;

        case DIAG_MODE_LOGGER:
          vagloggerStatus = VAGlogger();
          if (0 == vagloggerStatus)
          {
            buttonRightState = 1; //switch to next mode
            requestedGroup = 0;
            continue;
          }
          else
          {
            UartPrint("ecuDiag_kw(): VAGlogger() returned error = ");
            sprintf(st, "%d\n", vagloggerStatus);
            UartPrint(st);
            KWDisconnect();
            break;
          }
      }    

      displayModeChanged = 1;
      buttonRightState = 0;
    }
    else if (2 == buttonRightState)
    {
      UartPrint("ecuDiag_kw(): disconnect requested by user\n");
      KWDisconnect();
      sendEOS = 1;
      buttonRightState = 0;
      break;
    }

    frame.title = 0xff;

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
          ecuDiag_ShowScreen(mode);
        }
        else
          continue;
      }
      else if (KW_ERROR == KWStatus)
      {
        UartPrint("ecuDiag_kw(): KW_ERROR\n");
        break; //error
      }
      else if (KW_DISCONNECTED == KWStatus)
      {
        UartPrint("ecuDiag_kw(): KW_DISCONNECTED\n");
        sendEOS = 1;
        break; //error
      }
      else if (KW_IDLE == KWStatus)
      {
        idleCnt++;
        if (idleCnt > 200) //kw_task in idle state longer than 4 seconds
        {
          UartPrint("ecuDiag_kw(): KW_IDLE longer than 4 sec\n");
          break; //error
        }
      }
    }

    if (KW1281_GROUP_RESP == frame.title)
    {
      if (displayModeChanged)
      {
        displayModeChanged = 0;
      }
      else switch (requestedGroup)
      {
        case 32: //corr
          ProcessGroup32(frame.data);
          break;
          
        case 115: //pressure
          ProcessGroup115(frame.data);
          break;

        case 2:  //diag
        case 4:
          ProcessGroup2_4(requestedGroup, frame.data);
          break;
        
        case 91:
          ShowDebugGroup(frame.data);
          break;
        
        case 31:  //lambda
          if (DIAG_MODE_LAMBDA_GRAPH == mode)
            ProcessGroup31Graph(frame.data, &clearGraph);
          else
            ProcessGroup31(frame.data);
          break;

        case 20: //ignition retard
          ProcessGroup20(frame.data);
          break;

        case 15: //misfire
          ProcessGroup15(frame.data);
          break;

        case 16: //misfire
          ProcessGroup16(frame.data);
          break;
      }

      sendRequest = 1;
    }
    else if (KW1281_NO_ACK == frame.title)
      sendRequest = 1;

    if (xStatus == pdTRUE)
      vPortFree(frame.data);

    if ((sendRequest) && (groupNumber != 0))
    {
      frame.len = 4; //3
      frame.title = KW1281_GROUP_REQ; //KW1281_ERRORS_REQ
      frame.data = pvPortMalloc(frame.len-3);
      frame.data[0] = groupNumber;
      requestedGroup = groupNumber;
      xStatus = xQueueSendToBack(xKW1281InputQueue, &frame, 0);

      switch (mode)
      {
        case DIAG_MODE_AFR_BOOST: //afr + pressure
          groupNumber = (33 == groupNumber)?115:33;
          break;

        case DIAG_MODE_LAMBDA_BOOST:
          groupNumber = (31 == groupNumber)?115:31;
          //groupNumber = 31;
          break;

        case DIAG_MODE_DIAG: //diag
          if (2 == groupNumber)
            groupNumber = 4;
          else if (4 == groupNumber)
            groupNumber = 32;
          else
            groupNumber = 2;
          break;

        case DIAG_MODE_IGNITION: //ignition diag
          if (20 == groupNumber)
            groupNumber = 15;
          else if (15 == groupNumber)
            groupNumber = 16;
          else
            groupNumber = 20;
          break;

        case DIAG_MODE_AFR_GRAPH: //afr graph
        case DIAG_MODE_LAMBDA_GRAPH: //lambda graph
          break;
      }
    }
  } while (1);

  KWDisconnect();

  if (sendEOS)
    return 0;
  else
    return 255;
}

