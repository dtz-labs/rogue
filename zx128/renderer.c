#include "curses.h"
#include "zx_banking.h"
#include "zx_status_font.h"
#include <string.h>

#define ZX_SCREEN_COLS 80
#define ZX_VISIBLE_COLS 32
#define ZX_STATUS_COLS (2 * ZX_VISIBLE_COLS)

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

/*
 * The status font stops at 'Z', so ']', '^' and '|' do not exist in it at all.
 * Those three, plus four glyphs the map wants drawn differently from the way
 * text wants them, live here -- four bytes each, in the status packing.
 *
 * The redraws answer things that only show up once a whole dungeon is on
 * screen:
 *
 *   '.'  was a 2x2 block, which made bare floor heavier than the items and
 *        monsters standing on it. One pixel is enough to read as floor.
 *   '#'  is drawn on columns 0 and 2 of every even scanline. Both axes then
 *        repeat with period 2, so the dots stay evenly spaced across cell
 *        edges; on odd scanlines the vertical gap between stacked corridor
 *        cells came out at 3 where the gap inside a cell was 1.
 *   '+'  had its crossbar on columns 1..3, so every door bled into the glyph
 *        beside it. Centred on 0..2 it stays inside its own slot.
 *   '%'  rebuilt so the staircase does not disappear into the floor.
 *
 * '@' is deliberately NOT redrawn: the stock glyph is the densest thing in the
 * font, and against single-pixel floor it already reads as the player.
 */
#define ZX_MAP_GLYPHS 7

static const unsigned char zx_map_glyph_char[ZX_MAP_GLYPHS] = {
    '.', '#', '|', '+', '%', ']', '^'
};

static const unsigned char zx_map_glyph_data[ZX_MAP_GLYPHS][4] = {
    { 0x00, 0x00, 0x04, 0x00 },     /* . */
    { 0xa0, 0xa0, 0xa0, 0xa0 },     /* # */
    { 0x44, 0x44, 0x44, 0x44 },     /* | */
    { 0x00, 0x44, 0xe4, 0x40 },     /* + */
    { 0x88, 0x24, 0x48, 0x22 },     /* % */
    { 0xc4, 0x44, 0x44, 0xc0 },     /* ] */
    { 0x4a, 0x00, 0x00, 0x00 }      /* ^ */
};

static const unsigned char *status_glyph(unsigned char ch)
{
    if (ch >= 'a' && ch <= 'z')
        ch = (unsigned char)(ch - ('a' - 'A'));
    if (ch < ZX_STATUS_FONT_FIRST || ch > ZX_STATUS_FONT_LAST)
        ch = ' ';
    return zx_status_font
        + ((unsigned int)(ch - ZX_STATUS_FONT_FIRST) * ZX_STATUS_FONT_STRIDE);
}

/* One nibble per scanline, two scanlines to a byte, even scanline on top. */
static unsigned char status_scanline(const unsigned char *glyph,
                                     unsigned char scanline)
{
    unsigned char packed = glyph[scanline >> 1];

    return (scanline & 1U) ? (unsigned char)(packed & 0x0fU)
                           : (unsigned char)(packed >> 4);
}

/*
 * The status line is the one row where 32 characters are not enough: the
 * 80-column original ended mid-Hp and hid Str, Arm and Exp completely. Draw it
 * with the 4x8 font instead, fitting 64 characters in the same 32 cells, two
 * glyphs to a cell.
 *
 * The options and help screens write short prompts to this same row, and they
 * read better in the ROM font, so switch on the text rather than on who wrote
 * it: anything that still fits in 32 columns is drawn the normal way.
 */
