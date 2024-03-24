#include "ppu.h"

#include "bus.h"

#include "utils.h"

#include <stdio.h>

#include <assert.h>

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

	inline uint8_t get_sprite_height() {
		return BIT_GET(GB_Internal_Read(0xFF40), 2) ? 16 : 8;
	}

	inline bool sprites_enabled() {
		return BIT_GET(GB_Internal_Read(0xFF40), 1);
	}

	inline bool sprite_wins(FifoPixel* spritePixel, FifoPixel* bgPixel) {
		// Returns true if sprite pixel should be put on the screen.
		if (spritePixel->color_id == 0)
			return false;
		if (spritePixel->bg_priority == 1 && bgPixel->color_id != 0)
			return false;
		
		return true;
	}

	Sprite pop_spriteBuffer(SpriteBuffer* obj_buffer) {
		assert(obj_buffer->size > 0);

		Sprite out = obj_buffer->buff[obj_buffer->head++];
		obj_buffer->size--;

		if (obj_buffer->size == 0)
			obj_buffer->head = 0;

		return out;
	}

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
		g_ppu->spriteBuff.head = 0;
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

		if (xPos <= 0)
			return;
		if (ly + 16 < yPos)
			return;
		if (ly + 16 >= yPos + spriteHeight)
			return;
		if (g_ppu->spriteBuff.size >= MAX_VISIBLE_SPRITES)
			return;

		// Push to SpriteBuffer
		Sprite obj;
		obj.posY = yPos; obj.posX = xPos; obj.tileIndx = tileIndx; obj.flags = flags;

		g_ppu->spriteBuff.buff[g_ppu->spriteBuff.size] = obj;
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
				
				g_ppu->fetcher.tile_no = GB_Read(base + offset);
				//g_ppu->fetcher.tile_no = GB_Read(base | offset);

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

	void fetch_sprites(uint16_t nth_cycle) {
		switch (g_ppu->spriteFetcher.state) {
			case FetcherState::GET_TILE_NO: {
				if (nth_cycle & 0x1) { return; }
				g_ppu->spriteFetcher.state = FetcherState::GET_LO;
				break;
			}
			case FetcherState::GET_LO: {
				if (nth_cycle & 0x1) { return; }
				g_ppu->spriteFetcher.state = FetcherState::GET_HI;
				break;
			}
			case FetcherState::GET_HI: {
				if (nth_cycle & 0x1) { return; }
				g_ppu->spriteFetcher.state = FetcherState::PUSH;
				break;
			}
			case FetcherState::PUSH: {
				Sprite obj = pop_spriteBuffer(&g_ppu->spriteBuff);

				auto spriteHeight = get_sprite_height();
				uint8_t spriteFlags = obj.flags;
				bool yflip = BIT_GET(spriteFlags, 6);
				bool xflip = BIT_GET(spriteFlags, 5);

				// TODO
				uint16_t base = 0x8000;
				if (spriteHeight == 8) {
					base += obj.tileIndx * 16;
				}
				else {
					// 8x16 sprite
					if (obj.posY - 8 <= g_ppu->ly) {
						base += (obj.tileIndx | 0x01) * 16;
					}
					else if (yflip) {
						base += (obj.tileIndx | 0x01) * 16;
					}
					else {
						base += (obj.tileIndx & 0xFE) * 16;
					}
				}
			
				uint16_t offset = ((int16_t)g_ppu->ly - obj.posY) & 7;
				if (yflip == true)
					offset = ~offset;
				offset = (offset & 0b111) << 1;

				g_ppu->spriteFetcher.tile_no = obj.tileIndx;
				g_ppu->spriteFetcher.tile_data_lo = GB_Read(base + offset);
				g_ppu->spriteFetcher.tile_data_hi = GB_Read(base + offset + 1);

				for (auto i = 0; i < 8; ++i) {
					uint8_t shift = (xflip == true) ? i : (7 - i);

					uint8_t lo = (g_ppu->spriteFetcher.tile_data_lo >> shift) & 0x1;
					uint8_t hi = ((g_ppu->spriteFetcher.tile_data_hi >> shift) & 0x1) << 1;

					FifoPixel pixel     = {};
					pixel.color_id      = (lo | hi);
					pixel.palette		= BIT_GET(obj.flags, 4) ? GB_Internal_Read(0xFF49) : GB_Internal_Read(0xFF48);
					pixel.bg_priority   = BIT_GET(obj.flags, 7); 

					if (obj.posX + i - 8 >= g_ppu->lx) {
						if (i >= g_ppu->objFifo.size)
							push_fifo(&g_ppu->objFifo, pixel);
						else if (g_ppu->objFifo.buff[i].color_id == 0)
							g_ppu->objFifo.buff[i] = pixel;
					}
				}

				// TODO check this condition.
				g_ppu->fetchingSprite = (g_ppu->spriteBuff.size != 0) && (g_ppu->spriteBuff.buff[0].posX != obj.posX);

				g_ppu->spriteFetcher.state = FetcherState::GET_TILE_NO;
				break;
			}
		}
	}

	void push_pixel() {
		if (g_ppu->bgFifo.size > 0) {
			bool hasSprite = false;

			FifoPixel bg_pixel =  pop_fifo(&g_ppu->bgFifo);
			FifoPixel obj_pixel;
			if (g_ppu->objFifo.size > 0) {
				obj_pixel = pop_fifo(&g_ppu->objFifo);
				hasSprite = true;
			}

			FifoPixel* pixel = nullptr;

			if (!g_ppu->clippingStarted) {
				// This is done ONCE per scanline.
				auto scx = GB_Internal_Read(0xFF43);
				g_ppu->lx = -(scx % 8);
				g_ppu->clippingStarted = true;
			}

			if (g_ppu->lx >= 0) {
				if (hasSprite && sprite_wins(&obj_pixel, &bg_pixel)) {
					pixel = &obj_pixel;
				}
				else {
					pixel = &bg_pixel;
				}

				uint8_t color_val = (pixel->palette >> (2 * pixel->color_id)) & 0b11;
				g_ppu->screenBuffer[160 * g_ppu->ly + g_ppu->lx] = RAY_COLORS[color_val];
			}
			g_ppu->lx += 1;

			// Check if sprites should be fetched
			auto spriteX = g_ppu->spriteBuff.buff[0].posX;
			if (sprites_enabled() && g_ppu->spriteBuff.size > 0 && g_ppu->lx + 8 >= spriteX) {
				g_ppu->fetchingSprite = true;
				g_ppu->spriteFetcher.state = FetcherState::GET_TILE_NO;
				
				// Reset BG fetcher
				g_ppu->fetcher.state = FetcherState::GET_TILE_NO;
			}
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

		// Update LY
		GB_Internal_Write(0xFF44, g_ppu->ly);
	}
}

