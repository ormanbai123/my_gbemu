#include "timer.h"
#include "bus.h"

#include "utils.h"

#include <stdio.h>

namespace {
	Timer* g_timer = nullptr;

	uint16_t clock_freq_bits[4] = { 9, 3, 5, 7 };

	bool tima_overflowed = false;
	uint8_t tima_reset_elapsed = 0; // Count the number T cycles passed since TIMA overflow.
}

void TickTimer(uint32_t elapsedCycles) {
	uint8_t TAC = GB_Read(0xFF07);
	uint8_t TIMA = GB_Read(0xFF05);
	uint8_t TMA = GB_Read(0xFF06);
	
	uint8_t IF = GB_Read(0xFF0F);

	for (uint32_t i = 0; i < elapsedCycles; ++i) {

		if (tima_overflowed) {
			tima_reset_elapsed++;
			if (tima_reset_elapsed == 4) {
				tima_overflowed = false;
				tima_reset_elapsed = 0;

				TIMA = TMA;
				BIT_SET(IF, 2);
			}
		}

		g_timer->div++;

		uint16_t freq_bit = BIT_GET(g_timer->div, clock_freq_bits[TAC & 0b11]);
		uint16_t timer_enable = BIT_GET(TAC, 2);

		uint16_t and_result = freq_bit & timer_enable;
		if (g_timer->prevAndResult == 1 && and_result == 0) {
			// Falling edge
			if (TIMA == 0xFF) {
				/*TIMA = TMA;
				BIT_SET(IF, 2);*/
				
				TIMA = 0;
				tima_overflowed = true;
			}
			else {
				TIMA++;
			}
		}
		g_timer->prevAndResult = and_result;
	}

	GB_Internal_Write(0xFF0F, IF); // Update Interrupt Flag.
	GB_Internal_Write(0xFF05, TIMA); // Update TIMA.
	GB_Internal_Write(0xFF04, g_timer->div >> 8); // Update DIV.
}

void InitTimer(Timer* timer) {
	timer->div = 0;
	timer->prevAndResult = false;

	g_timer = timer;
}

void ResetDiv()
{
	g_timer->div = 0;
}
