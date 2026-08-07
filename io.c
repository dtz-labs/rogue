/*
 * Various input/output functions
 *
 * @(#)io.c	4.32 (Berkeley) 02/05/99
 */

#include <curses.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include "rogue.h"
#ifdef ZX128
#include "zx_message.h"
#endif

/*
 * msg:
 *	Display a message at the top of the screen.
 */
#ifdef ZX128
#define MAXMSG ZX_MSG_MAX
#define msgbuf zx_msgbuf
#define newpos zx_msg_newpos
#else
#define MAXMSG	(NUMCOLS - sizeof "--More--")
#endif

#ifdef ZX128
char zx_msgbuf[MAXMSG+1];
int zx_msg_newpos;
#else
static char msgbuf[2*MAXMSG+1];
static int newpos = 0;
#endif

/* VARARGS1 */
int
msg(char *fmt, ...)
{
    va_list args;

    /*
     * if the string is "", just clear the line
     */
    if (*fmt == '\0')
    {
	move(0, 0);
	clrtoeol();
	mpos = 0;
	return ~ESCAPE;
    }
    /*
     * otherwise add to the message and flush it out
    */
    va_start(args, fmt);
    if (doadd(fmt, args) == ESCAPE)
    {
	va_end(args);
	return ESCAPE;
    }
    va_end(args);
    return endmsg();
}

/*
 * addmsg:
 *	Add things to the current message
 */
/* VARARGS1 */
void
addmsg(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    doadd(fmt, args);
    va_end(args);
}

/*
 * endmsg:
 *	Display a new msg (giving him a chance to see the previous one
 *	if it is up there with the --More--)
 */
#ifndef ZX128
int
endmsg()
{
    char ch;

    if (save_msg)
	strcpy(huh, msgbuf);
    /*
     * All messages should start with uppercase, except ones that
     * start with a pack addressing character
     */
    if (islower(msgbuf[0]) && !lower_msg && msgbuf[1] != ')')
	msgbuf[0] = (char) toupper(msgbuf[0]);
#ifdef ZX128
    /* A second short message can use the free physical message row. */
    if (mpos && mpos <= ZX_MSG_COLS && newpos &&
	newpos <= ZX_MSG_COLS - (sizeof "--More--" - 1))
    {
	mvaddstr(0, ZX_MSG_COLS, msgbuf);
	clrtoeol();
	mpos = ZX_MSG_COLS + newpos;
	newpos = 0;
	msgbuf[0] = '\0';
	refresh();
	return ~ESCAPE;
    }
#endif
    if (mpos)
    {
	look(FALSE);
#ifdef ZX128
	if (mpos > ZX_MSG_PAGE_COLS - (sizeof "--More--" - 1))
	    mpos = ZX_MSG_PAGE_COLS - (sizeof "--More--" - 1);
#endif
	mvaddstr(0, mpos, "--More--");
#ifdef ZX128
	mpos += sizeof "--More--" - 1;
#endif
	refresh();
	if (!msg_esc)
	    wait_for(' ');
	else
	{
	    while ((ch = readchar()) != ' ')
		if (ch == ESCAPE)
		{
		    msgbuf[0] = '\0';
		    mpos = 0;
		    newpos = 0;
		    msgbuf[0] = '\0';
		    return ESCAPE;
		}
	}
    }
    mvaddstr(0, 0, msgbuf);
    clrtoeol();
    mpos = newpos;
    newpos = 0;
    msgbuf[0] = '\0';
    refresh();
    return ~ESCAPE;
}
#endif

/*
 * doadd:
 *	Perform an add onto the message buffer
 */
int
doadd(char *fmt, va_list args)
{
    static char buf[MAXSTR];
#ifdef ZX128
    char *src;
#endif

    /*
     * Do the printf into buf
     */
#ifdef ZX128
    vsnprintf(buf, sizeof buf, fmt, args);
    src = buf;
    while (*src)
    {
	if (newpos >= MAXMSG)
	    if (endmsg() == ESCAPE)
		return ESCAPE;
	msgbuf[newpos++] = *src++;
    }
    msgbuf[newpos] = '\0';
#else
    vsprintf(buf, fmt, args);
    if (strlen(buf) + newpos >= MAXMSG)
        endmsg(); 
    strcat(msgbuf, buf);
    newpos = (int) strlen(msgbuf);
#endif
    return ~ESCAPE;
}

