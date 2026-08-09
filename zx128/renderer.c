#include "curses.h"
#include "zx_banking.h"
#include "zx_map_font.h"
#include <string.h>

#define ZX_SCREEN_COLS 80
#define ZX_SCREEN_ROWS 24
#define ZX_VISIBLE_COLS 32              /* physical character cells across */

void zx_screen_copy(unsigned int index, unsigned char *target,
                    unsigned char count);

/*
 * Every row on screen is drawn in the 4x8 font: two glyphs to a character
 * cell, 64 columns where the ROM font gave 32. That covers the message row,
 * the status row and the inventory overlay as well as the map, so there is one
 * code path here rather than one per kind of row.
 *
 * The map wants a few glyphs drawn differently from the way text wants them,
 * and three the font does not carry at all. Those live below, four bytes each,
 * in the same packing as the font.
 *
 *   '.'  the font's floor is a 2x2 block, which makes bare floor heavier than
 *        the items and monsters standing on it. One pixel is enough.
 *   '#'  one dot per cell on alternating columns. Corridors cover a lot of
 *        ground and have to stay a texture rather than compete with the walls.
 *   '-'  the font draws it on columns 1..3, so a horizontal wall lost a pixel
 *        every fourth one and read as dashed. It has to be solid before a
 *        corner can meet it.
 *   '+'  a small filled square sitting on the wall.
 *   '%'  rebuilt so a staircase does not vanish into the floor.
 *   '|', ']', '^'  absent from the font, which stops at 'Z' for these.
 */
#define ZX_MAP_GLYPHS 8
#define ZX_MAP_DASH 3                   /* index of '-' below */

static const unsigned char zx_map_glyph_char[ZX_MAP_GLYPHS] = {
    '.', '#', '|', '-', '+', '%', ']', '^'
};

static const unsigned char zx_map_glyph_data[ZX_MAP_GLYPHS][4] = {
    { 0x00, 0x00, 0x04, 0x00 },     /* . */
    { 0x80, 0x00, 0x20, 0x00 },     /* # */
    { 0x44, 0x44, 0x44, 0x44 },     /* | */
    { 0x00, 0x00, 0xf0, 0x00 },     /* - */
    { 0x00, 0x0e, 0xee, 0x00 },     /* + */
    { 0x88, 0x24, 0x48, 0x22 },     /* % */
    { 0xc4, 0x44, 0x44, 0xc0 },     /* ] */
    { 0x4a, 0x00, 0x00, 0x00 }      /* ^ */
};

/*
 * Rogue stores a plain '-' in all four corners of a room -- horiz() writes the
 * whole top and bottom row and vert() only the sides between them -- so a
 * corner is not something the map records. It is recognised here instead.
 *
 * A '-' is a corner when the wall run stops on exactly one side of it. '+'
 * counts as the run continuing, or every door would read as two corners. One
 * probe of the cell below, then above, says which of the four it is. When
 * neither is known yet, which happens in a dark room revealed a cell at a
 * time, it stays an ordinary dash rather than guessing.
 */
#define ZX_CORNER_TOP_LEFT     0
#define ZX_CORNER_TOP_RIGHT    1
#define ZX_CORNER_BOTTOM_LEFT  2
#define ZX_CORNER_BOTTOM_RIGHT 3

static const unsigned char zx_corner_glyph[4][4] = {
    { 0x00, 0x00, 0x74, 0x44 },     /* top left     */
    { 0x00, 0x00, 0xc4, 0x44 },     /* top right    */
    { 0x44, 0x44, 0x70, 0x00 },     /* bottom left  */
    { 0x44, 0x44, 0xc0, 0x00 }      /* bottom right */
};

static const unsigned char *font_glyph(unsigned char ch)
{
    if (ch < ZX_MAP_FONT_FIRST || ch > ZX_MAP_FONT_LAST)
        ch = ' ';
    return zx_map_font
        + ((unsigned int)(ch - ZX_MAP_FONT_FIRST) * ZX_MAP_FONT_STRIDE);
}

