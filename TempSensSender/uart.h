#ifndef TSS_UART_H
#define TSS_IART_H

#include <stc15.h>

#define CONST_STR(c) c, sizeof(c)-1

void uart_init();
void uart_sendbyte(const unsigned char b);
void uart_sendstring(const unsigned char s[], int len);

#ifdef USE_SENDCSTRING
void uart_sendcstring(const unsigned char s[]);
#endif
						   
unsigned char uart_getbyte(unsigned char *b);
unsigned char uart_getstring(unsigned char *s, int len, long loop_timeout);
unsigned char uart_waitforstring(const unsigned char *stpf, int lenpf, unsigned char *buf, int lenbuf, long loop_timeout);
unsigned char uart_waitforcstrings(const unsigned char **stpf, unsigned char len_list, unsigned char *buf, int lenbuf, long loop_timeout);
#endif