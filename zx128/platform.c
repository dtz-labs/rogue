#include <curses.h>
#include <string.h>
#include "rogue.h"

volatile unsigned char zx_boot_stage;
volatile unsigned char zx_turn_count;

static void stop_forever(void)
{
    for (;;)
        ;
}

int main(void)
{
    zx_boot_stage = 0x10;
    strcpy(fruit, "slime-mold");
    dnum = 1;
    seed = 0x13579bdfL;

    initscr();
    zx_boot_stage = 0x20;
    init_probs();
    init_player();
    init_names();
    init_colors();
    init_stones();
    init_materials();
    setup();
    zx_boot_stage = 0x30;

    hw = newwin(LINES, COLS, 0, 0);
    new_level();
    zx_boot_stage = 0x40;
    start_daemon(zx_cb_runners, 0, AFTER);
    zx_boot_stage = 0x41;
    start_daemon(zx_cb_doctor, 0, AFTER);
    zx_boot_stage = 0x42;
    fuse(zx_cb_swander, 0, WANDERTIME, AFTER);
    zx_boot_stage = 0x43;
    start_daemon(zx_cb_stomach, 0, AFTER);
    zx_boot_stage = 0x44;
    playit();
    return 0;
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

void playit(void)
{
    inv_type = INV_SLOW;
    oldpos = hero;
    zx_boot_stage = 0x50;
    oldrp = roomin(&hero);
    zx_boot_stage = 0x51;
    while (playing) {
        zx_boot_stage = 0x52;
        command();
        ++zx_turn_count;
        zx_boot_stage = 0x53;
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
        refresh();
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
int md_readchar(void) { return getch(); }
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
