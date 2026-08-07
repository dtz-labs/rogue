#ifndef ROGUE_ZX_MESSAGE_H
#define ROGUE_ZX_MESSAGE_H

#define ZX_MSG_COLS 32
#define ZX_MSG_PAGE_COLS (2 * ZX_MSG_COLS)
#define ZX_MSG_MAX (ZX_MSG_PAGE_COLS - sizeof "--More--")

extern char zx_msgbuf[ZX_MSG_MAX + 1];
extern int zx_msg_newpos;

#endif
