#include "misc.h"
#include "Timer.h"

void delayXus(unsigned short n) {
	TH0 = 0;
	TL0 = 0;
	while ((TH0 << 8) + TL0 < n * (FOSC / 1000000));
}

void pack_short_to_charptr(unsigned char *buf, unsigned short n) {
	buf[0] = n >> 8;
	buf[1] = n;
}

unsigned char strn_cmp(unsigned char *b1, unsigned char *b2, unsigned int n) {
	int i;
	for (i=0; i<n; i++)
		if (b1[i] != b2[i])
			return 255;

	return 0;
}

void delayXms(unsigned short n) {
	int i;
	for (i=0; i<n; i++)
		delayXus(1000);
}
