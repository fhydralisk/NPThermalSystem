#include <string.h>
#include "uart.h"
#include "misc.h"

#define BAUD 115200
#define INTERRUPT_UART1	4

char g_busy = 0;
char g_bytebuf = 0;
char g_readable = 0;

void uart_init() {
	SCON = 0x50;
	AUXR1 = 0x40; // USE TX/RX_2
	T2L = (65536 - (FOSC / 4 /  BAUD));
	T2H = (65536 - (FOSC / 4 /  BAUD)) >> 8;
	AUXR = 0x14;
	AUXR |= 0x01;
	ES = 1;
}

void uart_sendbyte(const unsigned char b) {
	while (g_busy);
	g_busy = 1;
	SBUF = b;
}


void uart_sendstring(const unsigned char s[], int len) {
	int l;
	for (l=0; l<len; l++)
		uart_sendbyte(s[l]);
}

void uart_sendcstring(const unsigned char s[]) {
	int l = 0;
	while (s[l]) {
		uart_sendbyte(s[l]);
		l++;
	};
}

unsigned char uart_getbyte(unsigned char *b) {
	if (g_readable) {
		g_readable = 0;
		*b = g_bytebuf;
		return 1;
	}
	return 0;
}

unsigned char uart_getstring(unsigned char *s, int len, long loop_timeout) {
	long loops = 0;
	int len_received = 0;
	while (loops < loop_timeout) {
		if (uart_getbyte(&s[len_received])) {
			len_received++;
			if (len_received == len)
				return len;
		}
		loops++;
	}

	return len_received;
}

void enqueue(unsigned char b, unsigned char *queue, int len, int *st, int *ed) {
	
	if (*ed == -1) {
		*ed = 0;
	} else {
		*ed = *ed + 1;

		if (*ed >= len)
			// OVERFLOW
			*ed = 0;
		if (*ed == *st)
			*st = *st + 1;
	
		if (*st >= len)
			// will overflow
			*st = 0;
	}
	queue[*ed] = b;
}

int read_queue_size(int len, int st, int ed) {
	if (ed == -1)
		return 0;
	if (st <= ed)
		return ed - st + 1;
	else
		return len;
}

unsigned char read_rqueue(const unsigned char* queue, int len, int st, int n) {
	if (st + n >= len)
		return queue[st + n - len];
	else
		return queue[st + n];
}

void sort_rqueue(unsigned char *queue, int len, int st, int ed) {
    int pf = 0, pe = len, i;
    unsigned char swp;
    if (st <= ed || ed == -1)
        return;
    while (st - pf > 0 && pe - st > 0) {
        if (st - pf > pe - st) {
            // 交换后半段
            for (i = 0; i < pe - st; i++) {
                swp = queue[st + i];
                queue[st + i] = queue[pf + i];
                queue[pf + i] = swp;
            }
            pf += pe - st;
        } else {
            // 交换前半段
            int swplen = st - pf;
            for (i = 0; i < swplen; i++) {
                swp = queue[pf + i];
                queue[pf + i] = queue[pe - swplen + i];
                queue[pe - swplen + i] = swp;
            }
            pe -= swplen;
        }
    }
}

unsigned char uart_waitforstring(const unsigned char *stpf, int lenpf, unsigned char *buf, int lenbuf, long loop_timeout) {
	long loops = 0;
	unsigned char b;
	int n, qsize;
	int st = 0, ed = -1;

	if (lenpf > lenbuf)
		return 0;

	while (loops < loop_timeout) {
		if (uart_getbyte(&b)) {
			enqueue(b, buf, lenbuf, &st, &ed);
			qsize = read_queue_size(lenbuf, st, ed);
			
			if (qsize >= lenpf) {
				for (n=0; n<lenpf; n++){
					if (stpf[n]!=read_rqueue(buf, lenbuf, st, qsize-lenpf+n))
						break;
				}

				if (n == lenpf) {
					sort_rqueue(buf, lenbuf, st, ed);
					return qsize; // succeed
				}
			}			
		}
		loops++;
	}
	sort_rqueue(buf, lenbuf, st, ed);
	return 0;
}
#ifdef UART_WAITFORCSTRINGS
unsigned char uart_waitforcstrings(const unsigned char **stpf, unsigned char len_list, unsigned char *buf, int lenbuf, long loop_timeout) {
	long loops = 0;
	unsigned char b, nl;
	int lenpf;
	int n, qsize;
	int st = 0, ed = -1;

	for (nl=0; nl<len_list; nl++)
		if (strlen(stpf[nl]) > lenbuf)
			return 0;

	while (loops < loop_timeout) {
		if (uart_getbyte(&b)) {
			enqueue(b, buf, lenbuf, &st, &ed);
			qsize = read_queue_size(lenbuf, st, ed);
			for (nl=0; nl<len_list; nl++) {
				lenpf = strlen(stpf[nl]);
				if (qsize >= lenpf) {
					for (n=0; n<lenpf; n++){
						if (stpf[nl][n]!=read_rqueue(buf, lenbuf, st, qsize-lenpf+n))
							break;
					}
	
					if (n == lenpf) {
						sort_rqueue(buf, lenbuf, st, ed);
						return nl; // succeed
					}
				}	
			}		
		}
		loops++;
	}
	sort_rqueue(buf, lenbuf, st, ed);
	return 0;
}
#endif

void interrupt_uart() interrupt INTERRUPT_UART1 using 1 {
	if (RI) {
		RI = 0;
		g_bytebuf = SBUF;
		g_readable = 1;
	}

	if (TI) {
		TI = 0;
		g_busy = 0;
	}
}