#define _CRT_SECURE_NO_WARNINGS

#include "emulator.h"
#include "allocator.h"
#include <stdio.h>

#include "bus.h"

#include "utils.h"

#ifdef LOGGING_ENABLED
#include <iostream>
#include <string>
#endif

namespace {
    Gameboy* g_gameboy = nullptr;
}

void InitGameboy(Color* screenBuffer) {
    if (g_gameboy)
        return;

    g_gameboy = (Gameboy*)GB_Alloc(sizeof(Gameboy));

    // CPU
    g_gameboy->cpu = InitCpu();

    // PPU
    g_gameboy->ppu = InitPpu(screenBuffer);

    // Timer
    InitTimer(&g_gameboy->timer);

    // Bus
    InitBus(g_gameboy->memory);

    // Variable needed to delay effect of EI instruction.
    g_gameboy->eiDelayed = false;

    // Joypad
    InitJoypad(&g_gameboy->joypad);

    //for (int i = 0; i < 0x10000; ++i) { g_gameboy->memory[i] = 0; }
    g_gameboy->memory[0xFF00] = 0xCF;
    g_gameboy->memory[0xFF01] = 0x00;
    g_gameboy->memory[0xFF02] = 0x7E;
    g_gameboy->memory[0xFF04] = 0xAB;
    g_gameboy->memory[0xFF05] = 0x00;
    g_gameboy->memory[0xFF06] = 0x00;
    g_gameboy->memory[0xFF07] = 0xF8;
    g_gameboy->memory[0xFF0F] = 0xE1;
    g_gameboy->memory[0xFF10] = 0x80;
    g_gameboy->memory[0xFF11] = 0xBF;
    g_gameboy->memory[0xFF12] = 0xF3;
    g_gameboy->memory[0xFF13] = 0xFF;
    g_gameboy->memory[0xFF14] = 0xBF;
    g_gameboy->memory[0xFF16] = 0x3F;
    g_gameboy->memory[0xFF17] = 0x00;
    g_gameboy->memory[0xFF18] = 0xFF;
    g_gameboy->memory[0xFF19] = 0xBF;
    g_gameboy->memory[0xFF1A] = 0x7F;
    g_gameboy->memory[0xFF1B] = 0xFF;
    g_gameboy->memory[0xFF1C] = 0x9F;
    g_gameboy->memory[0xFF1D] = 0xFF;
    g_gameboy->memory[0xFF1E] = 0xBF;
    g_gameboy->memory[0xFF20] = 0xFF;
    g_gameboy->memory[0xFF21] = 0x00;
    g_gameboy->memory[0xFF22] = 0x00;
    g_gameboy->memory[0xFF23] = 0xBF;
    g_gameboy->memory[0xFF24] = 0x77;
    g_gameboy->memory[0xFF25] = 0xF3;
    g_gameboy->memory[0xFF26] = 0xF1;
    g_gameboy->memory[0xFF40] = 0x91;
    g_gameboy->memory[0xFF41] = 0x85;
    g_gameboy->memory[0xFF42] = 0x00;
    g_gameboy->memory[0xFF43] = 0x00;
    g_gameboy->memory[0xFF44] = 0x00;
    g_gameboy->memory[0xFF45] = 0x00;
    g_gameboy->memory[0xFF46] = 0xFF;
    g_gameboy->memory[0xFF47] = 0xFC;
    g_gameboy->memory[0xFF4A] = 0x00;
    g_gameboy->memory[0xFF4B] = 0x00;
    g_gameboy->memory[0xFF4D] = 0xFF;
    g_gameboy->memory[0xFF4F] = 0xFF;
    g_gameboy->memory[0xFF51] = 0xFF;
    g_gameboy->memory[0xFF52] = 0xFF;
    g_gameboy->memory[0xFF53] = 0xFF;
    g_gameboy->memory[0xFF54] = 0xFF;
    g_gameboy->memory[0xFF55] = 0xFF;
    g_gameboy->memory[0xFF56] = 0xFF;
    g_gameboy->memory[0xFF68] = 0xFF;
    g_gameboy->memory[0xFF69] = 0xFF;
    g_gameboy->memory[0xFF6A] = 0xFF;
    g_gameboy->memory[0xFF6B] = 0xFF;
    g_gameboy->memory[0xFF70] = 0xFF;
    g_gameboy->memory[0xFFFF] = 0x00;
}


