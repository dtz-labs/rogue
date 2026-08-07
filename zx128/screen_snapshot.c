#include <string.h>
#include <curses.h>
#include "rogue.h"

#define ZX_SCREEN_ROW_BYTES NUMCOLS
#define ZX_SCREEN_BYTES (NUMLINES * NUMCOLS)

unsigned char zx_screen_snapshot_bank7[ZX_SCREEN_BYTES];

void
zx_screen_snapshot_save(void)
{
    unsigned char row[ZX_SCREEN_ROW_BYTES];
    unsigned int index;

    for (index = 0; index < ZX_SCREEN_BYTES; index += ZX_SCREEN_ROW_BYTES)
    {
	zx_screen_copy(index, row, sizeof row);
	memcpy(&zx_screen_snapshot_bank7[index], row, sizeof row);
    }
}

void
zx_screen_snapshot_restore(void)
{
    unsigned char row[ZX_SCREEN_ROW_BYTES];
    unsigned int index;

    for (index = 0; index < ZX_SCREEN_BYTES; index += ZX_SCREEN_ROW_BYTES)
    {
	memcpy(row, &zx_screen_snapshot_bank7[index], sizeof row);
	zx_screen_write(index, row, sizeof row);
    }
}
