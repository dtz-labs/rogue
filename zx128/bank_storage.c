#include <curses.h>
#include "rogue.h"

/*
 * These arrays are linked into the unused tails of pageable RAM banks.  The
 * fixed-memory access layer in banked_data.c is the only code allowed to
 * expose them to the original game.
 */
#if ZX_STORAGE_BANK == 0
PLACE zx_places_bank0[25 * MAXLINES];
#elif ZX_STORAGE_BANK == 1
PLACE zx_places_bank1[19 * MAXLINES];
#elif ZX_STORAGE_BANK == 3
PLACE zx_places_bank3[21 * MAXLINES];
unsigned char zx_screen_bank3[24 * 80];
#elif ZX_STORAGE_BANK == 4
PLACE zx_places_bank4[15 * MAXLINES];
#else
#error unsupported ZX_STORAGE_BANK
#endif
