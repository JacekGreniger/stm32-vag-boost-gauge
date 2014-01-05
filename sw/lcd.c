#include "stm32f10x.h"
#include "lcd.h"
#include "font5x8.h"
#include "font3x5.h"
#include "font16x16.h"
#include "font8x16.h"

uint8_t lcdNegative = 0;

void LCDInit()
{
  SPI_InitTypeDef   SPI_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;  
  int i;
  
  // GPIO configuration
  GPIO_InitStructure.GPIO_Pin = LCD_CD | LCD_CS | LCD_RST;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
#ifdef LCD_H103
  GPIO_Init(GPIOC, &GPIO_InitStructure);
#else
  GPIO_Init(GPIOB, &GPIO_InitStructure);
#endif

  /* Configure SPI2 pins*/
  GPIO_InitStructure.GPIO_Pin = LCD_SCK | LCD_MOSI;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  GPIO_SetBits(LCD_CS_PORT, LCD_CS);
  
  /* SPI2 Config -------------------------------------------------------------*/
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_Init(SPI2, &SPI_InitStructure);

  /* Enable SPI2 */
  SPI_Cmd(SPI2, ENABLE);

  // LCD configuration
  GPIO_ResetBits(LCD_RST_PORT, LCD_RST);
  longdelay(100);
  GPIO_SetBits(LCD_RST_PORT, LCD_RST);
  longdelay(100);

  LCDWriteCommand(0x27); //5. temp compensation
  LCDWriteCommand(0xC4); //17. lcd mapping
  LCDWriteCommand(0xA0); //13. frame rate
  LCDWriteCommand(0xEB); //bias ratio
  LCDWriteCommand(0x81); //10. potentiometer
  LCDWriteCommand(0x60); //8. set scroll line
  LCDWriteCommand(0xAF); //16. display enable
  
  for (i=0; i<1188; i++)
  {
    LCDWriteData(0);
  }
}

void LCDWriteData(u8 data)
{
  GPIO_ResetBits(LCD_CS_PORT, LCD_CS);
  GPIO_SetBits(LCD_CD_PORT, LCD_CD); //CD=1, data
  delay(10);

  /* Send SPI2 data */
  SPI_I2S_SendData(SPI2, data);
  /* Wait for SPI2 Tx buffer empty */
  while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);

  GPIO_SetBits(LCD_CS_PORT, LCD_CS);
  delay(10);
}

void LCDWriteDataBlock(u8 *data, u16 size)
{
  GPIO_ResetBits(LCD_CS_PORT, LCD_CS);
  GPIO_SetBits(LCD_CD_PORT, LCD_CD); //CD=1, data
  while (size > 0)
  {
    /* Send SPI2 data */
    SPI_I2S_SendData(SPI2, *data);
    /* Wait for SPI2 Tx buffer empty */
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    data++;
    size--;
  }
  GPIO_SetBits(LCD_CS_PORT, LCD_CS);
}

void LCDWriteCommand(u8 data)
{
  GPIO_ResetBits(LCD_CS_PORT, LCD_CS);
  GPIO_ResetBits(LCD_CD_PORT, LCD_CD); //CD=0,command
  delay(10);
  /* Send SPI2 data */
  SPI_I2S_SendData(SPI2, data);
  /* Wait for SPI2 Tx buffer empty */
  while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);

  GPIO_SetBits(LCD_CS_PORT, LCD_CS);
  delay(10);
}

void LCDSetPosition(u8 x, u8 y)
{
  LCDWriteCommand(0xB0 | y);
  LCDWriteCommand(x & 0x0f);
  LCDWriteCommand(0x10 | (x>>4));
}


// xmax = 20  ymax = 7
void LCDSetTextPos(u8 x, u8 y) 
{
  x *= 6;
  //x += 4;

  LCDWriteCommand(0xB0 | y);
  LCDWriteCommand(x & 0x0f);
  LCDWriteCommand(0x10 | (x>>4));
}


void LCDClear()
{
  int i;
  for (i=0; i<1188; i++)
  {
    LCDWriteData(0);
  }
}


void LCDShowBitmap(u8 * bitmap)
{
  int y = 0;
  
  for (y=0; y<8; y++)
  {
    LCDSetPosition(0,y);
    LCDDMATransfer(bitmap, 128);
    bitmap += 128;
  }
}

DMA_InitTypeDef    DMA_InitStructure;
#define SPI2_DR_Address    0x4000380C

void LCDPrepareDMA()
{
  /* DMA1 Channel5 configuration ----------------------------------------------*/
  DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)SPI2_DR_Address;
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST; //DMA_DIR_PeripheralSRC DMA_DIR_PeripheralDST
  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
  DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
  DMA_InitStructure.DMA_Priority = DMA_Priority_High;
  DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
}


void LCDDMATransfer(u8 * p, u32 len)
{
  GPIO_ResetBits(LCD_CS_PORT, LCD_CS);
  GPIO_SetBits(LCD_CD_PORT, LCD_CD); //CD=1, data

  /* DMA1 Channel5 configuration ----------------------------------------------*/
  DMA_DeInit(DMA1_Channel5);  
  DMA_InitStructure.DMA_MemoryBaseAddr = (u32)p;
  DMA_InitStructure.DMA_BufferSize = len;
  DMA_Init(DMA1_Channel5, &DMA_InitStructure);

  /* Enable SPI2 DMA Tx request */
  SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);   

  /* Enable DMA1 channel5, channel4, channel3 and channel2 */
  DMA_Cmd(DMA1_Channel5, ENABLE);
   
  /* Transfer complete */
  while(!DMA_GetFlagStatus(DMA1_FLAG_TC5));

  DMA_Cmd(DMA1_Channel5, DISABLE);
  
  GPIO_SetBits(LCD_CS_PORT, LCD_CS);
}


