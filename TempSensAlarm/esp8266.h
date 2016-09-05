#ifndef TSS_ESP8266_H
#define TSS_ESP8266_H
#include <stc15.h>

#define WAIT_READY_TIMEOUT 1000000
#define WAIT_IP_TIMEOUT  1000000
#define WAIT_CONNECT_TIMEOUT 100000
#define DEFAULT_WAIT 100000
unsigned char _wait_for_ok(long timeloop);
unsigned char esp8266_wait_ready(long timeloop);
unsigned char esp8266_wait_gotip(long timeloop);
unsigned char esp8266_test_at();
unsigned char esp8266_connect_wifi(const char *ssid, const char *password, unsigned char join_nextboot);
unsigned char esp8266_disconnect_wifi(unsigned char join_nextboot);
void esp8266_reset();
unsigned char esp8266_disconnect();
unsigned char esp8266_connect_udp(const unsigned char *host, const unsigned char* port);
unsigned char esp8266_create_wifi(const char *ssid, const char *password, const char *ip, unsigned char create_nextboot);
unsigned char esp8266_create_server(const char *port);
void int2cstr(unsigned char *buf, int num);

#ifdef UART_MODE
unsigned char esp8266_exit_uartmode();
unsigned char esp8266_enter_uartmode();
#else
unsigned char esp8266_send(const unsigned char *packet, int len, char client);
unsigned char esp8266_recv(unsigned char *buf, int buf_len);
#endif


#endif