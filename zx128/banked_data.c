#include <curses.h>
#include <string.h>
#include "rogue.h"
#include <arch/z80.h>
#include <intrinsic.h>

#define ZX_BANKM (*(volatile unsigned char *)23388)
#define ZX_PLACE_BANK0_COUNT (25U * MAXLINES)
#define ZX_PLACE_BANK1_COUNT (19U * MAXLINES)
#define ZX_PLACE_BANK3_COUNT (21U * MAXLINES)
#define ZX_SCREEN_COUNT (24U * 80U)

extern PLACE zx_places_bank0[ZX_PLACE_BANK0_COUNT];
extern PLACE zx_places_bank1[ZX_PLACE_BANK1_COUNT];
extern PLACE zx_places_bank3[ZX_PLACE_BANK3_COUNT];
extern PLACE zx_places_bank4[15U * MAXLINES];
extern unsigned char zx_screen_bank3[ZX_SCREEN_COUNT];

static void write_page_state(unsigned char state)
{
    ZX_BANKM = state;
    z80_outp(0x7ffdU, state);
}

static unsigned char page_bank(unsigned char bank)
{
    unsigned char old_state = ZX_BANKM;
    intrinsic_di();
    write_page_state((unsigned char)((old_state & 0xf8U) | bank));
    return old_state;
}

static void restore_page(unsigned char old_state)
{
    write_page_state(old_state);
    intrinsic_ei();
}

static PLACE *place_address(unsigned int index, unsigned char *bank)
{
    if (index < ZX_PLACE_BANK0_COUNT) {
        *bank = 0;
        return &zx_places_bank0[index];
    }
    index -= ZX_PLACE_BANK0_COUNT;
    if (index < ZX_PLACE_BANK1_COUNT) {
        *bank = 1;
        return &zx_places_bank1[index];
    }
    index -= ZX_PLACE_BANK1_COUNT;
    if (index < ZX_PLACE_BANK3_COUNT) {
        *bank = 3;
        return &zx_places_bank3[index];
    }
    *bank = 4;
    return &zx_places_bank4[index - ZX_PLACE_BANK3_COUNT];
}

static unsigned int place_index(int y, int x)
{
    return ((unsigned int)x << 5) + (unsigned int)y;
}

void zx_place_get(int y, int x, PLACE *place)
{
    unsigned char bank;
    PLACE *source = place_address(place_index(y, x), &bank);
    unsigned char old_state = page_bank(bank);
    *place = *source;
    restore_page(old_state);
}

void zx_place_put(int y, int x, const PLACE *place)
{
    unsigned char bank;
    PLACE *target = place_address(place_index(y, x), &bank);
    unsigned char old_state = page_bank(bank);
    *target = *place;
    restore_page(old_state);
}

char zx_place_get_ch(int y, int x)
{
    PLACE place;
    zx_place_get(y, x, &place);
    return place.p_ch;
}

char zx_place_get_flags(int y, int x)
{
    PLACE place;
    zx_place_get(y, x, &place);
    return place.p_flags;
}

THING *zx_place_get_monst(int y, int x)
{
    PLACE place;
    zx_place_get(y, x, &place);
    return place.p_monst;
}

void zx_place_set_ch(int y, int x, char ch)
{
    PLACE place;
    zx_place_get(y, x, &place);
    place.p_ch = ch;
    zx_place_put(y, x, &place);
}

void zx_place_set_flags(int y, int x, char flags)
{
    PLACE place;
    zx_place_get(y, x, &place);
    place.p_flags = flags;
    zx_place_put(y, x, &place);
}

void zx_place_or_flags(int y, int x, char flags)
{
    PLACE place;
    zx_place_get(y, x, &place);
    place.p_flags |= flags;
    zx_place_put(y, x, &place);
}

void zx_place_and_flags(int y, int x, char flags)
{
    PLACE place;
    zx_place_get(y, x, &place);
    place.p_flags &= flags;
    zx_place_put(y, x, &place);
}

void zx_place_set_monst(int y, int x, THING *monst)
{
    PLACE place;
    zx_place_get(y, x, &place);
    place.p_monst = monst;
    zx_place_put(y, x, &place);
}

char zx_place_winat(int y, int x)
{
    PLACE place;
    zx_place_get(y, x, &place);
    return place.p_monst != NULL ? place.p_monst->t_disguise : place.p_ch;
}

static void clear_place_chunk(PLACE *places, unsigned int count,
                              unsigned char bank)
{
    unsigned char old_state = page_bank(bank);
    while (count--) {
        places->p_ch = ' ';
        places->p_flags = F_REAL;
        places->p_monst = NULL;
        ++places;
    }
    restore_page(old_state);
}

void zx_places_clear(void)
{
    clear_place_chunk(zx_places_bank0, ZX_PLACE_BANK0_COUNT, 0);
    clear_place_chunk(zx_places_bank1, ZX_PLACE_BANK1_COUNT, 1);
    clear_place_chunk(zx_places_bank3, ZX_PLACE_BANK3_COUNT, 3);
    clear_place_chunk(zx_places_bank4, 15U * MAXLINES, 4);
}

void zx_screen_set(unsigned int index, unsigned char value)
{
    unsigned char old_state = page_bank(3);
    zx_screen_bank3[index] = value;
    restore_page(old_state);
}

unsigned char zx_screen_get(unsigned int index)
{
    unsigned char value;
    unsigned char old_state = page_bank(3);
    value = zx_screen_bank3[index];
    restore_page(old_state);
    return value;
}

void zx_screen_copy(unsigned int index, unsigned char *target,
                    unsigned char count)
{
    unsigned char old_state = page_bank(3);
    memcpy(target, &zx_screen_bank3[index], count);
    restore_page(old_state);
}

void zx_screen_fill(unsigned int index, unsigned int count,
                    unsigned char value)
{
    unsigned char old_state = page_bank(3);
    memset(&zx_screen_bank3[index], value, count);
    restore_page(old_state);
}

char *zx_random_color_name(void)
{
    static char color[12];
    unsigned int index = (unsigned int)rnd(27);
    unsigned char old_state = page_bank(3);
    const char *source = rainbow[index];
    char *target = color;

    while ((*target++ = *source++) != '\0')
        ;
    restore_page(old_state);
    return color;
}
