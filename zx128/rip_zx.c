#include <curses.h>
#include <string.h>
#include "rogue.h"

static void reset_machine(void)
{
__asm
    rst 0
__endasm;
}

static void wait_for_key_release(void)
{
__asm
rip_wait_for_key_release_loop:
    xor a
    in a, (0xfe)
    and 0x1f
    cp 0x1f
    jr nz, rip_wait_for_key_release_loop
__endasm;
}

static void wait_for_reset(void)
{
    char ch;

    do {
        ch = readchar();
    } while (ch != 'r' && ch != 'R');
    wait_for_key_release();
    reset_machine();
    my_exit(0); /* Defensive fallback: RST 0 does not return on a Spectrum. */
}

void score(int amount, int flags, char monst)
{
    (void)flags;
    (void)monst;
    mvprintw(12, 8, "Score: %d", amount);
}

void death(char monst)
{
    clear();
    mvprintw(10, 8, "Killed by %s", killname(monst, FALSE));
    score(purse, 0, monst);
    mvaddstr(15, 8, "Press R to reset");
    move(LINES - 1, 0);
    refresh();
    playing = FALSE;
    wait_for_reset();
}

char death_monst(void)
{
    return (char)('A' + rnd(26));
}

char *killname(char monst, bool doart)
{
    (void)doart;
    if (monst == 's')
        return "starvation";
    if (monst >= 'A' && monst <= 'Z')
        return monsters[monst - 'A'].m_name;
    return "a monster";
}

void total_winner(void)
{
    clear();
    mvaddstr(10, 3, "You are the total winner!");
    mvaddstr(15, 8, "Press R to reset");
    move(LINES - 1, 0);
    refresh();
    playing = FALSE;
    wait_for_reset();
}

int center(char *text)
{
    int width = (int)strlen(text);
    return width < COLS ? (COLS - width) / 2 : 0;
}
