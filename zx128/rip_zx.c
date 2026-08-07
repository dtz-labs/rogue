#include <curses.h>
#include <string.h>
#include "rogue.h"

void zx_wait_for_restart(void)
{
    char ch;

    do {
        ch = readchar();
    } while (ch != 'r' && ch != 'R');
    zx_wait_for_key_release();
    zx_restart_game();
    my_exit(0); /* Defensive fallback: the cold restart does not return. */
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
    mvaddstr(15, 7, "Press R to restart");
    move(LINES - 1, 0);
    refresh();
    playing = FALSE;
    zx_wait_for_restart();
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
    mvaddstr(15, 7, "Press R to restart");
    move(LINES - 1, 0);
    refresh();
    playing = FALSE;
    zx_wait_for_restart();
}

int center(char *text)
{
    int width = (int)strlen(text);
    return width < COLS ? (COLS - width) / 2 : 0;
}
