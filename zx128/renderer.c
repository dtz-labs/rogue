#include "curses.h"
#include "zx_banking.h"

#define ZX_SCREEN_COLS 80
#define ZX_VISIBLE_COLS 32

void zx_screen_copy(unsigned int index, unsigned char *target,
                    unsigned char count);

static void draw_physical_cell(unsigned char row, unsigned char col,
                               unsigned char ch)
{
    const unsigned char *glyph;
    unsigned char scanline;

    if (ch < 32U || ch > 127U)
        ch = '?';
    glyph = (const unsigned char *)(0x3d00U +
            ((unsigned int)(ch - 32U) << 3));
    for (scanline = 0; scanline < 8U; ++scanline) {
        unsigned int pixel_y = ((unsigned int)row << 3) + scanline;
        unsigned int address = 0x4000U
            + ((pixel_y & 0xc0U) << 5)
            + ((pixel_y & 0x07U) << 8)
            + ((pixel_y & 0x38U) << 2)
            + col;
        *(unsigned char *)address = glyph[scanline];
    }
    ((unsigned char *)0x5800U)[(unsigned int)row * ZX_VISIBLE_COLS + col] = 7U;
}

void zx_render_row(unsigned char row, unsigned char first_col)
{
    unsigned char physical_row[ZX_VISIBLE_COLS];
    unsigned char col;

    zx_screen_copy((unsigned int)row * ZX_SCREEN_COLS + first_col,
                   physical_row, ZX_VISIBLE_COLS);
    for (col = 0; col < ZX_VISIBLE_COLS; ++col)
        draw_physical_cell(row, col, physical_row[col]);
}

void zx_render_message_line(void)
{
    unsigned char physical_row[ZX_VISIBLE_COLS];
    unsigned char col;

    zx_screen_copy(ZX_VISIBLE_COLS, physical_row, ZX_VISIBLE_COLS);
    for (col = 0; col < ZX_VISIBLE_COLS; ++col)
        draw_physical_cell(1, col, physical_row[col]);
}
