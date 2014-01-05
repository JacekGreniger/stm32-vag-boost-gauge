#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

extern uint8_t config[3][4];
extern const char * ext;

int ReadConfig();
int GetNextFileNumber();
int CreateLogFile(int file_number, FIL *log_file);
int CloseLogFile(FIL * log_file);

#endif
