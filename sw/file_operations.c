#include "stm32f10x.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "ctype.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"

#include "filesystem/integer.h"
#include "filesystem/diskio.h"
#include "filesystem/ff.h"

#include "file_operations.h"
#include "kw1281.h"

uint8_t config[3][4];
const char * path  = "";
const char * ext = ".CSV";
extern FIL log_file2;


//returns: 0 - error
//returns 1-999 - first free file number
int GetNextFileNumber ()
{
  FRESULT res;
  FILINFO fno;
  DIR dir;
  int file_number = 1;
  int i;
  char *fn;

  res = f_opendir(&dir, path);
  if (res != FR_OK) return 0; //filesystem error
  
  for (;;) 
  {
    res = f_readdir(&dir, &fno);
    if (res != FR_OK) { return 0; } //filesystem error
    if (fno.fname[0] == 0) { break; } //no more files in this directory
  
    if ((fno.fname[0] == '.') || (fno.fattrib & AM_DIR)) { continue; }
    
    fn = strstr(fno.fname, ext);
    if (NULL == fn) { continue; } //matching extension not found in filename
    fn[0] = 0;
    if ((strlen(fno.fname) != 3) || (!isdigit(fno.fname[0])) || (!isdigit(fno.fname[1])) || (!isdigit(fno.fname[2]))) { continue; }
    
    i = atoi(fno.fname);
    if (i >= file_number) { file_number = i+1; }
    if (999 == i) { file_number = 0; break; }
  }

  return file_number;
}


int CreateLogFile(int file_number, FIL *log_file)
{
  FRESULT res;
  char s[12];
  
  sprintf(s, "%c%c%c%s", '0' + file_number/100, '0' + (file_number%100)/10, '0' + file_number%10, ext);
  
  res = f_open(log_file, s, FA_CREATE_ALWAYS | FA_WRITE);
  if (res != FR_OK) { return 0; }
  
  return 1;
}


int CloseLogFile(FIL * log_file)
{
  FRESULT res;
  res = f_close(log_file);
  if (res != FR_OK) { return 0; }
  
  return 1; 
}


const char * config_filename = "config.txt";
const char * delimiters = " ,.:;\n\r";

int ReadConfig()
{
  FIL config_file;
  FRESULT res;
  int i, j, count, val;
  uint8_t textbuf[50];
  uint8_t * s;

  memset(config, sizeof(config), 0);
  
  res = f_open(&config_file, config_filename, FA_OPEN_EXISTING | FA_READ);
  if (res != FR_OK) { return 0; }

  i = 0;
  s = f_gets(textbuf, sizeof(textbuf), &config_file);
  if (NULL == s)
  {
    f_close(&config_file);
    return 0;
  }

  do
  {
    s = strtok(textbuf, delimiters);
    j = 0;
    while ((s!=NULL) && (j<4))
    {
      val = atoi(s);
      if ((val > 0) && (val <= 255))
      {
        j++;
        config[i][0] = j;
        config[i][j] = val;
      }
      else
      {
        break; //ignore this line
      }
      
      s = strtok(NULL, delimiters);
    }
    if (j > 0) { i++; }
    
    s = f_gets(textbuf, sizeof(textbuf), &config_file);
  } while ((s != NULL) && (i < 3));

  res = f_close(&config_file);
  if (res != FR_OK) { return 0; }
  
  return 1;
}

