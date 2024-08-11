#include "ppu.h"
#include "bus.h"
#include "utils.h"
#include <stdio.h>

#include <assert.h>

namespace {
	PPU* g_ppu;

	Color RAY_COLORS[] = {
		/*{255,255,255,255},
		{169,169,169,255},
		{84,84,84,255},
		{0,0,0,255}*/
		{175,203,70,255},
		{121,170,109,255},
		{34,111,95,255},
		{8,41,85,255}
	};

	uint8_t scanline_color_ids[160];
}

namespace {

	inline void change_mode_to(PpuMode mode) {
		g_ppu->mode = mode;
		CheckLcdInterrupt();
	}

	inline uint8_t get_sprite_height() {
		return BIT_GET(GB_Internal_Read(0xFF40), 2) ? 16 : 8;
	}

	inline bool sprites_enabled() {
		return BIT_GET(GB_Internal_Read(0xFF40), 1);
	}

	Sprite pop_spriteBuffer(SpriteBuffer* obj_buffer) {
		assert(obj_buffer->size > 0);

		Sprite out = obj_buffer->buff[obj_buffer->head++];
		obj_buffer->size--;

		if (obj_buffer->size == 0)
			obj_buffer->head = 0;

		return out;
	}

	void get_sprites() {
		g_ppu->spriteBuff.size = 0;
		g_ppu->spriteBuff.head = 0;

		uint8_t ly = g_ppu->ly;
		uint8_t spriteHeight = get_sprite_height();

		// Sprite attributes
		uint8_t yPos = 0;
		uint8_t xPos = 0;
		uint8_t tileIndx = 0;
		uint8_t flags = 0;

		for (auto i = 0; i < 160; i += 4) {
			yPos			= GB_Read(0xFE00 + i + 0);
			xPos			= GB_Read(0xFE00 + i + 1);
			tileIndx		= GB_Read(0xFE00 + i + 2);
			flags			= GB_Read(0xFE00 + i + 3);

			if (xPos == 0)
				continue;
			if (ly + 16 < yPos)
				continue;
			if (ly + 16 >= yPos + spriteHeight)
				continue;
			if (g_ppu->spriteBuff.size == MAX_VISIBLE_SPRITES)
				break;

			// Push to SpriteBuffer
			Sprite obj;
			obj.posY = yPos; obj.posX = xPos; obj.tileIndx = tileIndx; obj.flags = flags;

			g_ppu->spriteBuff.buff[g_ppu->spriteBuff.size] = obj;
			g_ppu->spriteBuff.size += 1;
		}

		// Sort the entries in sprite buffer.
		// Insertion sort is used here.
		for (int left = 0; left < g_ppu->spriteBuff.size; ++left) {
			int right = left;
			while (right > 0 && g_ppu->spriteBuff.buff[right - 1].posX > g_ppu->spriteBuff.buff[right].posX) {
				Sprite temp = g_ppu->spriteBuff.buff[right];
				g_ppu->spriteBuff.buff[right] = g_ppu->spriteBuff.buff[right - 1];
				g_ppu->spriteBuff.buff[right - 1] = temp;
				right -= 1;
			}
		}

	}

