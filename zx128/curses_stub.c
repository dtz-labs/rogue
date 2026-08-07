#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "curses.h"

#define ZX_SCREEN_ROWS 24
#define ZX_SCREEN_COLS 80
#define ZX_VISIBLE_COLS 32
#define ZX_FORMAT_SIZE 160

static WINDOW screen_window;
static WINDOW scratch_window;
static unsigned char curses_ended;
static unsigned char physical_row[ZX_VISIBLE_COLS];

void zx_screen_set(unsigned int index, unsigned char value);
unsigned char zx_screen_get(unsigned int index);
void zx_screen_copy(unsigned int index, unsigned char *target,
                    unsigned char count);
void zx_screen_fill(unsigned int index, unsigned int count,
                    unsigned char value);

WINDOW *stdscr = &screen_window;
WINDOW *curscr = &screen_window;
int LINES = ZX_SCREEN_ROWS;
int COLS = ZX_SCREEN_COLS;

static unsigned int cell_index(WINDOW *win, int y, int x)
{
    return (unsigned int)(win->_begy + y) * ZX_SCREEN_COLS + win->_begx + x;
}

static int format_to_window(WINDOW *win, const char *fmt, va_list args)
{
    char buffer[ZX_FORMAT_SIZE];
    int result = vsnprintf(buffer, sizeof buffer, fmt, args);
    waddstr(win, buffer);
    return result;
}

static void draw_physical_cell(unsigned char row, unsigned char col,
                               unsigned char ch)
{
    const unsigned char *glyph;
    unsigned char scanline;

    if (ch < 32U || ch > 127U)
        ch = '?';
    glyph = (const unsigned char *)(0x3d00U +
            ((unsigned int)(ch - 32U) << 3));
    for (scanline = 0; scanline < 8U; ++scanline) {
        unsigned int pixel_y = ((unsigned int)row << 3) + scanline;
        unsigned int address = 0x4000U
            + ((pixel_y & 0xc0U) << 5)
            + ((pixel_y & 0x07U) << 8)
            + ((pixel_y & 0x38U) << 2)
            + col;
        *(unsigned char *)address = glyph[scanline];
    }
    ((unsigned char *)0x5800U)[(unsigned int)row * ZX_VISIBLE_COLS + col] = 7U;
}

static void render_physical_screen(void)
{
    unsigned char row;
    unsigned char col;
    unsigned char first_col;

    if (screen_window._curx > ZX_VISIBLE_COLS / 2U)
        first_col = screen_window._curx - ZX_VISIBLE_COLS / 2U;
    else
        first_col = 0;
    if (first_col > ZX_SCREEN_COLS - ZX_VISIBLE_COLS)
        first_col = ZX_SCREEN_COLS - ZX_VISIBLE_COLS;

    for (row = 0; row < ZX_SCREEN_ROWS; ++row) {
        unsigned char row_first = (row == 0 || row == ZX_SCREEN_ROWS - 1)
            ? 0 : first_col;
        zx_screen_copy((unsigned int)row * ZX_SCREEN_COLS + row_first,
                       physical_row, ZX_VISIBLE_COLS);
        for (col = 0; col < ZX_VISIBLE_COLS; ++col)
            draw_physical_cell(row, col, physical_row[col]);
    }
}

WINDOW *initscr(void)
{
    memset(&screen_window, 0, sizeof screen_window);
    screen_window._maxy = ZX_SCREEN_ROWS;
    screen_window._maxx = ZX_SCREEN_COLS;
    curses_ended = FALSE;
    erase();
    return stdscr;
}

WINDOW *newwin(int lines, int cols, int begin_y, int begin_x)
{
    memset(&scratch_window, 0, sizeof scratch_window);
    scratch_window._maxy = (unsigned char)lines;
    scratch_window._maxx = (unsigned char)cols;
    scratch_window._begy = (unsigned char)begin_y;
    scratch_window._begx = (unsigned char)begin_x;
    return &scratch_window;
}

WINDOW *subwin(WINDOW *parent, int lines, int cols, int begin_y, int begin_x)
{
    (void)parent;
    return newwin(lines, cols, begin_y, begin_x);
}

int delwin(WINDOW *win) { (void)win; return OK; }
int endwin(void) { curses_ended = TRUE; return OK; }
int isendwin(void) { return curses_ended; }

int wmove(WINDOW *win, int y, int x)
{
    if (y < 0 || x < 0 || y >= win->_maxy || x >= win->_maxx)
        return ERR;
    win->_cury = (unsigned char)y;
    win->_curx = (unsigned char)x;
    return OK;
}

int move(int y, int x) { return wmove(stdscr, y, x); }
int mvcur(int old_y, int old_x, int new_y, int new_x)
{
    (void)old_y;
    (void)old_x;
    return move(new_y, new_x);
}

