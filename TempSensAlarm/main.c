#include <stc15.h>
#include <string.h>
#include <stdlib.h>
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
#define WEB_START_POS 0x0600

#define HB_TIMEOUT 5
#define ALARM_BEEP_AGAIN 300
#define VERSION 0x0100

unsigned char xdata g_buf[192];

void setting_state();

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

unsigned char connect_wifi() {	// return 1 succeed, 0 enter setting state
	unsigned short i;
	unsigned char len_ssid, len_pwd;
	/*unsigned char xdata ssid[SSID_LEN_MAX];
	unsigned char xdata pwd[PWD_LEN_MAX];*/
	unsigned char *ssid = g_buf;
	unsigned char *pwd = g_buf + SSID_LEN_MAX;

	// get ssid and pwd first
	len_ssid = IapReadByte(SSID_START_POS);
	if (len_ssid >= SSID_LEN_MAX || len_ssid == 0) {
		// TODO: extra
		return 0;
	}
	for (i=0;i<len_ssid;i++)
		ssid[i] = IapReadByte(SSID_START_POS + i + 1);
	ssid[i] = 0;
	len_pwd = IapReadByte(SSID_START_POS + len_ssid + 1);
	if (len_pwd >= PWD_LEN_MAX || len_pwd == 0) {
		// FIXME: TODO: INITSTATE, SSID_START_POS may vari.
		// setting_state();
		return 0;
	}
	for (i=0;i<len_pwd; i++)
		pwd[i] = IapReadByte(SSID_START_POS + len_ssid + i + 2);
	pwd[i] = 0;

	while (esp8266_connect_wifi(ssid, pwd, 1) == 0); //try connect
	return 1;
}

unsigned char connect_server() { // same as above
	unsigned short i;
	/*unsigned char xdata host[HOST_LEN_MAX];
	unsigned char xdata port[PORT_LEN_MAX];*/
	unsigned char *host = g_buf;
	unsigned char *port = g_buf + HOST_LEN_MAX;

	unsigned char len_host, len_port;
	len_host = IapReadByte(HOST_START_POS);
	if (len_host >= HOST_LEN_MAX || len_host == 0) {
		// FIXME: TODO: extra
		// BEEP = 0;
		// setting_state();
		return 0;
	}
	for (i=0;i<len_host;i++)
		host[i] = IapReadByte(HOST_START_POS + i + 1);
	host[i] = 0;
	len_port = IapReadByte(HOST_START_POS + len_host + 1);
	if (len_port >= PORT_LEN_MAX || len_port == 0) {
		// FIXME: TODO: extra
		// BEEP = 0;
		// setting_state();
		return 0;
	}
	for (i=0;i<len_port; i++)
		port[i] = IapReadByte(HOST_START_POS + len_host + i + 2);
	port[i] = 0;

	if (esp8266_connect_udp(host, port) == 0) {
		while(esp8266_connect_udp(host, port) == 0)
			LED_R = ~LED_R;
	}

	LED_R = 1;
	return 1;
}

