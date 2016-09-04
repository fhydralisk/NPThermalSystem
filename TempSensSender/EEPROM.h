#ifndef TSS_EEPROM_H
#define TSS_EEPROM_H
#include <stc15.h>

unsigned char IapReadByte(unsigned int addr);
void IapIdle();
#ifdef EEPROM_WRITE
void IapProgramByte(unsigned int addr, unsigned char dat);
void IapEreaseSector(unsigned int addr);
#endif

#endif