#include <stdlib.h>
#include <stdio.h>
#include "esp8266.h"
#include "uart.h"
#include "misc.h"

unsigned char _wait_for_ok(long timeloop) {
	unsigned char packet[5];
	return uart_waitforstring(CONST_STR("OK\r\n"), packet, sizeof(packet), timeloop);
}

unsigned char esp8266_wait_ready(long timeloop) {
	unsigned char packet[8];
	if (uart_waitforstring(CONST_STR("ready\r\n"), packet, sizeof(packet), timeloop)) 
		return 1;

	return esp8266_test_at();
}

unsigned char esp8266_wait_gotip(long timeloop) {
	unsigned char packet[9];
	if (uart_waitforstring(CONST_STR("GOT IP\r\n"), packet, sizeof(packet), timeloop)) 
		return 1;

	uart_sendstring(CONST_STR("AT+CIPSTATUS\r\n"));
	return uart_waitforstring(CONST_STR("STATUS:2"), packet, sizeof(packet), DEFAULT_WAIT);
}

unsigned char esp8266_test_at() {
	uart_sendstring(CONST_STR("AT\r\n"));
	if (_wait_for_ok(DEFAULT_WAIT)) 
		return 1;

	return 0;
}

void _send_udp_cmd(const unsigned char* host, const unsigned char* port) {
	uart_sendcstring("AT+CIPSTART=\"UDP\",\"");
	uart_sendcstring(host);
	uart_sendcstring("\",");
	uart_sendcstring(port);
	uart_sendcstring("\r\n");
}

void _send_close_cmd() {
	uart_sendcstring("AT+CIPCLOSE\r\n");
}

unsigned char esp8266_connect_udp(const unsigned char *host, const unsigned char* port) {
	_send_udp_cmd(host, port);	
	if (_wait_for_ok(WAIT_CONNECT_TIMEOUT) == 0) {
		_send_close_cmd();
		if (_wait_for_ok(DEFAULT_WAIT)) {
			_send_udp_cmd(host, port);
			if (_wait_for_ok(WAIT_CONNECT_TIMEOUT))
				return 1; // succeed
			else
				return 0; // error
		} else {
			return 0; //device error?
		}
	}

	return 1;
}
#ifdef ESP8266_DISCONNECT
unsigned char esp8266_disconnect() {
	_send_close_cmd();
	return _wait_for_ok(DEFAULT_WAIT);
}
#endif
#ifdef UART_MODE
unsigned char esp8266_enter_uartmode() {
	unsigned char pkt[2];
	uart_sendcstring("AT+CIPMODE=1\r\n");
	if (_wait_for_ok(DEFAULT_WAIT) == 0)
		return 0;	

	uart_sendcstring("AT+CIPSEND\r\n");
	return uart_waitforstring(CONST_STR(">"), pkt, sizeof(pkt), DEFAULT_WAIT);
}

unsigned char esp8266_exit_uartmode() {
	unsigned short i;
	uart_sendcstring("+++");
	for (i=0; i<1000; i++)
		delayXus(1000);
	return esp8266_test_at();		
}
#else

void int2cstr(unsigned char *buf, int num) {
	int i=0, tmp = num;
	while (tmp) {
		tmp /= 10;
		i++;
	}
	
	buf[i] = 0;
	while (num > 0) {
		buf[--i] = num % 10 + '0';
		num /= 10;
	}
}

unsigned char esp8266_send(const unsigned char* packet, int len, char client) {
	unsigned char len_str[7];
	uart_sendcstring("AT+CIPSEND=");
	if (client >= 0 && client <= 4) {
		uart_sendbyte(client+'0');
		uart_sendbyte(',');
	}
	int2cstr(len_str, len);
	uart_sendcstring(len_str);
	uart_sendcstring("\r\n");
	if (_wait_for_ok(DEFAULT_WAIT)==0)
		return 0;
	uart_sendstring(packet, len);
	return _wait_for_ok(len/2*DEFAULT_WAIT);
}

unsigned char esp8266_recv(unsigned char *buf, int buf_len) {
	unsigned char b;
	int len_1, len_2;
	if (uart_getbyte(&b) && uart_waitforstring(CONST_STR("IPD,"), buf, buf_len, 1000)) {
		len_1 = uart_waitforstring(CONST_STR(":"), buf, buf_len, 1000);
		buf[len_1 - 1]=0;
		len_2 = atoi(buf);
		len_2 = len_2 > buf_len ? buf_len : len_2;
		len_1 = uart_getstring(buf, len_2, 200*len_2);
		if (len_1 == len_2)
			return len_2;
		else
			return 0; // error
	}

	return 0;	
}
#endif