	void draw_scanline() {
		// TODO

		uint8_t lcdc = GB_Internal_Read(0xFF40);

		bool bg_enabled = BIT_GET(lcdc, 0);
		bool window_enabled = BIT_GET(lcdc, 5);

		uint16_t window_map = BIT_GET(lcdc, 6) ? 0x9C00 : 0x9800;
		uint16_t bg_map = BIT_GET(lcdc, 3) ? 0x9C00 : 0x9800;

		uint16_t bg_tile_area;
		bool signed_adressing = false;

		if (BIT_GET(lcdc, 4) == 1) {
			bg_tile_area = 0x8000;
		}
		else {
			bg_tile_area = 0x9000;
			signed_adressing = true;
		}

		uint8_t bgp = GB_Internal_Read(0xFF47);

		uint8_t scy = GB_Internal_Read(0xFF42);
		uint8_t scx = GB_Internal_Read(0xFF43);

		uint8_t loByte, hiByte;
		uint8_t	loBit, hiBit;
		uint8_t color_id, color_val;

		uint16_t tile_id_addr, tile_addr, offset;
		uint8_t	tile_id;

		uint16_t tile_row = (g_ppu->ly + scy) % 8;

		uint8_t wx = GB_Internal_Read(0xFF4B);

		bool increment_wind_line = false;

		for (int w = 0; w < 160; ++w) {
			if (window_enabled && g_ppu->wylyCondition && w + 7 >= wx) {

				increment_wind_line = true;

				offset = (((w + 7 - wx) / 8) & 0x1f) + (((g_ppu->windowLineCounter / 8) & 0x1F) * 32);
				offset &= 0x3ff;
				tile_id_addr = window_map + offset;

				tile_id = GB_Read(tile_id_addr);

				if (signed_adressing)
					tile_addr = bg_tile_area + (16 * (int8_t)tile_id) + (tile_row * 2);
				else
					tile_addr = bg_tile_area + (16 * tile_id) + (tile_row * 2);


				loByte = GB_Read(tile_addr);
				hiByte = GB_Read(tile_addr + 1);

				loBit = (loByte >> (7 - ((w + 7 - wx) & 7))) & 0x01;
				hiBit = (hiByte >> (7 - ((w + 7 - wx) & 7))) & 0x01;

				color_id = (hiBit << 1) | loBit;
				color_val = (bgp >> (2 * color_id)) & 0b11;

				scanline_color_ids[w] = color_id;

				g_ppu->screenBuffer[g_ppu->ly * 160 + w] = RAY_COLORS[color_val];
			}
			else if (bg_enabled) {

				offset = (((w + scx) / 8) & 0x1f) + ((((g_ppu->ly + scy) / 8) & 0x1F) * 32);
				offset &= 0x3ff;
				tile_id_addr = bg_map + offset;

				tile_id = GB_Read(tile_id_addr);

				if (signed_adressing)
					tile_addr = bg_tile_area + (16 * (int8_t)tile_id) + (tile_row * 2);
				else
					tile_addr = bg_tile_area + (16 * tile_id) + (tile_row * 2);


				loByte = GB_Read(tile_addr);
				hiByte = GB_Read(tile_addr + 1);

				loBit = (loByte >> (7 - ((w + scx) & 7))) & 0x01;
				hiBit = (hiByte >> (7 - ((w + scx) & 7))) & 0x01;

				color_id = (hiBit << 1) | loBit;
				color_val = (bgp >> (2 * color_id)) & 0b11;

				scanline_color_ids[w] = color_id;

				g_ppu->screenBuffer[g_ppu->ly * 160 + w] = RAY_COLORS[color_val];
			}
		}


		if (increment_wind_line)
			g_ppu->windowLineCounter += 1;


		if (sprites_enabled() == true) {

			get_sprites();

			int sprite_x, sprite_y;

			uint8_t spriteHeight;
			uint8_t spriteFlags;
			bool yflip, xflip;

			uint16_t sprite_base;
			uint16_t sprite_offset;

			uint8_t palette, bg_priority;

			auto n_sprites = g_ppu->spriteBuff.size;

			for (auto _ = 0; _ < n_sprites; ++_) {
				Sprite obj = pop_spriteBuffer(&g_ppu->spriteBuff);

				sprite_y = obj.posY - 16;

				spriteHeight = get_sprite_height();
				spriteFlags = obj.flags;
				yflip = BIT_GET(spriteFlags, 6);
				xflip = BIT_GET(spriteFlags, 5);

				sprite_base = 0x8000;

				if (spriteHeight == 8) {
					sprite_base += obj.tileIndx * 16;
				}
				else {
					// 8x16 sprite
					if (sprite_y + 8 <= g_ppu->ly) {
						sprite_base += (obj.tileIndx | 0x01) * 16;
					}
					else if (yflip) {
						sprite_base += (obj.tileIndx | 0x01) * 16;
					}
					else {
						sprite_base += (obj.tileIndx & 0xFE) * 16;
					}
				}

				sprite_offset = ((int16_t)g_ppu->ly - sprite_y) & 7;
				if (yflip == true)
					sprite_offset = ~sprite_offset;
				sprite_offset = (sprite_offset & 0b111) << 1;

				loByte = GB_Read(sprite_base + sprite_offset);
				hiByte = GB_Read(sprite_base + sprite_offset + 1);

				for (auto col = 0; col < 8; ++col) {

					sprite_x = col + obj.posX - 8; // Real X position of sprite pixel.

					// Skip sprites that are out of screen.
					if (sprite_x < 0 || sprite_x >= 160)
						continue;

					uint8_t shift = (xflip == true) ? col : (7 - col);

					loBit = (loByte >> shift) & 0x1;
					hiBit = (hiByte >> shift) & 0x1;

					palette = BIT_GET(obj.flags, 4) ? GB_Internal_Read(0xFF49) : GB_Internal_Read(0xFF48);
					bg_priority = BIT_GET(obj.flags, 7);

					color_id = (hiBit << 1) | loBit;
					color_val = (palette >> (2 * color_id)) & 0b11;

					if (color_id > 0) {
						if (bg_priority == 0 || (scanline_color_ids[sprite_x] == 0)) {
							g_ppu->screenBuffer[g_ppu->ly * 160 + sprite_x] = RAY_COLORS[color_val];
						}
					}
				}

			}
		}
	}

}

