#ifndef KW_TASH_H
#define KW_TASK_H

extern xQueueHandle xKW1281InputQueue;
extern xQueueHandle xKW1281OutputQueue;

typedef enum
{
  KW_IDLE,
  KW_NO_REQ,
  KW_START_REQ,
  KW_WORK,
  KW_DISCONNECT_REQ,
  KW_ERROR,
  KW_DISCONNECTED
} KWState_t;

void KWTaskInit(uint8_t prio);
void KWStart();
void KWDisconnect();
KWState_t KWGetStatus();
void KWTaskInit(uint8_t prio);

#endif

