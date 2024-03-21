#include "ppu.h"

#include "bus.h"

#include "utils.h"

#include <stdio.h>

namespace {
	PPU* g_ppu;

	Color RAY_COLORS[] = {
		{255,255,255,255},
		{169,169,169,255},
		{84,84,84,255},
		{0,0,0,255}
	};
}

namespace {

	void reset_fifo(FIFO* fifo) {
		fifo->size = 0;

		fifo->head = 0;
		fifo->tail = 0;
	}

	void push_fifo(FIFO* fifo, FifoPixel pixel) {
		fifo->size++;

		fifo->buff[fifo->tail] = pixel;
		fifo->tail = (fifo->tail + 1) % _FIFO_SIZE_;
	}

	FifoPixel pop_fifo(FIFO* fifo) {
		FifoPixel out = fifo->buff[fifo->head];

		fifo->size--;

		fifo->head = (fifo->head + 1) % _FIFO_SIZE_;
		if (fifo->head == fifo->tail) {
			fifo->head = 0;
			fifo->tail = 0;
		}
		return out;
	}

	void reset_fetcher(Fetcher* fetcher) {
		fetcher->state = FetcherState::GET_TILE_NO;
		fetcher->is_fetching_window = false;
		fetcher->tile_no = 0;
		fetcher->tile_data_lo = 0;
		fetcher->tile_data_hi = 0;
		fetcher->x_pos = 0;
	}

	void reset_oam_stuff() {
		g_ppu->oam_offset = 0;
		g_ppu->spriteBuff.size = 0;
	}

	void oam_scan() {
		uint8_t ly = g_ppu->ly;
		uint8_t spriteHeight = BIT_GET(GB_Internal_Read(0xFF40), 2) ? 16 : 8;

		// Sprite attributes
		uint8_t yPos = 0;
		uint8_t xPos = 0;
		uint8_t tileIndx = 0;
		uint8_t flags = 0;

		yPos     = GB_Read(0xFE00 + g_ppu->oam_offset + 0);
		xPos	 = GB_Read(0xFE00 + g_ppu->oam_offset + 1);
		tileIndx = GB_Read(0xFE00 + g_ppu->oam_offset + 2);
		flags	 = GB_Read(0xFE00 + g_ppu->oam_offset + 3);
		g_ppu->oam_offset += 4;

		if (xPos < 0)
			return;
		if (ly + 16 < yPos)
			return;
		if (ly + 16 >= yPos + spriteHeight)
			return;
		if (g_ppu->spriteBuff.size >= MAX_VISIBLE_SPRITES)
			return;

		// Push to SpriteBuffer
		g_ppu->spriteBuff.buff[g_ppu->spriteBuff.size] = tileIndx;
		g_ppu->spriteBuff.size = (g_ppu->spriteBuff.size + 1) % MAX_VISIBLE_SPRITES;	
	}