unsigned char init_esp8266(unsigned char reboot, unsigned char server) {  // same as above
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

	if (server) {
		LED_Y = 0;
		if (esp8266_create_wifi("Alarm", "", "192.168.100.1", 0))	{
			 if (esp8266_create_server("80"))
			 	return 1;
		}
		BEEP = 0;
		while (1);
		return 0;
	} else {
		LED_Y = 0;
	
		if (esp8266_wait_gotip(1000000) == 0) {
			// not connected	
			if (connect_wifi() == 0)
				return 0;
		}
	
		if (reboot == 2) BEEP = 0;
		LED_Y = 1;
		if  (esp8266_wait_gotip(30000)) {
			if (reboot == 2) BEEP = 1;
			LED_G = 0;
		}
	
		// connect to server
		return connect_server();
	}
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

void response_web(char client) {
	/*
	unsigned short pdata web_len, i;
	delayXus(1000);
	web_len = IapReadByte(WEB_START_POS);
	web_len = (web_len << 8) + IapReadByte(WEB_START_POS + 1);
	uart_sendcstring("AT+CIPSEND=");
	uart_sendbyte(client);
	uart_sendbyte(',');
	int2cstr(g_buf, web_len);
	uart_sendcstring(g_buf);
	uart_sendcstring("\r\n");
	_wait_for_ok(10000);

	for (i=0; i<web_len; i++)
		uart_sendbyte(IapReadByte(WEB_START_POS + 2 + i));
	*/
	unsigned short pdata web_len, i;
	delayXus(1000);
	web_len = IapReadByte(WEB_START_POS);
	web_len = (web_len << 8) + IapReadByte(WEB_START_POS + 1);
	esp8266_send(0, web_len, client);
	for (i=0; i<web_len; i++)
		uart_sendbyte(IapReadByte(WEB_START_POS + 2 + i));
}


/*
void do_web_set(char client) {
	unsigned char i, j, tuc, lens, lens2, succeeds = 0;
	unsigned short devid, tarid;
	// FIXME:  length too short
	unsigned char xdata wifi[30], host[30];
	for (i=0; i<5; i++) {
		lens = uart_waitforstring(CONST_STR("="), g_buf, sizeof(g_buf), 20000);
		g_buf[lens - 1] = 0;
		lens2 = uart_waitforstring(CONST_STR("&"), g_buf + lens, sizeof(g_buf) - lens, 20000);
		g_buf[lens + lens2-1] = 0;
		if (strncmp(g_buf, "devid", 5) == 0) {
			devid = atoi(g_buf + lens);
		} else if (strncmp(g_buf, "tarid", 5) == 0) {
			tarid = atoi(g_buf + lens);
		} else if ((strncmp(g_buf, "host", 4) == 0) || strncmp(g_buf, "wifi", 4) == 0) {
			char *rd;
			rd = g_buf[0]=='h'?host:wifi;
			// tuc = replace_webstr(g_buf+lens);
			rd[0] = lens2 - 1;
			for (j=0; j<lens2 - 1; j++)
				rd[j] = g_buf[lens + j];
			//succeeds += _cmd_set_wifi_or_host(g_buf+lens, lens2 - tuc - 1, CMD_SET_HOST);
			//uart_sendcstring(g_buf+lens);
		} 
	}	
	g_buf[0] = tarid >> 8;
	g_buf[1] = tarid;
	succeeds += _cmd_set_profile(g_buf, 2, VERSION, devid, &tarid);
	tuc = replace_webstr(wifi);
	succeeds += _cmd_set_wifi_or_host(wifi+1, wifi[0]-tuc, CMD_SET_WIFI);
	uart_sendstring(wifi+1, wifi[0]-tuc);
	tuc = replace_webstr(host);
	succeeds += _cmd_set_wifi_or_host(host+1, host[0]-tuc, CMD_SET_HOST);
	uart_sendstring(host+1, host[0]-tuc);
	if (succeeds < 3) {
		//failed
		LED_R = 0;
		// use tarid for temp space
		for (tarid=0; tarid<=1000; tarid++)
			delayXus(1000);

		IapEreaseSector(0x0000);
		IapEreaseSector(SSID_START_POS);
		IapEreaseSector(HOST_START_POS);

	} else {
		LED_G = 0;
		for (tarid=0; tarid<=1000; tarid++)
			delayXus(1000);
	}
	esp8266_reset();
	reboot();
}*/


unsigned char replace_webstr(char *buf) {
    unsigned char i = 0;
    while(buf[i]) {
        if (buf[i] == '%' && buf[i+1] == '3' && (buf[i+2] == 'A' || buf[i+2] == 'a')) {
            buf[i] = ':';
            break;
        }
        i++;
    }
    if (buf[i]) {
        i++;
        while (buf[i+2]) {
            buf[i] = buf[i+2];
            i++;
        }
        buf[i] = 0;
        return 2;
    }
    return 0;
}

void do_web_set(char client) {
	unsigned char lens, st=0, ed=0, mid = 0, tuc, succeeds = 0;
	unsigned char *pos_key, *pos_value;
	unsigned char len_value;
	unsigned short tarid, devid;
	lens = uart_waitforstring(CONST_STR(" "), g_buf, sizeof(g_buf), 200000);
	uart_sendstring(g_buf, lens);
	while (1) {
		for ( ; g_buf[ed] != '='; ed++);
		mid = ed;
		for ( ; ed < lens; ed++) {
			if (g_buf[ed] == '&')
				break;
		}
		pos_key = g_buf + st;
		pos_value = g_buf + mid + 1;
		len_value = ed - mid - 1;
		g_buf[ed] = 0;
		if (strncmp(pos_key, "devid", 5) == 0) {
			devid = atoi(pos_value);
		} else if (strncmp(pos_key, "tarid", 5) == 0) {
			tarid = atoi(pos_value);
		} else if (strncmp(pos_key, "wifi", 4) == 0) {
			tuc = replace_webstr(pos_value);
			succeeds += _cmd_set_wifi_or_host(pos_value, len_value - tuc, CMD_SET_WIFI);
			//uart_sendstring(pos_value, len_value - tuc);
		} else if (strncmp(pos_key, "host", 4) == 0){
			tuc = replace_webstr(pos_value);
			succeeds += _cmd_set_wifi_or_host(pos_value, len_value - tuc, CMD_SET_HOST);
			//uart_sendstring(pos_value, len_value - tuc);
		}
		if (ed == lens) {
			g_buf[0] = tarid >> 8;
			g_buf[1] = tarid;
			succeeds += _cmd_set_profile(g_buf, 2, VERSION, devid, &tarid);
			break;
		}
		st = ed + 1;
		ed ++; 
	}

	if (succeeds != CMD_SET_HOST+CMD_SET_WIFI+CMD_SET_TARGETID) {
		//failed
		LED_R = 0;
		// use tarid for temp space
		for (tarid=0; tarid<=1000; tarid++)
			delayXus(1000);

		IapEreaseSector(0x0000);
		IapEreaseSector(SSID_START_POS);
		IapEreaseSector(HOST_START_POS);

	} else {
		LED_G = 0;
		for (tarid=0; tarid<=1000; tarid++)
			delayXus(1000);
	}
	esp8266_reset();
	reboot();

}

void setting_state() {
//	unsigned short i;
	unsigned char client;
	LED_R = 0;
	blink(3, 1000);
	init_esp8266(0, 1);

	while(1) {
		if (uart_waitforstring(CONST_STR("IPD,"), g_buf, sizeof(g_buf), 100000)) {
			uart_getstring(g_buf, 1, 1000);
			client = g_buf[0] - '0'; 
			if (uart_waitforstring(CONST_STR("GET /"), g_buf, sizeof(g_buf), 10000)) {
				uart_getstring(g_buf, 7, 10000);
				
				if (g_buf[0] == ' ') {
					// GET /
					response_web(client);
				}  else if (strncmp(g_buf, "set.do?", 7)==0) {
					// set.do
					do_web_set(client);
				}
			}
		}
	}
}

void main() {
//	unsigned char packet[20];
	unsigned long i;
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
		setting_state();
	}

	if (init_esp8266(2, 0) == 0) {
		setting_state();
	}

	while(1) {
		for (i=0;i<100000;i++) {
			// listen for packet
			len_1 = esp8266_recv(g_buf, sizeof(g_buf) - 1);
			if (len_1) {
				len_1 = process_master_packet(g_buf, len_1, ver, devid, &ret);
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
					
			} else {
				delayXus(30);
			}
		}

		blink(0, 20);
		len_1 = construct_query_packet(g_buf, ver, devid, targetid);
		if (esp8266_send(g_buf, len_1, -1) == 0) {
			//TODO: shall fix issue
			init_esp8266(1, 0);
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

