#include <stc15.h>
#include <string.h>
#include "uart.h"
#include "DS18b20.h"
#include "misc.h"
#include "Timer.h"
#include "EEPROM.h"
#include "esp8266.h"

#define HB_DEAD 10

sbit LED_R = P1^2;
sbit LED_Y = P1^3;
sbit LED_G = P1^4;
sbit KEY_1 = P3^3;
sbit BEEP = P1^5;
sbit NTRST = P3^4;

sbit urx = P3^0;
sbit utx = P3^1;

sbit urx2 = P3^6;
sbit utx2 = P3^7;
/*
unsigned char build_report_packet(char *packet, unsigned short td, unsigned short version, unsigned short devid) {
	unsigned char len = sizeof(version) + sizeof(devid) + sizeof(td);
	packet[0] = version >> 8;
	packet[1] = version;
	packet[2] = devid >> 8;
	packet[3] = devid;
	packet[4] = td >> 8;
	packet[5] = td;
	
	return len;	
}*/

#define SSID_LEN_MAX 15
#define PWD_LEN_MAX 15
#define HOST_LEN_MAX 30
#define PORT_LEN_MAX 6
#define SSID_START_POS 0x0200
#define HOST_START_POS 0x0400

#define HB_TIMEOUT 5
#define ALARM_BEEP_AGAIN 300

void set_output(unsigned char output, unsigned char state) {
	switch (output) {
		case 0:
			LED_G = state;
			break;
		case 1:
			LED_Y = state;
			break;
		case 2:
			LED_R = state;
			break;
		case 3:
			BEEP = state;
			break;
		default:
			break;
	}
}

void blink(unsigned char output, unsigned short delay_ms) {
	set_output(output, 0);
	for ( ; delay_ms>0; delay_ms--) {
		delayXus(1000);
	}
	set_output(output,1);
}

unsigned char get_profile(unsigned short *ver, unsigned short *devid, unsigned short *targetid) {
	*ver = IapReadByte(0x0000);
	*ver = (*ver << 8) + IapReadByte(0x0001);
	*devid = IapReadByte(0x0002);
	*devid = (*devid << 8) + IapReadByte(0x0003);
	*targetid = IapReadByte(0x0004);
	*targetid = (*targetid<<8) + IapReadByte(0x0005);

	if (*ver == 0xffff || *devid == 0xffff || *ver == 0x00 || *devid == 0x00) {
		return 0;
	}

	return 1;
}

void connect_wifi() {
	unsigned short i;
	unsigned char len_ssid, len_pwd;
	unsigned char xdata ssid[SSID_LEN_MAX];
	unsigned char xdata pwd[PWD_LEN_MAX];

	// get ssid and pwd first
	len_ssid = IapReadByte(SSID_START_POS);
	if (len_ssid >= SSID_LEN_MAX || len_ssid == 0) {
		// TODO: extra
		BEEP = 0;
		LED_R = 0;
		while(1);
	}
	for (i=0;i<len_ssid;i++)
		ssid[i] = IapReadByte(SSID_START_POS + i + 1);
	ssid[i] = 0;
	len_pwd = IapReadByte(SSID_START_POS + len_ssid + 1);
	if (len_pwd >= PWD_LEN_MAX || len_pwd == 0) {
		// TODO: extra
		BEEP = 0;
		LED_R = 0;
		while(1);
	}
	for (i=0;i<len_pwd; i++)
		pwd[i] = IapReadByte(SSID_START_POS + len_ssid + i + 2);
	pwd[i] = 0;

	while (esp8266_connect_wifi(ssid, pwd, 1) == 0); //try connect
}

void connect_server() {
	unsigned short i;
	unsigned char xdata host[HOST_LEN_MAX];
	unsigned char xdata port[PORT_LEN_MAX];
	unsigned char len_host, len_port;
	len_host = IapReadByte(HOST_START_POS);
	if (len_host >= HOST_LEN_MAX || len_host == 0) {
		// TODO: extra
		// BEEP = 0;
		LED_R = 0;
		while(1) {
			blink(3, 10);
			for (i=0;i<1000;i++) {
				delayXus(500);
			}
		}
	}
	for (i=0;i<len_host;i++)
		host[i] = IapReadByte(HOST_START_POS + i + 1);
	host[i] = 0;
	len_port = IapReadByte(HOST_START_POS + len_host + 1);
	if (len_port >= PORT_LEN_MAX || len_port == 0) {
		// TODO: extra
		// BEEP = 0;
		LED_R = 0;
		while(1) {
			blink(3, 10);
			for (i=0;i<1000;i++) {
				delayXus(2000);
			}
		}
	}
	for (i=0;i<len_port; i++)
		port[i] = IapReadByte(HOST_START_POS + len_host + i + 2);
	port[i] = 0;

	if (esp8266_connect_udp(host, port) == 0) {
		while(esp8266_connect_udp(host, port) == 0)
			LED_R = ~LED_R;
	}

	LED_R = 1;
}

