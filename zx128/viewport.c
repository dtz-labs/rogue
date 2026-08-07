#include <curses.h>
#include "rogue.h"

#define ZX_VIEWPORT_MAX_LEFT (NUMCOLS - ZX_VIEWPORT_COLS)
#define ZX_VIEWPORT_EDGE 4
#define ZX_VIEWPORT_STEP 8
#define ZX_ROOM_PADDING 3

static int clamp_left(int value)
{
    if (value < 0)
        return 0;
    if (value > ZX_VIEWPORT_MAX_LEFT)
        return ZX_VIEWPORT_MAX_LEFT;
    return value;
}

void zx_update_viewport(void)
{
    int left = zx_viewport_first_col;
    int min_left = 0;
    int max_left = ZX_VIEWPORT_MAX_LEFT;

    if (left == ZX_VIEWPORT_NONE)
        left = clamp_left(hero.x - ZX_VIEWPORT_COLS / 2);

    if (proom != NULL && !(proom->r_flags & ISGONE) && proom->r_max.x > 0)
    {
        int room_left = proom->r_pos.x;
        int room_right = room_left + proom->r_max.x - 1;

        if (proom->r_max.x <= ZX_VIEWPORT_COLS)
        {
            int padded_left = clamp_left(room_left - ZX_ROOM_PADDING);
            int padded_right = room_right + ZX_ROOM_PADDING;
            int low;
            int high;

            if (padded_right >= NUMCOLS)
                padded_right = NUMCOLS - 1;
            low = clamp_left(padded_right - ZX_VIEWPORT_COLS + 1);
            high = clamp_left(padded_left);
            if (low > high)
            {
                low = clamp_left(room_right - ZX_VIEWPORT_COLS + 1);
                high = clamp_left(room_left);
            }
            if (left < low)
                left = low;
            else if (left > high)
                left = high;
            zx_viewport_set(left);
            return;
        }
        min_left = clamp_left(room_left);
        max_left = clamp_left(room_right - ZX_VIEWPORT_COLS + 1);
    }

    if (left < min_left)
        left = min_left;
    else if (left > max_left)
        left = max_left;
    while (hero.x - left < ZX_VIEWPORT_EDGE && left > min_left)
    {
        left -= ZX_VIEWPORT_STEP;
        if (left < min_left)
            left = min_left;
    }
    while (hero.x - left > ZX_VIEWPORT_COLS - ZX_VIEWPORT_EDGE - 1 &&
           left < max_left)
    {
        left += ZX_VIEWPORT_STEP;
        if (left > max_left)
            left = max_left;
    }
    zx_viewport_set(left);
}
