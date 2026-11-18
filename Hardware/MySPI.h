#ifndef __MYSPI_H
#define __MYSPI_H
void MySPI_Init(void);
void MySPI_Start(void);
void MySPI_Stop(void);
uint8_t MySPI_Swap(uint8_t sendByte);
#endif
