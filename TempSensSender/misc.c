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
