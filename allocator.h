#ifndef GB_ALLOCATER_H
#define GB_ALLOCATER_H

#include <cstdint>
#include <cstdlib>
#include <cstring>

void InitArena();
void ShutdownArena();
void* GB_Alloc(size_t size, size_t align = 2 * sizeof(void*));
void GB_Free();

#endif // !ALLOCATER_H