static unsigned char is_wall_run(unsigned char ch)
{
    return (unsigned char)(ch == '-' || ch == '+');
}

static unsigned char cell_at(unsigned char row, unsigned char col)
{
    unsigned char ch;

    zx_screen_copy((unsigned int)row * ZX_SCREEN_COLS + col, &ch, 1);
    return ch;
}

static const unsigned char *map_glyph(unsigned char ch)
{
    unsigned char i;

    for (i = 0; i < ZX_MAP_GLYPHS; ++i)
        if (zx_map_glyph_char[i] == ch)
            return zx_map_glyph_data[i];
    return font_glyph(ch);
}

/*
 * Only consulted for a '-' whose wall run ends on one side, so at most a
 * couple of cells in a row rather than 64.
 */
static const unsigned char *corner_glyph(unsigned char row, unsigned char col,
                                         unsigned char left_is_wall,
                                         unsigned char right_is_wall)
{
    /* The wall carries on to the right, so this is the room's left edge. */
    unsigned char is_left = (unsigned char)(right_is_wall && !left_is_wall);

    if (left_is_wall == right_is_wall)
        return zx_map_glyph_data[ZX_MAP_DASH];
    if (row + 1U < ZX_SCREEN_ROWS && cell_at((unsigned char)(row + 1U), col) == '|')
        return zx_corner_glyph[is_left ? ZX_CORNER_TOP_LEFT : ZX_CORNER_TOP_RIGHT];
    if (row > 0U && cell_at((unsigned char)(row - 1U), col) == '|')
        return zx_corner_glyph[is_left ? ZX_CORNER_BOTTOM_LEFT : ZX_CORNER_BOTTOM_RIGHT];
    return zx_map_glyph_data[ZX_MAP_DASH];
}

static const unsigned char *cell_glyph(const unsigned char *logical_row,
                                       unsigned char index,
                                       unsigned char row,
                                       unsigned char first_col,
                                       unsigned char dungeon)
{
    unsigned char ch = logical_row[index];

    if (!dungeon)
        return font_glyph(ch);
    if (ch != '-' || index == 0U || index + 1U >= ZX_MAP_COLS)
        return map_glyph(ch);
    return corner_glyph(row, (unsigned char)(first_col + index),
                        is_wall_run(logical_row[index - 1U]),
                        is_wall_run(logical_row[index + 1U]));
}

/*
 * The eight scanline addresses of a character row are the same for every cell
 * in it, so they are worked out once here rather than eight times per cell.
 * On a 32-cell row that is 256 address computations replaced by 8.
 */
static void row_scanline_bases(unsigned char row, unsigned char **bases)
{
    unsigned char scanline;

    for (scanline = 0; scanline < 8U; ++scanline) {
        unsigned int pixel_y = ((unsigned int)row << 3) + scanline;
        bases[scanline] = (unsigned char *)(0x4000U
            + ((pixel_y & 0xc0U) << 5)
            + ((pixel_y & 0x07U) << 8)
            + ((pixel_y & 0x38U) << 2));
    }
}

/*
 * Draw cells first_cell..last_cell of a row, in physical cells. Walking one
 * step dirties a couple of cells, not a whole row, and redrawing only those is
 * what keeps a move cheap -- particularly a vertical one, which dirties two
 * rows where a horizontal move dirties one.
 *
 * A span that is entirely blank is cleared with memset instead of drawn. That
 * is the common case whenever a window is erased or an overlay comes down: the
 * old path pushed a space glyph through the full per-cell pipeline for every
 * one of 768 cells to write nothing but zeros.
 *
 * The whole logical row is read even for a narrow span: that is one banked
 * memcpy against per-cell work costing far more, and corner inference wants
 * the neighbours either side of the span anyway.
 */