void CheckLcdInterrupt() {
	// Function checks for LCD related interrupts
	// It also updates STAT register (updates Mode and LYC bits).

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

void ResetLcd() {
	g_ppu->oldStatFlag = false;
	change_mode_to(PpuMode::OAMSCAN);
	g_ppu->ly = 0;
	g_ppu->elapsedDots = 0;
	g_ppu->scanlineDots = 0;

	// Window related stuff
	g_ppu->windowLineCounter = 0;
	g_ppu->wylyCondition = false;
}

PPU* InitPpu(Color* screen_buffer, void (*RenderFrame)(void)) {
	g_ppu = (PPU*)GB_Alloc(sizeof(PPU));


	//------------Init stuff------------------
	g_ppu->oldStatFlag = false;
	g_ppu->mode = PpuMode::OAMSCAN;
	g_ppu->ly = 0;
	g_ppu->elapsedDots = 0;
	g_ppu->scanlineDots = 0;
	
	// Window related stuff
	g_ppu->windowLineCounter = 0;
	g_ppu->wylyCondition = false;
	//---------------------------------------


	// Set Scanline Color buffer
	memset(scanline_color_ids, 0, 160);

	// Set ScreenBuffer
	g_ppu->screenBuffer = screen_buffer;

	// Draw Callback
	g_ppu->RenderFrame = RenderFrame;

	return g_ppu;
}

void TickPpu(uint32_t cycles) {
	// Cycles are T-cycles.

	// Check if LCD/PPU is enabled
	if (BIT_GET(GB_Internal_Read(0xFF40), 7) == 0){
		// DISABLED
		
		// Reset stuff
		g_ppu->ly = 0;
		change_mode_to(PpuMode::HBLANK);
		g_ppu->elapsedDots = 0;
		g_ppu->scanlineDots = 0;

		return;
	}
	else {
		// ENABLED
		for (uint32_t i = 0; i < cycles; ++i) {
			g_ppu->elapsedDots += 1;
			g_ppu->scanlineDots += 1;

			switch (g_ppu->mode) {
				case PpuMode::OAMSCAN: {
					if (g_ppu->scanlineDots == 80) {
						change_mode_to(PpuMode::DRAWING);

						// Check if WY equals LY 
						if (g_ppu->ly == GB_Internal_Read(0xFF4A))
							g_ppu->wylyCondition = true;
					}
					break;
				}
				case PpuMode::DRAWING: {
					if (g_ppu->scanlineDots == 252) {
						draw_scanline();
						change_mode_to(PpuMode::HBLANK);
					}
					break;
				}
				case PpuMode::HBLANK: {
					if (g_ppu->scanlineDots == 456) {

						g_ppu->scanlineDots = 0;
						g_ppu->ly += 1;

						if (g_ppu->ly == 144) {
							auto IF = GB_Internal_Read(0xFF0F);
							BIT_SET(IF, 0);
							GB_Internal_Write(0xFF0F, IF); // Request VBlank interrupt
							change_mode_to(PpuMode::VBLANK);
						}
						else {
							change_mode_to(PpuMode::OAMSCAN);
						}
					}
					break;
				}
				case PpuMode::VBLANK: {
					if (g_ppu->scanlineDots == 456) {
						//----------------
						// RESET stuff.
						g_ppu->wylyCondition = false;
						g_ppu->windowLineCounter = 0;

						g_ppu->scanlineDots = 0;
						g_ppu->ly += 1;

						if (g_ppu->elapsedDots == 70224) {
							// Do Actual Drawing (update texture).
							g_ppu->RenderFrame();

							g_ppu->elapsedDots = 0;
							g_ppu->scanlineDots = 0;

							g_ppu->ly = 0;

							change_mode_to(PpuMode::OAMSCAN);
						}

						CheckLcdInterrupt();
					}
					break;
				}
				default:
					break;
				}
			//check_lcd_interrupt();
		}
	}

}

uint8_t GetPpuLy() {
	return g_ppu->ly;
}