#ifndef ROGUE_ZX_PLACES_H
#define ROGUE_ZX_PLACES_H

#define ZX_PLACE_ROWS NUMLINES

#define ZX_PLACE_BANK0_COUNT (25U * ZX_PLACE_ROWS)
#define ZX_PLACE_BANK1_COUNT (19U * ZX_PLACE_ROWS)
#define ZX_PLACE_BANK3_COUNT (21U * ZX_PLACE_ROWS)
#define ZX_PLACE_BANK4_COUNT (15U * ZX_PLACE_ROWS)

#if (25 + 19 + 21 + 15) != NUMCOLS
#error ZX place-bank column counts must cover the complete map
#endif

#endif