void zx_render_status_row(unsigned char row)
{
    unsigned char text[ZX_STATUS_COLS];
    unsigned char col;

    zx_screen_copy((unsigned int)row * ZX_SCREEN_COLS, text, ZX_STATUS_COLS);
    for (col = ZX_VISIBLE_COLS; col < ZX_STATUS_COLS; ++col)
        if (text[col] != ' ')
            break;
    if (col == ZX_STATUS_COLS) {
        for (col = 0; col < ZX_VISIBLE_COLS; ++col)
            draw_physical_cell(row, col, text[col]);
        return;
    }

    for (col = 0; col < ZX_VISIBLE_COLS; ++col) {
        const unsigned char *left = status_glyph(text[col << 1]);
        const unsigned char *right = status_glyph(text[(col << 1) + 1]);
        unsigned char scanline;

        for (scanline = 0; scanline < 8U; ++scanline) {
            unsigned int pixel_y = ((unsigned int)row << 3) + scanline;
            unsigned int address = 0x4000U
                + ((pixel_y & 0xc0U) << 5)
                + ((pixel_y & 0x07U) << 8)
                + ((pixel_y & 0x38U) << 2)
                + col;
            *(unsigned char *)address =
                (unsigned char)((status_scanline(left, scanline) << 4)
                                | status_scanline(right, scanline));
        }
        ((unsigned char *)0x5800U)[(unsigned int)row * ZX_VISIBLE_COLS + col] = 7U;
    }
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

static const unsigned char *map_glyph(unsigned char ch)
{
    unsigned char i;

    for (i = 0; i < ZX_MAP_GLYPHS; ++i)
        if (zx_map_glyph_char[i] == ch)
            return zx_map_glyph_data[i];
    return status_glyph(ch);
}

/*
 * A map row in the 4x8 font: 64 dungeon columns in the same 32 cells, two
 * glyphs to a cell, exactly as the status row already does it. The message row
 * deliberately does not come through here -- it keeps the ROM font, because
 * the small font has no lower case and prose in capitals reads badly.
 */
void zx_render_map_row(unsigned char row, unsigned char first_col)
{
    unsigned char logical_row[ZX_MAP_COLS];
    unsigned char col;

    zx_screen_copy((unsigned int)row * ZX_SCREEN_COLS + first_col,
                   logical_row, ZX_MAP_COLS);

    for (col = 0; col < ZX_VISIBLE_COLS; ++col) {
        const unsigned char *left = map_glyph(logical_row[col << 1]);
        const unsigned char *right = map_glyph(logical_row[(col << 1) + 1]);
        unsigned char scanline;

        for (scanline = 0; scanline < 8U; ++scanline) {
            unsigned int pixel_y = ((unsigned int)row << 3) + scanline;
            unsigned int address = 0x4000U
                + ((pixel_y & 0xc0U) << 5)
                + ((pixel_y & 0x07U) << 8)
                + ((pixel_y & 0x38U) << 2)
                + col;
            *(unsigned char *)address =
                (unsigned char)((status_scanline(left, scanline) << 4)
                                | status_scanline(right, scanline));
        }
        ((unsigned char *)0x5800U)[(unsigned int)row * ZX_VISIBLE_COLS + col] = 7U;
    }
}

void zx_render_message_line(void)
{
    unsigned char physical_row[ZX_VISIBLE_COLS];
    unsigned char col;

    zx_screen_copy(ZX_VISIBLE_COLS, physical_row, ZX_VISIBLE_COLS);
    for (col = 0; col < ZX_VISIBLE_COLS; ++col)
        draw_physical_cell(1, col, physical_row[col]);
}

void zx_inventory_overlay_clear(void)
{
    memset((void *)0x4000U, 0, 6144U);
    memset((void *)0x5800U, 7, 768U);
}

void zx_inventory_overlay_line(unsigned char row, const char *text)
{
    unsigned char ch;
    unsigned char col;

    for (col = 0; col < ZX_VISIBLE_COLS; ++col) {
        ch = ' ';
        if (*text != '\0')
            ch = (unsigned char)*text++;
        draw_physical_cell(row, col, ch);
    }
}
