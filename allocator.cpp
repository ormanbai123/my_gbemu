#include "allocator.h"

#define MAX_MEMORY 20 * 1024 * 1024

namespace {

	typedef struct Arena {
		size_t buffer_len;
		size_t offset;
		uint8_t* buffer;
	};

	Arena* g_arena = nullptr;

	uintptr_t AlignForward(uintptr_t ptr, size_t align) {
		uintptr_t p, a, modulo;

		//assert(is_power_of_two(align));

		p = ptr;
		a = (uintptr_t)align;
		// Same as (p % a) but faster as 'a' is a power of two
		modulo = p & (a - 1);

		if (modulo != 0) {
			// If 'p' address is not aligned, push the address to the
			// next value which is aligned
			p += a - modulo;
		}
		return p;
	}
}


void InitArena() {
	if (g_arena)
		return;

	g_arena = (Arena*)malloc(sizeof(Arena));
	if (!g_arena)
		exit(EXIT_FAILURE);

	g_arena->buffer = (uint8_t*)malloc(sizeof(uint8_t) * MAX_MEMORY);
	if (!g_arena->buffer)
		exit(EXIT_FAILURE);

	g_arena->buffer_len = MAX_MEMORY;
	g_arena->offset = 0;
}

void ShutdownArena() {
	free(g_arena->buffer);
	free(g_arena);
}

void* GB_Alloc(size_t size, size_t align) {

	uintptr_t curr_ptr = (uintptr_t)g_arena->buffer + (uintptr_t)g_arena->offset;
	uintptr_t offset = AlignForward(curr_ptr, align);
	offset -= (uintptr_t)g_arena->buffer; // Change to relative offset

	// Check to see if the backing memory has space left
	if (offset + size <= g_arena->buffer_len) {
		void* ptr = &g_arena->buffer[offset];
		g_arena->offset = offset + size;

		// Zero new memory by default
		memset(ptr, 0, size);
		return ptr;
	}

	exit(EXIT_FAILURE);
}

void GB_Free() { /* do nothing */ }

