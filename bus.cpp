#include "bus.h"
#include "timer.h"
#include "ppu.h"
#include "joypad.h"

#include <stdio.h>

namespace {
	uint32_t ramSizes[] = { 0, 0, 8192, 32768, 131072, 65536 };

	Bus* g_bus = nullptr;
	Cartridge* g_cart = nullptr;
	MBC_TYPE g_mbc_type = MBC0;
	
	bool g_bios_enable = true;

	uint8_t boot_rom[0x100] = {
		0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32, 0xcb, 0x7c, 0x20, 0xfb, 0x21, 0x26, 0xff, 0xe,
		0x11, 0x3e, 0x80, 0x32, 0xe2, 0xc, 0x3e, 0xf3, 0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0,
		0x47, 0x11, 0x4, 0x1, 0x21, 0x10, 0x80, 0x1a, 0xcd, 0x95, 0x0, 0xcd, 0x96, 0x0, 0x13, 0x7b,
		0xfe, 0x34, 0x20, 0xf3, 0x11, 0xd8, 0x0, 0x6, 0x8, 0x1a, 0x13, 0x22, 0x23, 0x5, 0x20, 0xf9,
		0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99, 0xe, 0xc, 0x3d, 0x28, 0x8, 0x32, 0xd, 0x20,
		0xf9, 0x2e, 0xf, 0x18, 0xf3, 0x67, 0x3e, 0x64, 0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x4,
		0x1e, 0x2, 0xe, 0xc, 0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa, 0xd, 0x20, 0xf7, 0x1d, 0x20, 0xf2,
		0xe, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62, 0x28, 0x6, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x6,
		0x7b, 0xe2, 0xc, 0x3e, 0x87, 0xe2, 0xf0, 0x42, 0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x5, 0x20,
		0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x6, 0x4, 0xc5, 0xcb, 0x11, 0x17, 0xc1, 0xcb, 0x11, 0x17,
		0x5, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9, 0xce, 0xed, 0x66, 0x66, 0xcc, 0xd, 0x0, 0xb,
		0x3, 0x73, 0x0, 0x83, 0x0, 0xc, 0x0, 0xd, 0x0, 0x8, 0x11, 0x1f, 0x88, 0x89, 0x0, 0xe,
		0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63, 0x6e, 0xe, 0xec, 0xcc,
		0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e, 0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c,
		0x21, 0x4, 0x1, 0x11, 0xa8, 0x0, 0x1a, 0x13, 0xbe, 0x20, 0xfe, 0x23, 0x7d, 0xfe, 0x34, 0x20,
		0xf5, 0x6, 0x19, 0x78, 0x86, 0x23, 0x5, 0x20, 0xfb, 0x86, 0x20, 0xfe, 0x3e, 0x1, 0xe0, 0x50
	};

	uint8_t read_mbc1(uint16_t addr) {
		// Addr is in range [0x0000, 0x7FFF] or [0xA000, 0xBFFF]
		switch ((addr >> 12) & 0xF) 
		{
			// Reading from ROM
			case 0x0:
			case 0x1:
			case 0x2:
			case 0x3: {
				if (g_cart->banking_mode == 0b0) {
					return g_cart->rom[addr & 0x3FFF];
				}
				uint32_t mask = (uint32_t)(g_cart->nRamBank << 19);
				return g_cart->rom[mask | (addr & 0x3FFF)];
				//break;
			}
			case 0x4:
			case 0x5:
			case 0x6:
			case 0x7:
			{
				uint32_t mask = (uint32_t)(g_cart->nRamBank << 19) | (uint32_t)(g_cart->nRomBank << 14);
				return g_cart->rom[mask | (addr & 0x3FFF)];
				//break;
			}
			default:
			{
				// Reading from RAM
				if (g_cart->ramEnabled) {
					uint16_t lowerAddr = addr & 0x1FFF;
					if (g_cart->banking_mode == 0b0) {
						return g_cart->ram[lowerAddr];
					}
					return g_cart->ram[(uint16_t)(g_cart->nRamBank << 13) | lowerAddr];
				}
				else {
					return 0xFF;
				}
				//break;
			}
		}
	}
	void write_mbc1(uint16_t addr, uint8_t data) {
		// Addr is in range [0x0000, 0x7FFF] or [0xA000, 0xBFFF]
		switch ((addr >> 12) & 0xF) {
			case 0x0:
			case 0x1:
			{
				uint8_t lowerBits = data & 0x0F;
				if (lowerBits == 0xA)
					g_cart->ramEnabled = true;
				else
					g_cart->ramEnabled = false;
				break;
			}
			case 0x2:
			case 0x3:
			{
				uint8_t lowerBits = data & 0b00011111;
				if (lowerBits == 0) 
					g_cart->nRomBank = 1;
				else
					g_cart->nRomBank = lowerBits;
				break;
			}
			case 0x4:
			case 0x5:
			{
				g_cart->nRamBank = data & 0b11;
				break;
			}
			case 0x6:
			case 0x7:
			{
				g_cart->banking_mode = data & 0x01;
				break;
			}
			default:
			{
				// Writing to RAM
				if (g_cart->ramEnabled) {
					uint16_t lowerAddr = addr & 0x1FFF; // lower 13 bits
					if (g_cart->banking_mode == 0b0) {
						g_cart->ram[lowerAddr] = data;
					}
					else {
						g_cart->ram[uint16_t(g_cart->nRamBank << 13) | lowerAddr] = data;
					}
				}
				break;
			}
		}
	}
}

