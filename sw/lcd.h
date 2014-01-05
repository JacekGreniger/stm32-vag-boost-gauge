#ifndef LCD_H
#define LCD_H

#define LCD_CD_PORT GPIOB
#define LCD_CD GPIO_Pin_12

#define LCD_CS_PORT GPIOB
#define LCD_CS GPIO_Pin_10

#define LCD_RST_PORT GPIOB
#define LCD_RST GPIO_Pin_11

#define LCD_MOSI_PORT GPIOB
#define LCD_MOSI GPIO_Pin_15

#define LCD_SCK_PORT GPIOB
#define LCD_SCK GPIO_Pin_13

void LCDInit();
void LCDWriteData(u8 data);
void LCDWriteDataBlock(u8 *data, u16 size);
void LCDWriteCommand(u8 data);
void LCDSetPosition(u8 x, u8 y);
void LCDSetTextPos(u8 x, u8 y);
void LCDClear();
void LCDShowBitmap(u8 * bitmap);

void LCDDMATransfer(u8 * p, u32 len);
void LCDPrintString(u8 * str);
void LCDPrintChar(u8 c);
void LCDPrepareDMA();

void LCDPrintLine(u8 val, u8 len);
void LCDPrintCharSmall(u8 c);
void LCDPrintString12x16(u8 textPosX, u8 textPosY, u8 * str);

void LCDSetNormal();
void LCDSetNegative();

void BufferPrintCharSmall(u8* buf_p, u8 c);

#endif