void init_esp8266(unsigned char reboot) {
	unsigned short i;
	LED_G = 1; LED_R = 1; LED_Y = 1; BEEP = 1; NTRST = 1;

	if (reboot > 0) {
		LED_R = 0;
		if (reboot == 2) {
			NTRST = 0;
			
			for (i=0; i<10000; i++)
				delayXus(300);
			NTRST = 1;
		}
	
		if (reboot == 1) {
			esp8266_reset();
		}
		
		if (esp8266_wait_ready(1000000))
			LED_R = 1;
	}

	LED_Y = 0;

	if (esp8266_wait_gotip(1000000) == 0) {
		// not connected	
		connect_wifi();
	}

	if (reboot == 2) BEEP = 0;
	LED_Y = 1;
	if  (esp8266_wait_gotip(30000)) {
		if (reboot == 2) BEEP = 1;
		LED_G = 0;
	}

	// connect to server
	connect_server();
}

unsigned short construct_query_packet(unsigned char *buf, unsigned short ver, unsigned short devid, unsigned short targetid) {
	pack_short_to_charptr(buf, ver);
	pack_short_to_charptr(buf + 2, devid);
	pack_short_to_charptr(buf + 4, targetid);
	return 6;
}

unsigned char calc_crc(const unsigned char *buf, unsigned short len) {
	unsigned char ret = 0;
	while (len > 0)
		ret += buf[--len];

	return ret;
}

#define CMD_QUERY_RESULT 0x01
#define CMD_SET_TARGETID 0x02
#define CMD_SET_WIFI 0x03
#define CMD_SET_HOST 0x04

unsigned char _cmd_get_queryresult(unsigned char *buf, unsigned char pl, unsigned short *ret) {
	if (pl == 1) {
		switch (buf[0]) {
		case 0x70:
			*ret = 1;
			break;
		case 0x07:
			*ret = 0;
			break;
		default:
			return 0;
		}
		return CMD_QUERY_RESULT;
	}
	return 0;
}

unsigned char _cmd_set_profile(unsigned char* payload, unsigned char pl, unsigned short ver, unsigned short devid, unsigned short *ret) {
	if (pl == 2) {
		unsigned short targetid;
		targetid = (payload[0] << 8) + payload[1];
		if (targetid != 0x0000 && targetid !=0xffff) {
			IapEreaseSector(0x0000);
			IapProgramByte(0x0000, ver >> 8);
			IapProgramByte(0x0001, ver);
			IapProgramByte(0x0002, devid >> 8);
			IapProgramByte(0x0003, devid);
			IapProgramByte(0x0004, targetid >> 8);
			IapProgramByte(0x0005, targetid);
			*ret = targetid;

			return CMD_SET_TARGETID;
		}
	}
	return 0;
}

unsigned char _cmd_set_wifi_or_host(unsigned char* payload, unsigned char pl, unsigned char type) {
	unsigned short startpos = (type == CMD_SET_WIFI ? SSID_START_POS : HOST_START_POS);
	unsigned char psplit;
	unsigned char i;
	if (pl > 3) {
		
		for (psplit=0; psplit<pl; psplit++) 
			if (payload[psplit] == ':')
				break;

		if (psplit < pl - 1) {
			IapEreaseSector(startpos);
			// program ssid / host
			IapProgramByte(startpos, psplit);
			for (i=1; i<=psplit; i++) {
				IapProgramByte(startpos + i, payload[i-1]);
			}

			// program passwd / port
			IapProgramByte(startpos + psplit + 1, pl - psplit - 1);
			for (i=1; i<pl - psplit; i++) {
				IapProgramByte(startpos + psplit + i + 1, payload[psplit + i]);
			}
			return type;
		}
	}
	return 0;
}

