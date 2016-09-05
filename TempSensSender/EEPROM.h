#ifndef TSS_EEPROM_H
#define TSS_EEPROM_H
#include <stc15.h>

unsigned char IapReadByte(unsigned int addr);
unsigned short IapReadShortBig(unsigned int addr);
void IapIdle();
#ifdef EEPROM_WRITE
void IapProgramByte(unsigned int addr, unsigned char dat);
void IapEreaseSector(unsigned int addr);
void IapProgramData(unsigned int addr, unsigned char *buf, unsigned int len);
void IapProgramShortBig(unsigned int addr, unsigned short dat);
#endif

#endif