#include <string.h>
#include <curses.h>
#include "rogue.h"
#include "zx_restart.h"

#define ZX_RESTART_BLOCK_SIZE 64U

extern unsigned char _data_clib_head[];
extern unsigned char _data_compiler_tail[];
extern unsigned char _DATA_0_head[], _DATA_0_tail[];
extern unsigned char _DATA_1_head[], _DATA_1_tail[];
extern unsigned char _DATA_3_head[], _DATA_3_tail[];
extern unsigned char _DATA_4_head[], _DATA_4_tail[];
extern unsigned char _DATA_6_head[], _DATA_6_tail[];
extern unsigned char _DATA_7_head[], _DATA_7_tail[];
extern unsigned char _BSS_0_head[], _BSS_0_tail[];
extern unsigned char _BSS_1_head[], _BSS_1_tail[];
extern unsigned char _BSS_3_head[], _BSS_3_tail[];
extern unsigned char _BSS_4_head[], _BSS_4_tail[];
extern unsigned char _BSS_6_head[], _BSS_6_tail[];

extern unsigned char zx_restart_banked_data_bank4[];

unsigned char zx_restart_fixed_data_bank7[ZX_RESTART_FIXED_DATA_SIZE];

static unsigned int
copy_to_snapshot(unsigned char bank, const unsigned char *source,
                 unsigned int size, unsigned int offset)
{
    unsigned char block[ZX_RESTART_BLOCK_SIZE];

    while (size != 0) {
        unsigned char count = size > sizeof block ? sizeof block : size;
        zx_bank_read(bank, source, block, count);
        zx_bank_write(4, &zx_restart_banked_data_bank4[offset], block, count);
        source += count;
        offset += count;
        size -= count;
    }
    return offset;
}

static unsigned int
restore_from_snapshot(unsigned char bank, unsigned char *target,
                      unsigned int size, unsigned int offset)
{
    unsigned char block[ZX_RESTART_BLOCK_SIZE];

    while (size != 0) {
        unsigned char count = size > sizeof block ? sizeof block : size;
        zx_bank_read(4, &zx_restart_banked_data_bank4[offset], block, count);
        zx_bank_write(bank, target, block, count);
        target += count;
        offset += count;
        size -= count;
    }
    return offset;
}

void
zx_restart_snapshot_init(void)
{
    unsigned int offset = 0;

    memcpy(zx_restart_fixed_data_bank7, _data_clib_head,
           ZX_RESTART_FIXED_DATA_SIZE);
    offset = copy_to_snapshot(0, _DATA_0_head, ZX_RESTART_BANK0_DATA_SIZE, offset);
    offset = copy_to_snapshot(1, _DATA_1_head, ZX_RESTART_BANK1_DATA_SIZE, offset);
    offset = copy_to_snapshot(3, _DATA_3_head, ZX_RESTART_BANK3_DATA_SIZE, offset);
    offset = copy_to_snapshot(4, _DATA_4_head, ZX_RESTART_BANK4_DATA_SIZE, offset);
    offset = copy_to_snapshot(6, _DATA_6_head, ZX_RESTART_BANK6_DATA_SIZE, offset);
    copy_to_snapshot(7, _DATA_7_head, ZX_RESTART_BANK7_DATA_SIZE, offset);
}

static void
zx_restart_restore_data(void)
{
    unsigned int offset = 0;

    memcpy(_data_clib_head, zx_restart_fixed_data_bank7,
           ZX_RESTART_FIXED_DATA_SIZE);
    offset = restore_from_snapshot(0, _DATA_0_head, ZX_RESTART_BANK0_DATA_SIZE, offset);
    offset = restore_from_snapshot(1, _DATA_1_head, ZX_RESTART_BANK1_DATA_SIZE, offset);
    offset = restore_from_snapshot(3, _DATA_3_head, ZX_RESTART_BANK3_DATA_SIZE, offset);
    offset = restore_from_snapshot(4, _DATA_4_head, ZX_RESTART_BANK4_DATA_SIZE, offset);
    offset = restore_from_snapshot(6, _DATA_6_head, ZX_RESTART_BANK6_DATA_SIZE, offset);
    restore_from_snapshot(7, _DATA_7_head, ZX_RESTART_BANK7_DATA_SIZE, offset);
}

void
zx_restart_game(void)
{
    zx_restart_restore_data();
    zx_bank_clear(0, _BSS_0_head,
                  (unsigned int)(_BSS_0_tail - _BSS_0_head));
    zx_bank_clear(1, _BSS_1_head,
                  (unsigned int)(_BSS_1_tail - _BSS_1_head));
    zx_bank_clear(3, _BSS_3_head,
                  (unsigned int)(_BSS_3_tail - _BSS_3_head));
    zx_bank_clear(4, _BSS_4_head,
                  (unsigned int)(_BSS_4_tail - _BSS_4_head));
    zx_bank_clear(6, _BSS_6_head,
                  (unsigned int)(_BSS_6_tail - _BSS_6_head));
    zx_restart_finish();
}
