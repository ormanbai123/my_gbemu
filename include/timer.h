#ifndef GB_TIMER_H
#define GB_TIMER_H

#include <stdint.h>

typedef struct Timer {
	uint16_t div;

	bool prevAndResult;
};

void TickTimer(uint32_t elapsedCycles);

void InitTimer(Timer* timer);

void ResetDiv();

#endif // !TIMER_H