unsigned char process_master_packet(const unsigned char *buf, unsigned short len, unsigned short ver, unsigned short devid, unsigned short *ret) {
	/*
		Packet Struct:
		 Forward       VER	   CMD     Payload Len     Payload    CRC
		0x0F 0xF0	|  2 B  |  1 B  |	   1B		|	PL B	|  1B  |

		VER = 0x0100
		CMD:
			QUERY_RESULT 0x01, PL(Payload Len) = 1, Payload = ALERT(0x70), OK(0x07), OTHER:ERROR
			SET_TARGETID 0x02, PL(Payload Len) = 2, Payload = targetid
			SET_WIFI     0x03, PL = vari, Payload = ssid:password
			SET_HOST	 0x04, PL = vari, Payload = host:port
		CRC:
			Byte ADD From VER to Payload
	*/
	if (buf[0] == 0x0F && buf[1] == 0xF0) {
		if (
				(buf[2]<<8) + buf[3] == ver && 	//  validate version
				buf[5] + 7 == len &&			//  validate length
				calc_crc(buf+2, len-3) == buf[len-1]  // validate crc
			) {
			switch (buf[4]) {
			case CMD_QUERY_RESULT:
				return _cmd_get_queryresult(buf + 6, buf[5], ret);
			case CMD_SET_TARGETID:
				return _cmd_set_profile(buf + 6, buf[5], ver, devid, ret);
			case CMD_SET_WIFI:
			case CMD_SET_HOST:
				return _cmd_set_wifi_or_host(buf + 6, buf[5], buf[4]);
			default:
				return 0;
			}
		}
	}
	return 0;
}

void reboot() {
	unsigned short i;
	LED_R = 0;
	LED_G = 0;
	LED_Y = 0;
	BEEP = 0;
	for (i=0; i<10000; i++) {
		delayXus(100);
	}
	IAP_CONTR |= 0x20;
	while (1);
}

void main() {
//	unsigned char packet[20];
	unsigned long i;
	unsigned char xdata buf[40];
	unsigned short pdata ver, devid, targetid;
	unsigned char pdata len_1;
	unsigned short pdata ret;
	unsigned short pdata hb_time = 32768, alarm_time = 0;
	unsigned char is_alarm_triggerd = 0;
	uart_init();
	timer0_init();
	EA=1;

	// process info
	ver = IapReadByte(0x0000);
	ver = (ver << 8) + IapReadByte(0x0001);
	devid = IapReadByte(0x0002);
	devid = (devid << 8) + IapReadByte(0x0003);
	targetid = IapReadByte(0x0004);
	targetid = (targetid<<8) + IapReadByte(0x0005);

	if (get_profile(&ver, &devid, &targetid) == 0) {
		LED_R = 0;
		BEEP = 0;
		while (1);
	}

	init_esp8266(2);

	while(1) {
		for (i=0;i<100000;i++) {
			// listen for packet
			len_1 = esp8266_recv(buf, sizeof(buf) - 1);
			if (len_1) {
				len_1 = process_master_packet(buf, len_1, ver, devid, &ret);
				if (len_1) {
					blink(1, 20);
					hb_time = 0;
					if (len_1 == CMD_QUERY_RESULT) {
						is_alarm_triggerd = ret;
					} else if (len_1 == CMD_SET_TARGETID) {
						targetid = ret;
					} else {
						esp8266_disconnect_wifi(0);
						reboot();
					}
				}
				/*
				buf[len_1] = 0;
				if (strcmp(buf, "TEST")==0) {
					blink(1, 20);
				}	*/
					
			} else {
				delayXus(30);
			}
		}

		blink(0, 20);
		len_1 = construct_query_packet(buf, ver, devid, targetid);
		if (esp8266_send(buf, len_1) == 0) {
			//TODO: shall fix issue
			init_esp8266(1);
		}
		else
			LED_R = 1;

		//LED_G = ~LED_G;
		LED_R = !is_alarm_triggerd;
		hb_time ++;

		if (is_alarm_triggerd) {
			if (alarm_time == 0) {
				blink(3, 500);
			}

			alarm_time ++;
			if (alarm_time > ALARM_BEEP_AGAIN) {
				alarm_time = 0;
			}	
		} else {
			alarm_time = 0;
		}

		if (hb_time > HB_TIMEOUT) {
			LED_Y = 0; 
		}

		// NTRST = KEY_1; 
	}
}
