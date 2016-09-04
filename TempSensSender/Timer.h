#ifndef TSS_TIMER_H
#define TSS_TIMER_H

#include <stc15.h>

typedef struct {
	unsigned long low;
	unsigned long high;
} u64;

void timer0_init();
//void get_time(u64* time);


#endif