#ifdef LOGGING_ENABLED
std::string LogRunGameboy() {
    std::string out;

    uint32_t local_t_cycles = 0;

    if (g_gameboy->cpu->imeRequested && !g_gameboy->cpu->isStopped) {
        if (g_gameboy->eiDelayed) {
            g_gameboy->cpu->IME = 1;
            g_gameboy->eiDelayed = false;
            g_gameboy->cpu->imeRequested = false;
        }
        else
            g_gameboy->eiDelayed = true;
    }

    local_t_cycles = 0;
    local_t_cycles += HandleInterrupts(g_gameboy->cpu);

    if (!g_gameboy->cpu->isStopped)
        local_t_cycles += LogTickCpu(g_gameboy->cpu, out);
    else
        local_t_cycles += 4;

    TickTimer(local_t_cycles);
    TickPpu(local_t_cycles);

    return out;
}

Gameboy* GetGameboyState() {
    return g_gameboy;
}
#endif



void InsertCartridge(const char* path) {
    FILE* fd = nullptr;
    size_t rom_size = 0;

    fd = fopen(path, "rb");
    if (!fd) {
        printf("Cannot open ROM!\n");
        exit(1);
    }

    fseek(fd, 0, SEEK_END);
    rom_size = ftell(fd);
    rewind(fd);

    uint8_t* rom_data = (uint8_t*)GB_Alloc(rom_size);
    fread(rom_data, sizeof(uint8_t), rom_size, fd);

    fclose(fd);

    InitCartridge(rom_data, rom_size);
}


void HandleGbInput() {
    // REMEMBER
    // Unconventionally for the Game Boy, a button being pressed
    // is seen as the corresponding bit being 0, not 1.

    uint8_t oldState = GetButtons();

    // Key down
    if (IsKeyDown(KEY_W)) {
        SetButton(JOYPAD_BUTTON::Up);
    }
    if (IsKeyDown(KEY_A)) {
        SetButton(JOYPAD_BUTTON::Left);
    }
    if (IsKeyDown(KEY_S)) {
        SetButton(JOYPAD_BUTTON::Down);
    }
    if (IsKeyDown(KEY_D)) {
        SetButton(JOYPAD_BUTTON::Right);
    }
    if (IsKeyDown(KEY_K)) {
        SetButton(JOYPAD_BUTTON::B);
    }
    if (IsKeyDown(KEY_L)) {
        SetButton(JOYPAD_BUTTON::A);
    }
    if (IsKeyDown(KEY_O)) {
        SetButton(JOYPAD_BUTTON::Select);
    }
    if (IsKeyDown(KEY_P)) {
        SetButton(JOYPAD_BUTTON::Start);
    }

    // Key up
    if (IsKeyUp(KEY_W)) {
        ClearButton(JOYPAD_BUTTON::Up);
    }
    if (IsKeyUp(KEY_A)) {
        ClearButton(JOYPAD_BUTTON::Left);
    }
    if (IsKeyUp(KEY_S)) {
        ClearButton(JOYPAD_BUTTON::Down);
    }
    if (IsKeyUp(KEY_D)) {
        ClearButton(JOYPAD_BUTTON::Right);
    }
    if (IsKeyUp(KEY_K)) {
        ClearButton(JOYPAD_BUTTON::B);
    }
    if (IsKeyUp(KEY_L)) {
        ClearButton(JOYPAD_BUTTON::A);
    }
    if (IsKeyUp(KEY_O)) {
        ClearButton(JOYPAD_BUTTON::Select);
    }
    if (IsKeyUp(KEY_P)) {
        ClearButton(JOYPAD_BUTTON::Start);
    }
    
    // TODO
    // Maybe check for buttons that are selected (in Joypad Register)? 
    uint8_t newState = !!GetButtons();
    for (uint8_t i = 0; i < 8; ++i) {
        if ((oldState & 0b1) == 1 && (newState & 0b1) == 0) {
            auto IF = GB_Internal_Read(0xFF0F);
            GB_Internal_Write(0xFF0F, BIT_SET(IF, 4));
        }
        oldState >>= 1;
        newState >>= 1;
    }

}

void RunGameboy()
{
    uint32_t local_t_cycles = 0;

    while (g_gameboy->elapsedCycles < 70224) {

        if (g_gameboy->cpu->imeRequested && !g_gameboy->cpu->isStopped) {
            if (g_gameboy->eiDelayed) {
                g_gameboy->cpu->IME = 1;
                g_gameboy->eiDelayed = false;
                g_gameboy->cpu->imeRequested = false;
            }
            else
                g_gameboy->eiDelayed = true;
        }

        local_t_cycles = 0;
        local_t_cycles += HandleInterrupts(g_gameboy->cpu);

        if (!g_gameboy->cpu->isStopped)
            local_t_cycles += TickCpu(g_gameboy->cpu);
        else
            local_t_cycles += 4;

        TickPpu(local_t_cycles);
        TickTimer(local_t_cycles);

        g_gameboy->elapsedCycles += local_t_cycles;
    }

    g_gameboy->elapsedCycles %= 70224;
}

void ShutdownGameboy() {}

Gameboy* getInstance() { return g_gameboy; }