int waddch(WINDOW *win, int ch)
{
    if (ch == '\n') {
        win->_curx = 0;
        if (win->_cury + 1 < win->_maxy)
            ++win->_cury;
        return OK;
    }
    if (win->_cury >= win->_maxy || win->_curx >= win->_maxx)
        return ERR;
    zx_screen_set(cell_index(win, win->_cury, win->_curx), (unsigned char)ch);
    if (++win->_curx >= win->_maxx) {
        win->_curx = 0;
        if (win->_cury + 1 < win->_maxy)
            ++win->_cury;
    }
    return OK;
}

int addch(int ch) { return waddch(stdscr, ch); }
int mvaddch(int y, int x, int ch) { return move(y, x) == ERR ? ERR : addch(ch); }
int mvwaddch(WINDOW *win, int y, int x, int ch)
{
    return wmove(win, y, x) == ERR ? ERR : waddch(win, ch);
}

int waddstr(WINDOW *win, const char *str)
{
    while (*str)
        waddch(win, *str++);
    return OK;
}

int addstr(const char *str) { return waddstr(stdscr, str); }
int mvaddstr(int y, int x, const char *str)
{
    return move(y, x) == ERR ? ERR : addstr(str);
}

int printw(const char *fmt, ...)
{
    int result;
    va_list args;
    va_start(args, fmt);
    result = format_to_window(stdscr, fmt, args);
    va_end(args);
    return result;
}

int mvprintw(int y, int x, const char *fmt, ...)
{
    int result;
    va_list args;
    if (move(y, x) == ERR)
        return ERR;
    va_start(args, fmt);
    result = format_to_window(stdscr, fmt, args);
    va_end(args);
    return result;
}

int wprintw(WINDOW *win, const char *fmt, ...)
{
    int result;
    va_list args;
    va_start(args, fmt);
    result = format_to_window(win, fmt, args);
    va_end(args);
    return result;
}

int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...)
{
    int result;
    va_list args;
    if (wmove(win, y, x) == ERR)
        return ERR;
    va_start(args, fmt);
    result = format_to_window(win, fmt, args);
    va_end(args);
    return result;
}

int refresh(void) { render_physical_screen(); return OK; }
int wrefresh(WINDOW *win) { (void)win; return refresh(); }

int werase(WINDOW *win)
{
    int y;
    for (y = 0; y < win->_maxy; ++y)
        zx_screen_fill(cell_index(win, y, 0), win->_maxx, ' ');
    return wmove(win, 0, 0);
}

int erase(void) { return werase(stdscr); }
int clear(void) { return erase(); }
int wclear(WINDOW *win) { return werase(win); }

int wclrtoeol(WINDOW *win)
{
    zx_screen_fill(cell_index(win, win->_cury, win->_curx),
                   win->_maxx - win->_curx, ' ');
    return OK;
}

int clrtoeol(void) { return wclrtoeol(stdscr); }
int wstandout(WINDOW *win) { win->_standout = TRUE; return OK; }
int wstandend(WINDOW *win) { win->_standout = FALSE; return OK; }
int standout(void) { return wstandout(stdscr); }
int standend(void) { return wstandend(stdscr); }

int mvwinch(WINDOW *win, int y, int x)
{
    if (y < 0 || x < 0 || y >= win->_maxy || x >= win->_maxx)
        return ERR;
    return zx_screen_get(cell_index(win, y, x));
}

int mvinch(int y, int x) { return mvwinch(stdscr, y, x); }
int inch(void) { return mvinch(stdscr->_cury, stdscr->_curx); }

int mvwin(WINDOW *win, int y, int x)
{
    win->_begy = (unsigned char)y;
    win->_begx = (unsigned char)x;
    return OK;
}

int touchwin(WINDOW *win) { (void)win; return OK; }
int clearok(WINDOW *win, int flag) { (void)win; (void)flag; return OK; }
int idlok(WINDOW *win, int flag) { (void)win; (void)flag; return OK; }
int leaveok(WINDOW *win, int flag) { (void)win; (void)flag; return OK; }
int keypad(WINDOW *win, int flag) { (void)win; (void)flag; return OK; }
int raw(void) { return OK; }
int noecho(void) { return OK; }
int nocbreak(void) { return OK; }
int halfdelay(int tenths) { (void)tenths; return OK; }
int baudrate(void) { return 9600; }
int beep(void) { return OK; }

int getch(void)
{
    return getchar();
}

int wgetnstr(WINDOW *win, char *str, int length)
{
    int ch;
    int used = 0;
    while (used + 1 < length) {
        ch = getch();
        if (ch == '\r' || ch == '\n')
            break;
        if ((ch == 8 || ch == 12) && used) {
            --used;
            continue;
        }
        str[used++] = (char)ch;
        waddch(win, ch);
    }
    str[used] = '\0';
    return OK;
}

int box(WINDOW *win, int vertical, int horizontal)
{
    (void)win;
    (void)vertical;
    (void)horizontal;
    return OK;
}

char *unctrl(int ch)
{
    static char text[3];
    ch &= 0xff;
    if (ch < 32) {
        text[0] = '^';
        text[1] = (char)(ch + '@');
        text[2] = '\0';
    } else {
        text[0] = (char)ch;
        text[1] = '\0';
    }
    return text;
}

int erasechar(void) { return 12; }
int killchar(void) { return 21; }
