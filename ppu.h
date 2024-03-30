#ifndef GB_PPU_H
#define GB_PPU_H

#include "raylib.h"
#include <stdint.h>

#define MAX_VISIBLE_SPRITES 10

typedef struct Sprite {
	uint8_t posY;
	uint8_t posX;
	uint8_t tileIndx;
	uint8_t flags;
};

typedef struct SpriteBuffer {
	uint8_t head; // indx from which to pop.
	uint8_t size;
	Sprite buff[MAX_VISIBLE_SPRITES]; // Holds sprite tile numbers
};

enum PpuMode {
	HBLANK, 
	VBLANK,
	OAMSCAN,
	DRAWING
};

typedef struct PPU {
	uint32_t elapsedDots;
	uint16_t scanlineDots;
	
	bool oldStatFlag;
	PpuMode mode;

	uint8_t ly; 

	// Window related stuff
	bool isFetchingWindow;
	uint8_t windowLineCounter; // Internal Counter for Window rendering.
	bool wylyCondition;

	// Oam sprite buffer
	SpriteBuffer spriteBuff;
	//

	Color* screenBuffer;
	void (*RenderFrame)(void);
};

void ResetLcd();

PPU* InitPpu(Color* screen_buffer, void (*RenderFrame)(void));

void TickPpu(uint32_t cycles);

void CheckLcdInterrupt();

uint8_t GetPpuLy();

#endif // !GB_PPU_H
