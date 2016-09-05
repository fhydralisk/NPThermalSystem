#include <intrins.h>
#include "EEPROM.h"
#ifdef DEBUG
#include "uart.h"
#endif
#define ENABLE_IAP 0x83
#define CMD_IDLE 0
#define CMD_READ 1
#define CMD_PROGRAM 2
#define CMD_ERASE 3

void IapIdle() {
	IAP_CONTR = 0;
	IAP_CMD = 0;
	IAP_TRIG = 0;
	IAP_ADDRH = 0x80;
	IAP_ADDRL = 0;
}

unsigned char IapReadByte(unsigned int addr) {
	unsigned char dat;
	IAP_CONTR = ENABLE_IAP;
	IAP_CMD = CMD_READ;
	IAP_ADDRL = addr;
	IAP_ADDRH = addr >> 8;
	IAP_TRIG = 0x5a;
	IAP_TRIG = 0xa5;
	_nop_();
	dat = IAP_DATA;
	IapIdle();

	return dat;
}

unsigned short IapReadShortBig(unsigned int addr) {
	unsigned short ret;
	ret = ( IapReadByte(addr) << 8 ) + IapReadByte(addr+1);
	return ret;
}

#ifdef EEPROM_WRITE
void IapProgramByte(unsigned int addr, unsigned char dat){
	IAP_CONTR = ENABLE_IAP;
	IAP_CMD = CMD_PROGRAM;
	IAP_ADDRL = addr;
	IAP_ADDRH = addr >> 8;
	IAP_DATA = dat;
	IAP_TRIG = 0x5a;
	IAP_TRIG = 0xa5;
	_nop_();
	IapIdle(); 
}
void IapEreaseSector(unsigned int addr){
	IAP_CONTR = ENABLE_IAP;
	IAP_CMD = CMD_ERASE;
	IAP_ADDRL = addr;
	IAP_ADDRH = addr >> 8;
	IAP_TRIG = 0x5a;
	IAP_TRIG = 0xa5;
	_nop_();
	IapIdle();
}
void IapProgramData(unsigned int addr, unsigned char *buf, unsigned int len) {
	unsigned int i;
	for (i=0; i<len; i++) 
		IapProgramByte(addr+i, buf[i]);
}
void IapProgramShortBig(unsigned int addr, unsigned short dat) {
	IapProgramByte(addr, dat >> 8);
	IapProgramByte(addr + 1, dat);
}
#endif