/*
 * step_ok:
 *	Returns true if it is ok to step on ch
 */
int
step_ok(int ch)
{
    switch (ch)
    {
	case ' ':
	case '|':
	case '-':
	    return FALSE;
	default:
	    return (!isalpha(ch));
    }
}

/*
 * readchar:
 *	Reads and returns a character, checking for gross input errors
 */
char
readchar()
{
    char ch;

    ch = (char) md_readchar();

    if (ch == 3)
    {
	quit(0);
        return(27);
    }

    return(ch);
}

/*
 * status:
 *	Display the important stats line.  Keep the cursor where it was.
 */
void
status()
{
    register int oy, ox, temp;
    static int hpwidth = 0;
    static int s_hungry = 0;
    static int s_lvl = 0;
    static int s_pur = -1;
    static int s_hp = 0;
    static int s_arm = 0;
    static str_t s_str = 0;
    static rogue_exp_t s_exp = 0;
    static char *state_name[] =
    {
	"", "Hungry", "Weak", "Faint"
    };

    /*
     * If nothing has changed since the last status, don't
     * bother.
     */
    temp = (cur_armor != NULL ? cur_armor->o_arm : pstats.s_arm);
    if (s_hp == pstats.s_hpt && s_exp == pstats.s_exp && s_pur == purse
	&& s_arm == temp && s_str == pstats.s_str && s_lvl == level
	&& s_hungry == hungry_state
	&& !stat_msg
	)
	    return;

    s_arm = temp;

    getyx(stdscr, oy, ox);
    if (s_hp != max_hp)
    {
	temp = max_hp;
	s_hp = max_hp;
	for (hpwidth = 0; temp; hpwidth++)
	    temp /= 10;
    }

    /*
     * Save current status
     */
    s_lvl = level;
    s_pur = purse;
    s_hp = pstats.s_hpt;
    s_str = pstats.s_str;
    s_exp = pstats.s_exp; 
    s_hungry = hungry_state;

    if (stat_msg)
    {
	move(0, 0);
	msg("Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %2d(%d)  Arm: %-2d  Exp: %d/%ld  %s",
	    level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp, pstats.s_str,
	    max_stats.s_str, 10 - s_arm, pstats.s_lvl, (long)pstats.s_exp,
	    state_name[hungry_state]);
    }
    else
    {
	move(STATLINE, 0);
                
	printw("Level: %d  Gold: %-5d  Hp: %*d(%*d)  Str: %2d(%d)  Arm: %-2d  Exp: %d/%ld  %s",
	    level, purse, hpwidth, pstats.s_hpt, hpwidth, max_hp, pstats.s_str,
	    max_stats.s_str, 10 - s_arm, pstats.s_lvl, (long)pstats.s_exp,
	    state_name[hungry_state]);
    }

    clrtoeol();
    move(oy, ox);
}

/*
 * wait_for
 *	Sit around until the guy types the right key
 */
void
wait_for(int ch)
{
    register char c;

    if (ch == '\n')
        while ((c = readchar()) != '\n' && c != '\r')
	    continue;
    else
        while (readchar() != ch)
	    continue;
}

/*
 * show_win:
 *	Function used to display a window and wait before returning
 */
void
show_win(char *message)
{
    WINDOW *win;

    win = hw;
    wmove(win, 0, 0);
    waddstr(win, message);
    touchwin(win);
#ifdef ZX128
    /* Let the renderer put columns 32..63 on the second physical row. */
    mpos = win->_curx;
#endif
    wmove(win, hero.y, hero.x);
    wrefresh(win);
    wait_for(' ');
#ifdef ZX128
    wmove(win, 0, 0);
    wclrtoeol(win);
    mpos = 0;
#endif
    clearok(curscr, TRUE);
    touchwin(stdscr);
}