void InitBus(uint8_t* memory) {
	g_bus = (Bus*)GB_Alloc(sizeof(Bus));
	g_bus->memory = memory;
}

void InitCartridge(uint8_t* cartData, size_t size) {
	g_cart = (Cartridge*)GB_Alloc(sizeof(Cartridge));

	g_cart->ramEnabled = false;
	g_cart->rom = cartData;


	if (g_cart->rom[0x147] == 0x00) {
		// MBC0
		printf("Cartridge type = MBC0\n");
		
		g_mbc_type = MBC_TYPE::MBC0;
		return;
	}
	if (g_cart->rom[0x147] >= 0x01 && g_cart->rom[0x147] <= 0x03) {
		// MBC1
		printf("Cartridge type = MBC1\n");

		g_mbc_type = MBC_TYPE::MBC1;

		g_cart->banking_mode = 0b0;
		g_cart->nRomBank = 0x01;
		g_cart->nRamBank = 0b00;

		if (g_cart->rom[0x149]) {
			g_cart->ram = (uint8_t*)GB_Alloc(ramSizes[g_cart->rom[0x149]]);
		}
		return;
	}

	// Unimplemented cartridge types.
	// TODO change this.
	printf("Unimplemented Cartridge Type!\n");
	exit(1);
}

uint8_t GB_Read(uint16_t addr) {
	switch (addr >> 12) {
		// (addr <= 0x7FFF) or (0xA000 <= addr <= 0xBFFF)
		case 0x0:
#ifdef BIOS_ENABLE
		{
			if (g_bios_enable && addr < 0x100)
				return boot_rom[addr];
		}
#endif
		case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7:
		case 0xA: case 0xB:
		{
			switch (g_mbc_type)
			{
			case MBC0:
				// TODO CHECK THIS.
				return g_cart->rom[addr];
			case MBC1: return read_mbc1(addr);
			case MBC2:
				//TODO
				break;
			case MBC3:
				//TODO
				break;
			default:
				break;
			}

			// TODO change this somehow
			return -1;
			break;
		}
		default:
		{
			switch (addr) {
				case 0xFF00: {
					if (((g_bus->memory[addr] >> 5) & 0b1) == 0) {
						return (g_bus->memory[addr] & 0xF0) | GetActionButtons();
					}
					if (((g_bus->memory[addr] >> 4) & 0b1) == 0) {
						return (g_bus->memory[addr] & 0xF0) | GetDirectionButtons();
					}
					return (g_bus->memory[addr] | 0x0F);
				}
				case 0xFF44: {
					// Reading LY
					return GetPpuLy();
				}
				default: {
					return g_bus->memory[addr];
				}
			}

			break;
		}
	}
}

void GB_Write(uint16_t addr, uint8_t data) {
	switch (addr >> 12) {
		// (addr <= 0x7FFF) or (0xA000 <= addr <= 0xBFFF)
		case 0x0: case 0x1: case 0x2: case 0x3: //0000	3FFF
		case 0x4: case 0x5: case 0x6: case 0x7: //4000	7FFF	
		case 0xA: case 0xB:
		{
			switch (g_mbc_type)
			{
			case MBC0:
				break;
			case MBC1: write_mbc1(addr, data);
			case MBC2:
				//TODO
				break;
			case MBC3:
				//TODO
				break;
			default:
				break;
			}
			break;
		}
		default:
		{
			switch (addr)
			{
				case 0xFF00: {
					// Joypad
					// Lower nibble is Read-only
					data &= 0xF0;
					g_bus->memory[addr] = (g_bus->memory[addr] & 0x0F) | data;
				}
				case 0xFF02: {
					// FOR TEST!! Blargg
					if (data == 0x81)
						printf("%c", g_bus->memory[0xFF01]);
					g_bus->memory[addr] = data;
					break;
				}
				case 0xFF04: {
					// Set DIV to 0
					ResetDiv();
					g_bus->memory[addr] = 0;
					break;
				}
				case 0xFF40: {
					// If Bit 7 of data is 0, Reset PPU/LCD
					if ((data >> 7) == 1 && (g_bus->memory[addr] >> 7) == 0) {
						ResetLcd();
					}
					g_bus->memory[addr] = data;

					CheckLcdInterrupt();
					break;
				}
				case 0xFF41: {
					// STAT
					data &= 0b01111000;
					g_bus->memory[addr] = (g_bus->memory[addr] & 0b10000111) | data;

					CheckLcdInterrupt();
					break;
				}
				case 0xFF44: {
					// LY (Read-only)
					break;
				}
				case 0xFF45: {
					// LYC
					g_bus->memory[addr] = data;

					CheckLcdInterrupt();
					break;
				}
				case 0xFF46: {
					// OAM DMA Tranfser start
					// TODO
					// Maybe optimize by fetching pointer???
					uint16_t from_base = (uint16_t)data << 8;
					uint16_t to_base = 0xFE00;
					for (uint8_t i = 0; i < 160; ++i) {
						GB_Internal_Write(to_base + i, GB_Read(from_base + i));
					}

					g_bus->memory[addr] = data;
					break;
				}
				case 0xFF50: {
					g_bios_enable = false;
					break;
				}
				default: {
					g_bus->memory[addr] = data;
					break;
				}
			}
			break;
		}
	}
}


void GB_Internal_Write(uint16_t addr, uint8_t data) {
	// Use with CAUTION!
	// Dont write to RAMs, ROMs
	g_bus->memory[addr] = data;
}

uint8_t GB_Internal_Read(uint16_t addr) {
	// Use with CAUTION!
	return g_bus->memory[addr];
}