void LCDPrintString (u8 * str)
{
  u8 c = *str;
  u8 * font_p;
  int i;
  
  while (c > 0)
  {
    font_p = (u8*)font5x8;
    font_p += 5 * (c - FIRST_CHAR_CODE);
    for (i=0; i<5; i++)
    {
      LCDWriteData(*font_p);
      font_p++;
    }

    str++;
    c = *str;
    if (c != 0)
    {
      LCDWriteData(0x00);
    }
  }
}


void LCDPrintChar (u8 c)
{
  u8 * font_p;
  uint8_t d;
  int i;
  
  font_p = (u8*)font5x8;
  font_p += 5 * (c - FIRST_CHAR_CODE);
  for (i=0; i<5; i++)
  {
    d = *font_p++;
    if (lcdNegative)
      d = ~d;
    LCDWriteData(d);
  }

  if (lcdNegative)
    LCDWriteData(0xFF);
  else
    LCDWriteData(0x00);
}


void LCDPrintLine(u8 val, u8 len)
{
  u8 i;
  for(i=0; i<len; i++)
  {
    LCDWriteData(val);
  }
}

void LCDPrintCharSmall(u8 c)
{
  u8 * font_p;
  int i;

  if ((c >= '0') && (c <= '9'))
  {
    c = c - '0';
  }
  else if (c == '.')
  {
    c = 10;
  }
  else 
  {
    c = 11;
  }

  font_p = (u8*)font3x5;
  font_p += 3*c ;
  for (i=0; i<3; i++)
  {
    LCDWriteData(*font_p >> 2);
    font_p++;
  }
}

void BufferPrintCharSmall(u8* buf_p, u8 c)
{
  u8 * font_p;
  int i;

  if ((c >= '0') && (c <= '9'))
  {
    c = c - '0';
  }
  else if (c == '.')
  {
    c = 10;
  }
  else 
  {
    c = 11;
  }

  font_p = (u8*)font3x5;
  font_p += 3*c ;
  for (i=0; i<3; i++)
  {
    *buf_p |= (*font_p >> 2);
    buf_p++;
    font_p++;
  }
}

void LCDPrintString12x16(u8 textPosX, u8 textPosY, u8 * str)
{
  u8 c;
  u8 * str1 = str;
  u8 * str2 = str;
  u8 * font_p;
  int i;
  u8 longSpace = 1;

  //LCDSetTextPos(textPosX, textPosY); 
  LCDSetPosition(textPosX, textPosY);
  c = *str1;
  while (c > 0)
  {
    font_p = (u8*)font16x16;
    font_p += 32 * (c - FIRST_CHAR_CODE);
    font_p += 2;
//    if (c != ' ')
//      longSpace = 0;
    for (i=2; i<(((c!=' ') | longSpace)?14:6); i++)
    {
      LCDWriteData(*font_p);
      font_p++;
    }

    str1++;
    c = *str1;
  }

  longSpace = 1;
//  LCDSetTextPos(textPosX, textPosY+1);
  LCDSetPosition(textPosX, textPosY+1);
  c = *str2;
  while (c > 0)
  {
    font_p = (u8*)font16x16;
    font_p += 32 * (c - FIRST_CHAR_CODE);
    font_p += 16;
    font_p += 2;
//    if (c != ' ')
//      longSpace = 0;
    for (i=2; i<(((c!=' ') | longSpace)?14:6); i++)
    {
      LCDWriteData(*font_p);
      font_p++;
    }

    str2++;
    c = *str2;
  }
}

void LCDPrintString8x16(u8 textPosX, u8 textPosY, u8 * str)
{
  u8 c;
  u8 * str1 = str;
  u8 * str2 = str;
  u8 * font_p;
  int i;
  u8 longSpace = 1;

  //LCDSetTextPos(textPosX, textPosY); 
  LCDSetPosition(textPosX, textPosY);
  c = *str1;
  while (c > 0)
  {
    font_p = (u8*)font8x16;
    font_p += 32 * (c - FIRST_CHAR_CODE);
    font_p += 2;
    if (c != ' ')
      longSpace = 0;
    for (i=2; i<(((c!=' ') | longSpace)?14:6); i++)
    {
      LCDWriteData(*font_p);
      font_p++;
    }

    str1++;
    c = *str1;
  }

  longSpace = 1;
//  LCDSetTextPos(textPosX, textPosY+1);
  LCDSetPosition(textPosX, textPosY+1);
  c = *str2;
  while (c > 0)
  {
    font_p = (u8*)font8x16;
    font_p += 32 * (c - FIRST_CHAR_CODE);
    font_p += 16;
    font_p += 2;
    if (c != ' ')
      longSpace = 0;
    for (i=2; i<(((c!=' ') | longSpace)?14:6); i++)
    {
      LCDWriteData(*font_p);
      font_p++;
    }

    str2++;
    c = *str2;
  }
}

void LCDSetNormal()
{
  lcdNegative = 0;
}

void LCDSetNegative()
{
  lcdNegative = 1;
}