void ResetLcd() {
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

	// Sprite Fetcher
	bool fetchingSprite = false;
	reset_fetcher(&g_ppu->spriteFetcher);

	// OAM
	reset_oam_stuff();

	g_ppu->clippingStarted = false;
}

void InitPpu(Color* screen_buffer) {
	g_ppu = (PPU*)GB_Alloc(sizeof(PPU));

	ResetLcd();

	// Set ScreenBuffer
	g_ppu->screenBuffer = screen_buffer;
}

void TickPpu(uint32_t cycles) {
	// Cycles are T-cycles.

	// Check if LCD/PPU is enabled
	if (BIT_GET(GB_Internal_Read(0xFF40), 7) == 0){
		// DISABLED
		uint8_t stat = GB_Internal_Read(0xFF41);
		stat &= 0b11111100;
		stat |= PpuMode::HBLANK;
		GB_Internal_Write(0xFF41, stat);
		return;
	}
	else {
		// ENABLED
		for (uint32_t i = 0; i < cycles; ++i) {
			g_ppu->elapsedDots += 1;
			g_ppu->scanlineDots += 1;

			switch (g_ppu->mode) {
			case PpuMode::OAMSCAN: {
				if ((i & 1) == 0)
					oam_scan(); // execute every other T-cycle.

				if (g_ppu->scanlineDots == 80) {
					g_ppu->mode = PpuMode::DRAWING;

					// Sort entries in spriteBuffer;
					// Insertion sort is used here.
					for (auto left = 0; left < g_ppu->spriteBuff.size; ++left) {
						auto right = left;
						while (right > 0 && g_ppu->spriteBuff.buff[right - 1].posX > g_ppu->spriteBuff.buff[right].posX) {
							auto temp = g_ppu->spriteBuff.buff[right];
							g_ppu->spriteBuff.buff[right] = g_ppu->spriteBuff.buff[right - 1];
							g_ppu->spriteBuff.buff[right - 1] = temp;
							right -= 1;
						}
					}
				}
				break;
			}
			case PpuMode::DRAWING: {
				bool fetching_sprite = g_ppu->fetchingSprite;
				if(!fetching_sprite)
					fetch_bg(i);
				if (fetching_sprite)
					fetch_sprites(i);
				if (!fetching_sprite)
					push_pixel();

				if (g_ppu->lx == 160) {
					// Reset stuff when entering HBLANK
					reset_fifo(&g_ppu->bgFifo);
					reset_fifo(&g_ppu->objFifo);
					reset_fetcher(&g_ppu->fetcher); g_ppu->doneDummyRead = false;
					reset_oam_stuff();

					// Reset sprite fetcher
					reset_fetcher(&g_ppu->spriteFetcher);
					g_ppu->fetchingSprite = false;

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

}