void esp8266_reset() {
	uart_sendcstring("AT+RST\r\n");
}

unsigned char esp8266_query_wifi_mode() {
	unsigned char pkt[8];
	uart_sendcstring("AT+CWMODE?\r\n");
	if (uart_waitforstring(CONST_STR("CWMODE:"), pkt, sizeof(pkt), DEFAULT_WAIT))
	    if (uart_getbyte(pkt)) {
			pkt[0] -= '0';
			_wait_for_ok(DEFAULT_WAIT);
			return pkt[0];
		}

	return 255;
}

unsigned char esp8266_set_wifi_mode(unsigned char mode) {
	if (mode > 0 && mode < 4) {
		uart_sendcstring("AT+CWMODE_DEF=");
		uart_sendbyte(mode + '0');
		uart_sendcstring("\r\n");
		//	esp8266_reset();
		return _wait_for_ok(WAIT_READY_TIMEOUT);
	}
	return 0;
}

unsigned char esp8266_join_wifi(const char *ssid, const char *password, unsigned char join_nextboot) {;
	if (join_nextboot) {
		uart_sendcstring("AT+CWAUTOCONN=1\r\n");
		_wait_for_ok(DEFAULT_WAIT);
		uart_sendcstring("AT+CWJAP_DEF=\"");
	}
	else
		uart_sendcstring("AT+CWJAP_CUR=\"");

	uart_sendcstring(ssid);
	uart_sendcstring("\",\"");
	uart_sendcstring(password);
	uart_sendcstring("\"\r\n");

	return _wait_for_ok(WAIT_IP_TIMEOUT * 2);
}


unsigned char esp8266_connect_wifi(const char *ssid, const char *password, unsigned char join_nextboot) {
	unsigned char wifimode;
	wifimode = esp8266_query_wifi_mode();
	if (wifimode == 255)
		goto error;
	if (wifimode != 1) {
		if (esp8266_set_wifi_mode(1) == 0)
			goto error;
	}

	return esp8266_join_wifi(ssid, password, join_nextboot);

error:
	return 0;
	
}


unsigned char esp8266_disconnect_wifi(unsigned char join_nextboot) {
	if (join_nextboot)
		uart_sendcstring("AT+CWAUTOCONN=1\r\n");
	else
		uart_sendcstring("AT+CWAUTOCONN=0\r\n");
	_wait_for_ok(DEFAULT_WAIT);
	
	uart_sendcstring("AT+CWQAP\r\n");
	return _wait_for_ok(WAIT_READY_TIMEOUT);

}

unsigned char esp8266_create_wifi(const char *ssid, const char *password, const char *ip, unsigned char create_nextboot) {
	char postfix[6] = "_CUR";
	if (create_nextboot) {
		postfix[1] = 'D';
		postfix[2] = 'E';
		postfix[3] = 'F';
	}
	uart_sendcstring("AT+CWMODE");
	uart_sendcstring(postfix);
	uart_sendcstring("=2\r\n");
	if (wait_for_ok(WAIT_READY_TIMEOUT) == 0)
		goto error;

	uart_sendcstring("AT+CWSAP");
	uart_sendcstring(postfix);
	uart_sendcstring("=\"");
	uart_sendcstring(ssid);
	uart_sendcstring("\",\"");
	uart_sendcstring(password);
	uart_sendcstring("\",3,");
	if (password[0] == 0)
		uart_sendcstring("0\r\n");
	else
		uart_sendcstring("3\r\n");

	if (_wait_for_ok(WAIT_READY_TIMEOUT) == 0)
		goto error;

	uart_sendcstring("AT+CIPAP");
	uart_sendcstring(postfix);
	uart_sendcstring("=\"");
	uart_sendcstring(ip);
	uart_sendcstring("\"\r\n");

	return _wait_for_ok(WAIT_READY_TIMEOUT);

error:
	return 0;
}

unsigned char esp8266_create_server(const char *port) {
	uart_sendcstring("AT+CIPMUX=1\r\n");
	if (_wait_for_ok(WAIT_IP_TIMEOUT) == 0)
		goto error;
	uart_sendcstring("AT+CIPSERVER=1,");
	uart_sendcstring(port);
	uart_sendcstring("\r\n");
	return _wait_for_ok(WAIT_IP_TIMEOUT);

error:
	return 0;
}