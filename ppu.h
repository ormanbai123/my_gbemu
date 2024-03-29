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

enum FetcherState {
	GET_TILE_NO,
	GET_LO,
	GET_HI,
	PUSH
};

typedef struct Fetcher {
	FetcherState state;
	uint16_t x_pos;

	uint8_t tile_no;
	uint8_t tile_data_lo;
	uint8_t tile_data_hi;
};

typedef struct PPU {
	uint32_t elapsedDots;
	uint32_t scanlineDots;
	
	bool oldStatFlag;
	PpuMode mode;

	uint8_t ly; 
	int lx; // Need to be signed for scrolling

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

#endif // !GB_PPU_H