void zx_render_row_span(unsigned char row, unsigned char first_col,
                        unsigned char first_cell, unsigned char last_cell,
                        unsigned char dungeon)
{
    unsigned char logical_row[ZX_MAP_COLS];
    const unsigned char *glyphs[ZX_MAP_COLS];
    unsigned char *bases[8];
    unsigned char col;
    unsigned char scanline;
    unsigned char blank = TRUE;
    unsigned char count;

    zx_screen_copy((unsigned int)row * ZX_SCREEN_COLS + first_col,
                   logical_row, ZX_MAP_COLS);

    if (last_cell >= ZX_VISIBLE_COLS)
        last_cell = ZX_VISIBLE_COLS - 1U;
    count = (unsigned char)(last_cell - first_cell + 1U);
    row_scanline_bases(row, bases);

    for (col = (unsigned char)(first_cell << 1);
         col <= (unsigned char)((last_cell << 1) + 1U); ++col)
        if (logical_row[col] != ' ') {
            blank = FALSE;
            break;
        }

    if (blank) {
        for (scanline = 0; scanline < 8U; ++scanline)
            memset(bases[scanline] + first_cell, 0, count);
        memset(&((unsigned char *)0x5800U)[(unsigned int)row * ZX_VISIBLE_COLS
                                           + first_cell], 7, count);
        return;
    }

    /*
     * Resolve every glyph in the span first, then walk the screen one scanline
     * at a time. Interleaving the two meant indexing the base-address array
     * for every byte written; this way each scanline is a straight run of
     * increments, which is what a Z80 is good at.
     */
    for (col = (unsigned char)(first_cell << 1);
         col <= (unsigned char)((last_cell << 1) + 1U); ++col)
        glyphs[col] = cell_glyph(logical_row, col, row, first_col, dungeon);

    for (scanline = 0; scanline < 4U; ++scanline) {
        unsigned char *even = bases[scanline << 1] + first_cell;
        unsigned char *odd = bases[(scanline << 1) + 1U] + first_cell;
        const unsigned char **pair = &glyphs[first_cell << 1];
        unsigned char n = count;

        while (n-- != 0) {
            unsigned char l = (*pair++)[scanline];
            unsigned char r = (*pair++)[scanline];

            *even++ = (unsigned char)((l & 0xf0U) | (r >> 4));
            *odd++ = (unsigned char)((l << 4) | (r & 0x0fU));
        }
    }
    memset(&((unsigned char *)0x5800U)[(unsigned int)row * ZX_VISIBLE_COLS
                                       + first_cell], 7, count);
}

void zx_inventory_overlay_clear(void)
{
    memset((void *)0x4000U, 0, 6144U);
    memset((void *)0x5800U, 7, 768U);
}

void zx_inventory_overlay_line(unsigned char row, const char *text)
{
    unsigned char *bases[8];
    unsigned char col;
    unsigned char scanline;

    row_scanline_bases(row, bases);
    for (col = 0; col < ZX_VISIBLE_COLS; ++col) {
        const unsigned char *left = font_glyph((unsigned char)
            (*text != '\0' ? *text++ : ' '));
        const unsigned char *right = font_glyph((unsigned char)
            (*text != '\0' ? *text++ : ' '));

        /*
         * One packed byte carries two scanlines, so four iterations cover the
         * cell. Taking them one scanline at a time meant sixteen shift-and-mask
         * steps per cell to undo work the packing had already done.
         */
        for (scanline = 0; scanline < 4U; ++scanline) {
            unsigned char l = left[scanline];
            unsigned char r = right[scanline];

            bases[scanline << 1][col] = (unsigned char)((l & 0xf0U) | (r >> 4));
            bases[(scanline << 1) + 1U][col] =
                (unsigned char)((l << 4) | (r & 0x0fU));
        }
        ((unsigned char *)0x5800U)[(unsigned int)row * ZX_VISIBLE_COLS + col] = 7U;
    }
}
