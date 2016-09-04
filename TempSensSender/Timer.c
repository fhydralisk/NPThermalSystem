#include "Timer.h"

void timer0_init() {
	TMOD = 0x01; //MODE1
	AUXR |= 0x80; //1T
	TR0 = 1; //start
}
