#include "integer.h"
#include "fattime.h"
#include "time.h"

DWORD get_fattime (void)
{
  DWORD res;

  res =  (((DWORD)2012 - 2000) << 25)
      | ((DWORD)(5+1) << 21)
      | ((DWORD)1 << 16)
      | (WORD)(15 << 11)
      | (WORD)(18 << 5)
      | (WORD)(00 >> 1);

  return res;
}

