#ifndef GB_CPU_H
#define GB_CPU_H

#include <cstdint>
#include <stdio.h>

//#define LOGGING_ENABLED 1

#ifdef LOGGING_ENABLED
#include <string>
#endif

namespace {
	const uint8_t dummy = 1;
}
#define IS_LITTLE_ENDIAN (int)(((char*)&dummy)[0])

union Register {
	uint16_t reg16;
	struct {
#ifdef IS_LITTLE_ENDIAN
		uint8_t lo;
		uint8_t hi;
#else
		uint8_t hi;
		uint8_t lo;
#endif
	};
};

typedef struct CPU {
	Register AF;
	Register BC;
	Register DE;
	Register HL;
	Register SP;
	Register PC;

	bool IME;
	bool isStopped;

	bool imeRequested;
};

CPU* InitCpu();

uint8_t TickCpu(CPU* cpu); // Returns elapsed cycles

uint8_t HandleInterrupts(CPU* cpu); // Returns elapsed cycles

#ifdef LOGGING_ENABLED
uint8_t LogTickCpu(CPU* cpu, std::string& in_str);
#endif

#endif // !CPU_H