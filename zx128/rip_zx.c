#include <curses.h>
#include <string.h>
#include "rogue.h"

void score(int amount, int flags, char monst)
{
    (void)flags;
    (void)monst;
    mvprintw(LINES - 2, 0, "Score: %d", amount);
    refresh();
}

void death(char monst)
{
    clear();
    mvprintw(10, 8, "Killed by %s", killname(monst, FALSE));
    score(purse, 0, monst);
    playing = FALSE;
    my_exit(0);
}

char death_monst(void)
{
    return (char)('A' + rnd(26));
}

char *killname(char monst, bool doart)
{
    static char unknown[] = "a monster";
    static char starvation[] = "starvation";
    (void)doart;
    if (monst == 's')
        return starvation;
    if (monst >= 'A' && monst <= 'Z')
        return monsters[monst - 'A'].m_name;
    return unknown;
}

void total_winner(void)
{
    clear();
    mvaddstr(10, 16, "You are the total winner!");
    refresh();
    playing = FALSE;
    my_exit(0);
}

int center(char *text)
{
    int width = (int)strlen(text);
    return width < COLS ? (COLS - width) / 2 : 0;
}
