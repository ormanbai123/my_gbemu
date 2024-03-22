#ifndef GB_PPU_H
#define GB_PPU_H

#include "raylib.h"
#include <stdint.h>

#define _FIFO_SIZE_ 8

typedef struct FifoPixel {
	uint8_t color_id;
	uint8_t palette;
	uint8_t bg_priority;
};

typedef struct FIFO {
	uint16_t size;
	FifoPixel buff[_FIFO_SIZE_];

	uint16_t head;
	uint16_t tail;
};

#define MAX_VISIBLE_SPRITES 10

typedef struct SpriteBuffer {
	uint8_t size;
	uint8_t buff[MAX_VISIBLE_SPRITES]; // Holds sprite tile numbers
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
	bool is_fetching_window;
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

	// Oam sprite buffer
	uint16_t oam_offset;
	SpriteBuffer spriteBuff;
	//

	// Fetcher
	bool doneDummyRead;
	Fetcher fetcher;
	//
	
	// Fifo
	FIFO bgFifo;
	FIFO objFifo;
	//

	bool clippingStarted;

	Color* screenBuffer;
};

void ResetLcd();

void InitPpu(Color* screen_buffer);

void TickPpu(uint32_t cycles);

// Logging
void DrawTiles();

#endif // !GB_PPU_H
