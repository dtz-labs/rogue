#include <ctype.h>
#include <string.h>
#include <curses.h>
#include "rogue.h"
#include "zx_message.h"

int endmsg(void)
{
    char ch;

    if (save_msg)
        strcpy(huh, zx_msgbuf);
    if (islower(zx_msgbuf[0]) && !lower_msg && zx_msgbuf[1] != ')')
        zx_msgbuf[0] = (char) toupper(zx_msgbuf[0]);

    /* A second short message can use the free physical message row. */
    if (mpos && mpos <= ZX_MSG_COLS && zx_msg_newpos &&
        zx_msg_newpos <= ZX_MSG_COLS - (sizeof "--More--" - 1))
    {
        mvaddstr(0, ZX_MSG_COLS, zx_msgbuf);
        clrtoeol();
        mpos = ZX_MSG_COLS + zx_msg_newpos;
        zx_msg_newpos = 0;
        zx_msgbuf[0] = '\0';
        refresh();
        return ~ESCAPE;
    }

    if (mpos)
    {
        look(FALSE);
        if (mpos > ZX_MSG_PAGE_COLS - (sizeof "--More--" - 1))
            mpos = ZX_MSG_PAGE_COLS - (sizeof "--More--" - 1);
        mvaddstr(0, mpos, "--More--");
        mpos += sizeof "--More--" - 1;
        refresh();
        if (!msg_esc)
            wait_for(' ');
        else
        {
            while ((ch = readchar()) != ' ')
                if (ch == ESCAPE)
                {
                    zx_msgbuf[0] = '\0';
                    mpos = 0;
                    zx_msg_newpos = 0;
                    return ESCAPE;
                }
        }
    }
    mvaddstr(0, 0, zx_msgbuf);
    clrtoeol();
    mpos = zx_msg_newpos;
    zx_msg_newpos = 0;
    zx_msgbuf[0] = '\0';
    refresh();
    return ~ESCAPE;
}
