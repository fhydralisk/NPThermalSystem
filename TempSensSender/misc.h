#ifndef TSS_MISC_H
#define TSS_MISC_H

#include <stc15.h>
#include "Timer.h"

#ifndef FOSC
#define FOSC 11059200
#endif

void delayXus(unsigned short n);
void pack_short_to_charptr(unsigned char *buf, unsigned short n);
unsigned char strn_cmp(unsigned char *b1, unsigned char *b2, unsigned int n);
void delayXms(unsigned short n);

#endif