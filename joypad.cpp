#include "joypad.h"

#include "utils.h"

namespace {
	Joypad* g_joypad = nullptr;
}

void InitJoypad(Joypad* joypad) {
	g_joypad = joypad;

	g_joypad->select_dpad = 1; // 1 means unset, 0 means set.
	g_joypad->select_buttons = 1;

	g_joypad->state = 0xFF;
}

uint8_t GetActionButtons() {
	return (g_joypad->state >> 4);
}

uint8_t GetDirectionButtons() {
	return g_joypad->state & 0x0F;
}

void SetButton(JOYPAD_BUTTON btn) {
	BIT_CLEAR(g_joypad->state, btn);
}

void ClearButton(JOYPAD_BUTTON btn) {
	BIT_SET(g_joypad->state, btn);
}

uint8_t GetButtons() {
	return g_joypad->state;
}
