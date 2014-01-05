#ifndef IO_THREAD_H
#define IO_THREAD_H

typedef struct
{
  uint8_t inserted;
  uint8_t initialized;
} cardStatus_t;

extern cardStatus_t cardStatus;

extern uint8_t buttonRightState;
extern uint8_t buttonLeftState;

extern FATFS Fatfs[_DRIVES];    // File system object for each logical drive 

void IOConfigure();
void IOTask(void *pvParameters);

#endif

