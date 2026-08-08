#ifndef ROGUE_ZX_CURSES_H
#define ROGUE_ZX_CURSES_H

#include <stdio.h>
#include <stdarg.h>

typedef unsigned char bool;
typedef unsigned char chtype;

typedef struct zx_window {
    unsigned char _cury;
    unsigned char _curx;
    unsigned char _begy;
    unsigned char _begx;
    unsigned char _maxy;
    unsigned char _maxx;
    unsigned char _standout;
} WINDOW;

#define TRUE 1
#define FALSE 0
#define ERR (-1)
#define OK 0
#define A_CHARTEXT 0xff

/*
 * Physical character cells across the Spectrum screen. Overlays and the
 * message row work in these, because they use the 8x8 ROM font.
 */
#define ZX_VIEWPORT_COLS 32
/*
 * Dungeon columns shown at once. The map is drawn in the 4x8 font, two glyphs
 * to a cell, so it sees twice as much of the 80-column level as the cells
 * suggest. Panning still exists; it just has 16 columns of travel, not 48.
 */
#define ZX_MAP_COLS 64
#define ZX_VIEWPORT_NONE 0xffU

#define KEY_LEFT 256
#define KEY_RIGHT 257
#define KEY_UP 258
#define KEY_DOWN 259
#define KEY_HOME 260
#define KEY_PPAGE 261
#define KEY_NPAGE 262
#define KEY_END 263
#define KEY_A1 264
#define KEY_A3 265
#define KEY_B2 266
#define KEY_C1 267
#define KEY_C3 268
#define KEY_BACKSPACE 269

extern WINDOW *stdscr;
extern WINDOW *curscr;
extern int LINES;
extern int COLS;
extern volatile unsigned char zx_viewport_first_col;

#define getyx(win, y, x) do { (y) = (win)->_cury; (x) = (win)->_curx; } while (0)

WINDOW *initscr(void);
WINDOW *newwin(int lines, int cols, int begin_y, int begin_x);
WINDOW *subwin(WINDOW *parent, int lines, int cols, int begin_y, int begin_x);
int delwin(WINDOW *win);
int endwin(void);
int isendwin(void);
int move(int y, int x);
int wmove(WINDOW *win, int y, int x);
int mvcur(int old_y, int old_x, int new_y, int new_x);
int addch(int ch);
int mvaddch(int y, int x, int ch);
int waddch(WINDOW *win, int ch);
int mvwaddch(WINDOW *win, int y, int x, int ch);
int addstr(const char *str);
int mvaddstr(int y, int x, const char *str);
int waddstr(WINDOW *win, const char *str);
int printw(const char *fmt, ...);
int mvprintw(int y, int x, const char *fmt, ...);
int wprintw(WINDOW *win, const char *fmt, ...);
int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...);
int refresh(void);
int wrefresh(WINDOW *win);
int clear(void);
int erase(void);
int wclear(WINDOW *win);
int werase(WINDOW *win);
int clrtoeol(void);
int wclrtoeol(WINDOW *win);
int standout(void);
int standend(void);
int wstandout(WINDOW *win);
int wstandend(WINDOW *win);
int inch(void);
int mvinch(int y, int x);
int mvwinch(WINDOW *win, int y, int x);
int mvwin(WINDOW *win, int y, int x);
int touchwin(WINDOW *win);
int clearok(WINDOW *win, int flag);
int idlok(WINDOW *win, int flag);
int leaveok(WINDOW *win, int flag);
int keypad(WINDOW *win, int flag);
int raw(void);
int noecho(void);
int nocbreak(void);
int halfdelay(int tenths);
int getch(void);
int wgetnstr(WINDOW *win, char *str, int length);
int beep(void);
int box(WINDOW *win, int vertical, int horizontal);
int baudrate(void);
char *unctrl(int ch);
int erasechar(void);
int killchar(void);
void zx_viewport_set(unsigned char first_col);

#endif