	void fetch_bg(uint16_t nth_cycle) {
		uint8_t  lcdc = GB_Internal_Read(0xFF40);
		uint16_t scy  = GB_Internal_Read(0xFF42);
		uint16_t scx  = GB_Internal_Read(0xFF43);
		uint16_t ly   = g_ppu->ly;

		switch (g_ppu->fetcher.state) {
			case FetcherState::GET_TILE_NO: {
				// Executes every other cycle
				if (nth_cycle & 0x1)
					return;

				uint16_t base = 0x9800;
				uint16_t offset = 0;

				if (g_ppu->fetcher.is_fetching_window) {
					// Window
					// TODO
					//offset = g_ppu->fetcher.x_pos + 32 * (/ 8);
					base = BIT_GET(lcdc, 6) ? 0x9C00 : 0x9800;
				}
				else {
					// Background
					offset = ((g_ppu->fetcher.x_pos + (scx / 8)) & 0x1F) + ((((ly + scy) & 0xFF) / 8) * 32);
					//offset = (((g_ppu->lx + scx) / 8) & 0x1f) | ((((ly + scy) / 8) & 0x1f) << 5);
					offset &= 0x3FF;

					base = BIT_GET(lcdc, 3) ? 0x9C00 : 0x9800;
				}
				
				// TODO FIX TILE FETCHER 
				// PROBLEM: GB_Read() always returns 0!
				g_ppu->fetcher.tile_no = GB_Read(base + offset);
				//g_ppu->fetcher.tile_no = GB_Read(base | offset);
				

				// Debugging
				//printf("Base:%x | Offset:%x |||| ", base, offset);
				//printf("Tile number: %d |", g_ppu->fetcher.tile_no);
				//----------------------------------------------

				g_ppu->fetcher.state = FetcherState::GET_LO; // Change state.
				break;
			}
			case FetcherState::GET_LO: {
				// Executes every other cycle
				if (nth_cycle & 0x1)
					return;

				uint16_t base;
				uint16_t offset;

				if (BIT_GET(lcdc, 4)) {
					base = 0x8000 + g_ppu->fetcher.tile_no * 16;
				}
				else {
					base = 0x9000 + (int8_t)g_ppu->fetcher.tile_no * 16; // Make signed
				}

				if (!g_ppu->fetcher.is_fetching_window)
					offset = 2 * ((ly + scy) % 8);
				else
					; //offset = 2 * () //TODO

				g_ppu->fetcher.tile_data_lo = GB_Read(base + offset + 0);

				g_ppu->fetcher.state = FetcherState::GET_HI; // Change state.
				break;
			}
			case FetcherState::GET_HI: {
				// Executes every other cycle
				if (nth_cycle & 0x1)
					return;

				uint16_t base;
				uint16_t offset;

				if (BIT_GET(lcdc, 4)) {
					base = 0x8000 + g_ppu->fetcher.tile_no * 16;
				}
				else {
					base = 0x9000 + (int8_t)g_ppu->fetcher.tile_no * 16; // Make signed
				}

				if (!g_ppu->fetcher.is_fetching_window)
					offset = 2 * ((ly + scy) % 8);
				else
					; //offset = 2 * () //TODO

				g_ppu->fetcher.tile_data_hi = GB_Read(base + offset + 1);

				g_ppu->fetcher.state = FetcherState::PUSH; // Change state.

				if (!g_ppu->doneDummyRead) {
					g_ppu->fetcher.state = FetcherState::GET_TILE_NO;
					g_ppu->doneDummyRead = true;
				}

				break;
			}
			case FetcherState::PUSH: {
				auto bgp = GB_Internal_Read(0xFF47);

				// Push only if Background FIFO is empty.
				if (g_ppu->bgFifo.size == 0) {
					for (auto i = 0; i < 8; ++i) {
						uint8_t lo = !!(g_ppu->fetcher.tile_data_lo & (1 << (7 - i)));
						uint8_t hi = !!(g_ppu->fetcher.tile_data_hi & (1 << (7 - i))) << 1;

						FifoPixel pixel = {};
						pixel.color_id = (lo | hi);
						pixel.palette = bgp;	   // only relevant to obj sprites
						pixel.bg_priority = 0; // only relevant to obj sprites
						
						push_fifo(&g_ppu->bgFifo, pixel);
					}

					g_ppu->fetcher.x_pos += 1; // Address offset!
					
					g_ppu->fetcher.state = FetcherState::GET_TILE_NO; // Change state.
				}
				break;
			}
		}
	}

	void fetch_obj(uint16_t nth_cycle) {

	}

	void push_pixel() {
		if (g_ppu->bgFifo.size > 0) {
			FifoPixel bg_pixel = pop_fifo(&g_ppu->bgFifo);

			FifoPixel& pixel = bg_pixel;

			if (!g_ppu->clippingStarted) {
				// This is done ONCE per scanline.
				auto scx = GB_Internal_Read(0xFF43);
				g_ppu->lx = -(scx % 8);
				g_ppu->clippingStarted = true;
			}

			if (g_ppu->lx >= 0) {
				if (1) {
					// TODO ADD SPRITE MIXING

					uint8_t color_val = (pixel.palette >> (2 * pixel.color_id)) & 0b11;
					g_ppu->screenBuffer[160 * g_ppu->ly + g_ppu->lx] = RAY_COLORS[color_val];
				}
			}
			g_ppu->lx += 1;
		}
	}

	void check_lcd_interrupt() {
		auto IF = GB_Internal_Read(0xFF0F);
		uint8_t stat = GB_Internal_Read(0xFF41);
		uint8_t lyc_eq_ly = (GB_Internal_Read(0xFF45) == g_ppu->ly);

		bool stat_flag = (lyc_eq_ly && BIT_GET(stat, 6)) ||
			(g_ppu->mode == PpuMode::OAMSCAN && BIT_GET(stat, 5)) ||
			(g_ppu->mode == PpuMode::VBLANK && BIT_GET(stat, 4)) ||
			(g_ppu->mode == PpuMode::HBLANK && BIT_GET(stat, 3));

		if (!g_ppu->oldStatFlag && stat_flag)
			GB_Internal_Write(0xFF0F, BIT_SET(IF, 1)); // Request LCD Interrupt;
		g_ppu->oldStatFlag = stat_flag;


		// Update registers
		uint8_t mask = (lyc_eq_ly << 2) | (uint8_t)g_ppu->mode;
		stat = (stat & 0b11111000) | mask;
		GB_Internal_Write(0xFF41, stat);
	}
}

