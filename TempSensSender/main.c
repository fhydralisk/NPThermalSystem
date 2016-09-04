#include <stc15.h>
#include "uart.h"
#include "DS18b20.h"
#include "misc.h"
#include "Timer.h"
#include "EEPROM.h"

#define HB_DEAD 10

sbit LED_R = P1^2;
sbit LED_Y = P1^3;
sbit LED_G = P1^4;
sbit KEY_1 = P3^3;
sbit BEEP = P1^5;
sbit NTRST = P3^4;

unsigned char build_report_packet(char *packet, unsigned short td, unsigned short version, unsigned short devid) {
	unsigned char len = sizeof(version) + sizeof(devid) + sizeof(td);
	packet[0] = version >> 8;
	packet[1] = version;
	packet[2] = devid >> 8;
	packet[3] = devid;
	packet[4] = td >> 8;
	packet[5] = td;
	
	return len;	
}

void main() {
	unsigned short td;
	unsigned long i;
	unsigned short reset_counter;
	unsigned short ver;
	unsigned short devid;
	unsigned char packet[10];
	unsigned char packet_len;
	unsigned short hb_time = 32768;

	uart_init();
	timer0_init();
	EA = 1;
	LED_Y = 0;

	for (i=0;i<10000;i++)
		delayXus(100); // wait for stable v
	// LED_Y = 1;

	ver = IapReadByte(0x0000);
	ver = (ver << 8) + IapReadByte(0x0001);
	devid = IapReadByte(0x0002);
	devid = (devid << 8) + IapReadByte(0x0003);
	if (ver == 0xffff || devid == 0xffff || ver == 0x00 || devid == 0x00) {
		LED_R = 0;
		BEEP = 0;
		while (1);
	}
    while (1){
		td = ds18b20_readTemperaData();
		if (td == 0x0550 || td == 0x1000 || td == 0x2000) {
			
			LED_R = 0;
		} else {
			packet_len = build_report_packet(packet, td, ver, devid);
			uart_sendstring(packet, packet_len);
			LED_R = 1;
		}
		for (i=0; i<100000; i++) {
			if (KEY_1 == 0) {
				// RESET ALL
				for (reset_counter=0; reset_counter<5000; reset_counter++) {
					if (reset_counter % 1000 < 100) 
						LED_R = 0;
					else
						LED_R = 1;
					delayXus(1000);
					if (KEY_1 == 1) {
						//Abort
						LED_R = 1;
						break;
					}
				}

				if (reset_counter == 5000) {
					// RESTART
					LED_R = 0;
					BEEP = 0;
					NTRST = 0;
					for (reset_counter=0; reset_counter<100; reset_counter++)
						delayXus(10000);
					NTRST = 1;
					BEEP = 1;
					LED_R = 1;

					IAP_CONTR |= 0x20;
					while (1);
				}
			}
			if (uart_getbyte(packet) && packet[0] == 0x0F) {
				if (uart_getstring(packet, 3, 5000) == 3) {
					if (packet[0] == 0xF0 && (packet[1] << 8) + packet[2] == devid) {
						hb_time = 0;
						LED_Y = 1;
					}
				}
			} else {
				delayXus(40);
			}
		}
		hb_time++;
		if (hb_time > HB_DEAD) {
			LED_Y = 0;
			BEEP = 0;
			delayXus(10000);
			BEEP = 1;
		} 
		LED_G = ~LED_G;
	}
}