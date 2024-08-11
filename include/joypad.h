#ifndef GB_JOYPAD_H
#define GB_JOYPAD_H

#include <stdint.h>

enum JOYPAD_BUTTON {
	Right, Left, Up,  Down,
	A, B, Select, Start
};

typedef struct Joypad {
	// lower nibble are D-pad (up,down,left,right)
	// high nibble buttons (b,a,select,start)
	uint8_t state;
	bool select_dpad;
	bool select_buttons;
};

void InitJoypad(Joypad* joypad);

uint8_t GetActionButtons();

uint8_t GetDirectionButtons();

void SetButton(JOYPAD_BUTTON btn);

void ClearButton(JOYPAD_BUTTON btn);

uint8_t GetButtons();

#endif