void InitPpu(Color* screen_buffer) {
	g_ppu = (PPU*)GB_Alloc(sizeof(PPU));

	g_ppu->oldStatFlag = false;
	g_ppu->mode = PpuMode::OAMSCAN; 
	g_ppu->ly = 0;
	g_ppu->lx = 0;
	g_ppu->elapsedDots = 0;
	g_ppu->scanlineDots = 0;

	// FIFO
	reset_fifo(&g_ppu->bgFifo);
	reset_fifo(&g_ppu->objFifo);

	// Fetcher
	g_ppu->doneDummyRead = false;
	reset_fetcher(&g_ppu->fetcher);

	// OAM
	reset_oam_stuff();

	g_ppu->clippingStarted = false;

	// Set ScreenBuffer
	g_ppu->screenBuffer = screen_buffer; 
}

void TickPpu(uint32_t cycles) {
	// Cycles are T-cycles.

	for (uint32_t i = 0; i < cycles; ++i) {

		g_ppu->elapsedDots  += 1;
		g_ppu->scanlineDots += 1;

		switch (g_ppu->mode) {
			case PpuMode::OAMSCAN: {
				if ((i & 1) == 0)
					oam_scan(); // execute every other T-cycle.

				if (g_ppu->scanlineDots == 80)
					g_ppu->mode = PpuMode::DRAWING;
				break;
			}
			case PpuMode::DRAWING: {
				fetch_bg(i);
				push_pixel();

				if (g_ppu->lx == 160) {
					// Reset stuff when entering HBLANK
					reset_fifo(&g_ppu->bgFifo);
					reset_fifo(&g_ppu->objFifo);
					reset_fetcher(&g_ppu->fetcher); g_ppu->doneDummyRead = false;
					reset_oam_stuff();

					g_ppu->lx = 0;

					// Clipping is done ONCE per scanline, therefore reset.
					g_ppu->clippingStarted = false; 

					g_ppu->mode = PpuMode::HBLANK;
				}
				break;
			}
			case PpuMode::HBLANK: {
				if (g_ppu->scanlineDots == 456) {

					//printf("Line done at: %d\n", g_ppu->elapsedDots);
					
					g_ppu->scanlineDots = 0;

					g_ppu->ly += 1;

					if (g_ppu->ly == 144) {
						auto IF = GB_Internal_Read(0xFF0F);
						GB_Internal_Write(0xFF0F, BIT_SET(IF, 0)); // Request VBlank interrupt
						g_ppu->mode = PpuMode::VBLANK;
					}
					else {
						g_ppu->mode = PpuMode::OAMSCAN;
					}
				}
				break;
			}
			case PpuMode::VBLANK: {
				if (g_ppu->scanlineDots == 456) {
					g_ppu->scanlineDots = 0;
					g_ppu->ly += 1;

					if (g_ppu->elapsedDots == 70224) {

						g_ppu->elapsedDots = 0;
						g_ppu->scanlineDots = 0;

						g_ppu->ly = 0;

						g_ppu->mode = PpuMode::OAMSCAN;
					}
				}
				break;
			}
			default:
				break;
		}
		check_lcd_interrupt();
	}

}

void DrawTiles() {
	auto bgp = GB_Internal_Read(0xFF47);

	auto base_addr = 0x8000;

	auto max_X = 24 * 8;
	auto max_Y = 16 * 8;

	uint32_t x = 0;
	uint32_t y = 0;

	/*for (base_addr; base_addr < 0x97FF; base_addr += 16) {
		for (auto j = 0; j < 8; ++j) {
			uint8_t loByte = GB_Read(base_addr + j);
			uint8_t hiByte = GB_Read(base_addr + j + 1);

			for (auto b = 0; b < 8; ++b) {
				uint8_t loBit = (loByte >> (7 - b)) & 0x01;
				uint8_t hiBit = ((hiByte >> (7 - b)) & 0x01) << 1;

				uint8_t color_id = (loBit | hiBit);
				uint8_t color_val = (bgp >> (2 * color_id)) & 0b11;

				g_ppu->screenBuffer[max_X * (y + j) + (x + b)] = RAY_COLORS[color_val];
			}
		}

		if ((x+8) == 192) {
			y += 8;
		}
		x = (x + 8) % max_X;
	}*/

	for (base_addr; base_addr < 0x97FF; base_addr += 16) {
		for (auto i = 0; i < 16; ++i) {
			printf("%02x ", GB_Read(base_addr + i));
		}
		printf("\n");
	}

	printf("\n\n");
}