#ifndef GB_EMULATOR_H
#define GB_EMULATOR_H


#include <cstdlib>
#include <cstdint>

#include "cpu.h"
#include "timer.h"

#include "ppu.h"

#include "joypad.h"

typedef struct Gameboy {
	uint8_t memory[0x10000];
	CPU* cpu;
	
	PPU* ppu;

	Timer timer;

	uint32_t elapsedCycles; // Elapsed T cycles;

	bool eiDelayed;

	Joypad joypad;

	Color* screenBuffer;
};

void InitGameboy(Color* screenBuffer);

void InsertCartridge(const char* path);

void HandleGbInput();

void RunGameboy();

void ShutdownGameboy();

Gameboy* getInstance();

#ifdef LOGGING_ENABLED
std::string LogRunGameboy();
Gameboy* GetGameboyState();
#endif

#endif // !EMULATOR_H