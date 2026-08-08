#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "curses.h"
#include "rogue.h"
#include "zx_banking.h"

#define ZX_SCREEN_ROWS 24
#define ZX_SCREEN_COLS 80
#define ZX_VISIBLE_COLS ZX_VIEWPORT_COLS
#define ZX_FORMAT_SIZE 160

static WINDOW screen_window;
static WINDOW scratch_window;
static unsigned char curses_ended;
/*
 * Dirty tracking is per row, but by column span rather than whole-row. A step
 * changes two or three cells; redrawing all 32 of them was most of the cost of
 * a move, and a vertical step pays it twice because it dirties two rows.
 * first > last means the row is clean.
 */
static unsigned char dirty_first[ZX_SCREEN_ROWS];
static unsigned char dirty_last[ZX_SCREEN_ROWS];
volatile unsigned char zx_viewport_first_col;
volatile unsigned char zx_rendered_rows;
/* Cells actually redrawn by the last refresh: the real cost of a move. */
volatile unsigned int zx_rendered_cells;
volatile unsigned char zx_refresh_count;
volatile unsigned char zx_refresh_turn;

extern volatile unsigned char zx_turn_count;
extern int mpos;

void zx_screen_set(unsigned int index, unsigned char value);
unsigned char zx_screen_get(unsigned int index);
void zx_screen_fill(unsigned int index, unsigned int count,
                    unsigned char value);
void zx_render_row_span(unsigned char row, unsigned char first_col,
                        unsigned char first_cell,
                        unsigned char last_cell) ZX_BANKED_6;

WINDOW *stdscr = &screen_window;
WINDOW *curscr = &screen_window;
int LINES = ZX_SCREEN_ROWS;
int COLS = ZX_SCREEN_COLS;

static unsigned int cell_index(WINDOW *win, int y, int x)
{
    return (unsigned int)(win->_begy + y) * ZX_SCREEN_COLS + win->_begx + x;
}

static void mark_dirty_cols(unsigned char row, unsigned char from,
                            unsigned char to)
{
    if (from < dirty_first[row])
        dirty_first[row] = from;
    if (to > dirty_last[row])
        dirty_last[row] = to;
}

static void mark_dirty(unsigned char row)
{
    mark_dirty_cols(row, 0, ZX_SCREEN_COLS - 1U);
}

static void mark_clean(unsigned char row)
{
    dirty_first[row] = ZX_SCREEN_COLS;
    dirty_last[row] = 0;
}

static int format_to_window(WINDOW *win, const char *fmt, va_list args)
{
    char buffer[ZX_FORMAT_SIZE];
    int result = vsnprintf(buffer, sizeof buffer, fmt, args);
    waddstr(win, buffer);
    return result;
}

static void render_physical_screen(void)
{
    unsigned char row;
    unsigned char first_col;

    zx_rendered_rows = 0;
    zx_rendered_cells = 0;
    first_col = zx_viewport_first_col == ZX_VIEWPORT_NONE
        ? 0 : zx_viewport_first_col;

    for (row = 0; row < ZX_SCREEN_ROWS; ++row) {
        /* Messages and the status line are not panned with the dungeon. */
        int row_first = (row == 0 || row == ZX_SCREEN_ROWS - 1) ? 0 : first_col;
        int lo = dirty_first[row];
        int hi = dirty_last[row];

        if (lo > hi)
            continue;
        mark_clean(row);
        if (hi < row_first || lo >= row_first + ZX_MAP_COLS)
            continue;               /* the change is outside the visible span */
        if (lo < row_first)
            lo = row_first;
        if (hi >= row_first + ZX_MAP_COLS)
            hi = row_first + ZX_MAP_COLS - 1;
        ++zx_rendered_rows;
        zx_rendered_cells += (unsigned int)
            (((hi - row_first) >> 1) - ((lo - row_first) >> 1) + 1);
        zx_render_row_span(row, (unsigned char)row_first,
                           (unsigned char)((lo - row_first) >> 1),
                           (unsigned char)((hi - row_first) >> 1));
    }
}

void zx_viewport_set(unsigned char first_col)
{
    unsigned char row;

    if (first_col == zx_viewport_first_col)
        return;
    zx_viewport_first_col = first_col;
    for (row = 1; row < ZX_SCREEN_ROWS - 1U; ++row)
        mark_dirty(row);
}

WINDOW *initscr(void)
{
    memset(&screen_window, 0, sizeof screen_window);
    screen_window._maxy = ZX_SCREEN_ROWS;
    screen_window._maxx = ZX_SCREEN_COLS;
    curses_ended = FALSE;
    zx_viewport_first_col = ZX_VIEWPORT_NONE;
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
    unsigned char row;

    if (ch == '\n') {
        win->_curx = 0;
        if (win->_cury + 1 < win->_maxy)
            ++win->_cury;
        return OK;
    }
    if (win->_cury >= win->_maxy || win->_curx >= win->_maxx)
        return ERR;
    row = win->_begy + win->_cury;
    zx_screen_set(cell_index(win, win->_cury, win->_curx), (unsigned char)ch);
    mark_dirty_cols(row, (unsigned char)(win->_begx + win->_curx),
                    (unsigned char)(win->_begx + win->_curx));
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

int refresh(void)
{
    render_physical_screen();
    zx_refresh_turn = zx_turn_count;
    ++zx_refresh_count;
    return OK;
}
int wrefresh(WINDOW *win) { (void)win; return refresh(); }

int werase(WINDOW *win)
{
    int y;
    for (y = 0; y < win->_maxy; ++y) {
        zx_screen_fill(cell_index(win, y, 0), win->_maxx, ' ');
        mark_dirty(win->_begy + y);
    }
    return wmove(win, 0, 0);
}

int erase(void) { return werase(stdscr); }
int clear(void)
{
    /* A full-screen redraw starts a new view, not the old dungeon viewport. */
    zx_viewport_first_col = ZX_VIEWPORT_NONE;
    return erase();
}
int wclear(WINDOW *win) { return werase(win); }

int wclrtoeol(WINDOW *win)
{
    zx_screen_fill(cell_index(win, win->_cury, win->_curx),
                   win->_maxx - win->_curx, ' ');
    mark_dirty_cols(win->_begy + win->_cury,
                    (unsigned char)(win->_begx + win->_curx),
                    (unsigned char)(win->_begx + win->_maxx - 1));
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

int touchwin(WINDOW *win)
{
    unsigned char row;

    (void)win;
    for (row = 0; row < ZX_SCREEN_ROWS; ++row)
        mark_dirty(row);
    return OK;
}
int clearok(WINDOW *win, int flag)
{
    if (flag)
        touchwin(win);
    return OK;
}
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
    return fgetc_cons();
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
    if (ch == ESCAPE)
        return "BREAK";
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
