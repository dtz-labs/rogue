#ifndef ROGUE_ZX_MAP_FONT_H
#define ROGUE_ZX_MAP_FONT_H

/* Packed 4x8 font: one nibble per scanline, high nibble is the even one. */
#define ZX_MAP_FONT_FIRST 32
#define ZX_MAP_FONT_LAST 126
#define ZX_MAP_FONT_STRIDE 4

extern const unsigned char zx_map_font[];

#endif
