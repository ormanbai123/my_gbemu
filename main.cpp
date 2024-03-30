#include "raylib.h"
#include <cstdlib>

#include "allocator.h"

#include "emulator.h"

// gameboy width and height
const int width = 160; 
const int height = 144;

const int windowWidth = width * 4; 
const int windowHeight = height * 4;

bool memoryMeasured = false;

// Rendering stuff
Color* screen;
Texture2D texture;

// Logging
#ifdef LOGGING_ENABLED
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
class CircularBuf {
public:
	size_t maxSize = 10;
	size_t size = 0;

	CircularBuf() {
		past_commands = std::vector<std::string>(maxSize, "");
	}
	void push(std::string str) {
		if (size == maxSize)
			size = 0;
		past_commands[size++] = str;
	}
	std::string get(size_t indx) {
		return past_commands[indx];
	}
private:
	std::vector<std::string> past_commands;
};
#endif
//

void RenderFrame() {
	UpdateTexture(texture, screen);
}

int main()
{
	InitArena();

	screen = (Color*)GB_Alloc((size_t)width * height * sizeof(Color));

	InitGameboy(screen, RenderFrame);
	InsertCartridge("zelda_awakening.gb");
	
	SetTraceLogLevel(LOG_ERROR);
    InitWindow(windowWidth, windowHeight, "My Emulator");
    SetTargetFPS(60);
		
	Image im = {};
	im.data = screen;
	im.width = width;
	im.height = height;
	im.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
	im.mipmaps = 1;
	
	texture = LoadTextureFromImage(im);

	// Logging
#ifdef LOGGING_ENABLED
	CircularBuf command_buffer;
	Gameboy* log_state = GetGameboyState();
#endif
	//

	Gameboy* myGameboy = getInstance();

    // Main loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update

		// Logging
#ifdef LOGGING_ENABLED
		if (IsKeyPressed(KEY_F7)) {
			command_buffer.push(LogRunGameboy());
		}
#else
		RunGameboy();
#endif
		//--------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

        ClearBackground(RAYWHITE);
		
		// Logging
#ifdef LOGGING_ENABLED
		for (int i = 0; i < command_buffer.maxSize; ++i) {
			if( i == command_buffer.size - 1)
				DrawText(command_buffer.get(i).c_str(), 10, (i + 1) * 30, 20, RED);
			else
				DrawText(command_buffer.get(i).c_str(), 10, (i + 1) * 30, 20, BLUE);
		}

		std::stringstream ss;
		ss << "BC: 0x" << std::uppercase << std::hex << log_state->cpu->BC.reg16 << "\n\n";
		ss << "DE: 0x" << std::uppercase << std::hex << log_state->cpu->DE.reg16 << "\n\n";
		ss << "HL: 0x" << std::uppercase << std::hex << log_state->cpu->HL.reg16 << "\n\n";
		ss << "AF: 0x" << std::uppercase << std::hex << log_state->cpu->AF.reg16 << "\n\n";
		ss << "SP: 0x" << std::uppercase << std::hex << log_state->cpu->SP.reg16 << "\n\n";
		ss << "PC: 0x" << std::uppercase << std::hex << log_state->cpu->PC.reg16 << "\n\n";

		DrawText(ss.str().c_str(), 300, (0 + 1) * 30, 20, BLACK);
#else
		DrawTextureEx(texture, Vector2{0,0}, 0, 4.0f, WHITE);
#endif
		//----------------------

		if (IsKeyDown(KEY_B)) {
			DrawText(TextFormat("Mode: %d", myGameboy->ppu->mode), 0, 0, 30, RED);
			DrawText(TextFormat("LY: %d", myGameboy->ppu->ly), 0, 30, 30, RED);
		}

        EndDrawing();
        //----------------------------------------------------------------------------------

		HandleGbInput();

		// Getting memory usage info
		/*if (!memoryMeasured) {
			memoryMeasured = true;
			printf("Memory usage: %f/10MB", (float)GetMemUsage() / (1024 * 1024));
		}*/
    }


    // De-Initialization
	//UnloadImage(im);				// frees memory for us (which is unfortunate)
	
	UnloadTexture(texture);
	
    CloseWindow();        // Close window and OpenGL context

	ShutdownGameboy();		// Shutdown Gameboy

	ShutdownArena();	// De-allocate ALL in-game heap memory
    return 0;
}