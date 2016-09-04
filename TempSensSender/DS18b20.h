#ifndef TSS_DS18B20_H
#define TSS_DS18B20_H

#include <stc15.h>

sbit DQ = P3^2;
typedef unsigned char u8;
typedef unsigned short u16;

char ds18b20_init();
void ds18b20_writeByte(u8 dat);
unsigned char ds18b20_readByte();
u16 ds18b20_readTemperaData();
char read_temp(short temp, float *d_temp);

#endif