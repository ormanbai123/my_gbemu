#ifndef GB_BUS_H
#define GB_BUS_H


#include <cstdint>
#include "allocator.h"

#define BIOS_ENABLE

enum MBC_TYPE {
	MBC0,
	MBC1,
	MBC2, 
	MBC3
};

typedef struct Cartridge {
	uint8_t* rom;
	uint8_t* ram;

	// Registers
	bool ramEnabled;
	bool banking_mode;
	uint8_t nRomBank;
	uint8_t nRamBank;
};

typedef struct Bus {
	uint8_t* memory;
};

void InitBus(uint8_t* memory);
void InitCartridge(uint8_t* cartData, size_t size);

uint8_t GB_Read(uint16_t addr);
void GB_Write(uint16_t addr, uint8_t data);

void GB_Internal_Write(uint16_t addr, uint8_t data); // Function to write to Hardware Registers
uint8_t GB_Internal_Read(uint16_t addr); // Handy function to read Hardware Registers

#endif // !BUS_H
