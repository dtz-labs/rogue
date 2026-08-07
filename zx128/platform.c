#include <arch/zx.h>
#include <curses.h>
#include <string.h>
#include "rogue.h"

volatile unsigned char zx_boot_stage;
volatile unsigned char zx_turn_count;

static bool zx_seed_collecting;

extern unsigned char _BSS_7_head[], _BSS_7_tail[];

/* Passed from bank 0 movement code to bank 6/7 helpers. */
coord nh;
coord rndmove_ret;

static void stop_forever(void)
{
    for (;;)
        ;
}

void zx_wait_for_key_release(void)
{
__asm
zx_wait_for_key_release_loop:
    xor a
    in a, (0xfe)
    and 0x1f
    cp 0x1f
    jr nz, zx_wait_for_key_release_loop
__endasm;
}

int main(void)
{
    bool named_hero;

    zx_restart_snapshot_init();
    strcpy(fruit, "slime-mold");
    dnum = 1;
    seed = 0x13579bdfL;

    initscr();
    /* The loader leaves whatever border the ROM last set; the dungeon reads
       as one surface only when the border matches the black screen. Cold
       restart re-enters at the CRT, so main() repaints it every time. */
    zx_border(INK_BLACK);
    zx_seed_collecting = TRUE;
    zx_startup_help();
    named_hero = zx_startup_name();
    zx_seed_collecting = FALSE;
    if (!named_hero)
        seed = 0x13579bdfL;
    else {
        dnum = (int)(seed & 0x7fffU);
        if (dnum == 0)
            dnum = 1;
    }
    init_probs();
    init_player();
    init_names();
    init_colors();
    init_stones();
    init_materials();
    setup();

    hw = newwin(LINES, COLS, 0, 0);
    new_level();
    start_daemon(zx_cb_runners, 0, AFTER);
    start_daemon(zx_cb_doctor, 0, AFTER);
    fuse(zx_cb_swander, 0, WANDERTIME, AFTER);
    start_daemon(zx_cb_stomach, 0, AFTER);
    playit();
    return 0;
}

static void zx_restart_enter(void) __naked
{
__asm
    di
    ld a, 0x10
    ld (0x5b5c), a
    ld bc, 0x7ffd
    out (c), a
    ei
    jp 0x6003
__endasm;
}

void zx_restart_finish(void)
{
    zx_bank_clear(7, _BSS_7_head,
                  (unsigned int)(_BSS_7_tail - _BSS_7_head));
    zx_restart_enter();
}

int rnd(int range)
{
    unsigned int value = (unsigned int)RN;
    return range == 0 ? 0 : value % range;
}

int roll(int number, int sides)
{
    int total = 0;
    while (number--)
        total += rnd(sides) + 1;
    return total;
}

/*
 * Every caller passes a string literal that lives in its own bank's RODATA,
 * but get_item's body is in bank 1: paging it in replaces the caller's page
 * under that pointer, so the prompt printed garbage. This shim stays in fixed
 * memory, where no paging has happened yet and the literal is still readable,
 * and hands the banked half a copy that every bank can see.
 */
/* Sized for the longest purpose in use, "zap with" and "identify". */
#define ZX_PURPOSE_MAX 10

static char zx_item_purpose[ZX_PURPOSE_MAX];

THING *get_item(char *purpose, int type)
{
    strncpy(zx_item_purpose, purpose, sizeof zx_item_purpose - 1);
    zx_item_purpose[sizeof zx_item_purpose - 1] = '\0';
    return zx_get_item_banked(zx_item_purpose, type);
}

void playit(void)
{
    inv_type = INV_CLEAR;
    oldpos = hero;
    oldrp = roomin(&hero);
    while (playing) {
        zx_boot_stage = 0x52;
        command();
        ++zx_turn_count;
    }
}

void quit(int ignored)
{
    int old_y;
    int old_x;
    (void)ignored;
    getyx(curscr, old_y, old_x);
    msg("really quit?");
    if (readchar() == 'y') {
        playing = FALSE;
        clear();
        mvprintw(LINES - 2, 0, "You quit with %d gold pieces", purse);
        mvaddstr(15, 7, "Press R to restart");
        refresh();
        zx_wait_for_restart();
        stop_forever();
    }
    move(old_y, old_x);
    mpos = 0;
    count = 0;
    to_death = FALSE;
}

void fatal(char *message)
{
    mvaddstr(LINES - 2, 0, message);
    refresh();
    stop_forever();
}

void endit(int ignored) { (void)ignored; fatal("Okay, bye bye!"); }
void leave(int ignored) { endit(ignored); }
void my_exit(int ignored) { (void)ignored; stop_forever(); }
void tstp(int ignored) { (void)ignored; }
void shell(void) { msg("There is no shell on the ZX Spectrum."); }

void save_game(void) { msg("Saving is not available in this build."); }
bool restore(char *file, char **envp) { (void)file; (void)envp; return FALSE; }
void flush_type(void) { }

void init_check(void) { }
void open_score(void) { }
void setup(void) { raw(); noecho(); keypad(stdscr, TRUE); }
void getltchars(void) { }
void resetltchars(void) { }
void playltchars(void) { }

void md_init(void) { }
int md_readchar(void)
{
    unsigned int before = *(volatile unsigned int *)0x5c78;
    int ch = getch();

    if (zx_seed_collecting) {
        unsigned int elapsed =
            *(volatile unsigned int *)0x5c78 - before;
        seed = seed * 11109UL + 13849UL +
            ((rogue_seed_t)elapsed << 8) + (unsigned char)ch;
    }

    return ch == ' ' && zx_break() ? ESCAPE : ch;
}
int md_hasclreol(void) { return TRUE; }
int md_shellescape(void) { return FALSE; }
int md_getpid(void) { return 1; }
int md_getuid(void) { return 0; }
char *md_getusername(void) { return "Rogue"; }
char *md_gethomedir(void) { return ""; }
char *md_getrealname(int uid) { (void)uid; return "Rogue"; }
void md_sleep(int seconds) { (void)seconds; }
void md_normaluser(void) { }
void md_raw_standout(void) { standout(); }
void md_raw_standend(void) { standend(); }
void md_tstpsignal(void) { }
void md_tstphold(void) { }
void md_tstpresume(void) { }
void md_ignoreallsignals(void) { }
void md_onsignal_autosave(void) { }
void md_onsignal_exit(void) { }
void md_onsignal_default(void) { }
