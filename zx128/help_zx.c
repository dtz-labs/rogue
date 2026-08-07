#include <curses.h>
#include "rogue.h"

extern volatile unsigned char zx_boot_stage;

void
zx_startup_help(void)
{
    int ch;

    clear();
    mvaddstr(0, 7, "ROGUE ZX128 - KEYS");
    mvaddstr(2, 1, "h j k l / y u b n   move");
    mvaddstr(3, 1, "SHIFT + direction   run");
    mvaddstr(4, 1, "f/F + direction     fight");
    mvaddstr(6, 1, "i inventory      , pick up");
    mvaddstr(7, 1, "q potion         r scroll");
    mvaddstr(8, 1, "e food           d drop");
    mvaddstr(9, 1, "w wield          W wear");
    mvaddstr(10, 1, "T take off       P/R rings");
    mvaddstr(11, 1, "t throw          z zap");
    mvaddstr(12, 1, "s search         ^ trap");
    mvaddstr(13, 1, ">/< stairs        . rest");
    mvaddstr(15, 1, "? command help   o options");
    mvaddstr(16, 1, "BREAK cancels a command");
    mvaddstr(19, 5, "SPACE - enter your name");
    zx_boot_stage = 'H';
    refresh();
    do {
        ch = readchar();
    } while (ch != ' ' && ch != '\n' && ch != '\r' && ch != ESCAPE);
    zx_wait_for_key_release();
    clear();
}

/* Kept with command.c in bank 0 so its strings are valid while displayed. */
struct h_list helpstr[] = {
    {'?',       "\tprints help",                          TRUE},
    {'/',       "\tidentify object",                     TRUE},
    {'h',       "\tleft",                                TRUE},
    {'j',       "\tdown",                                TRUE},
    {'k',       "\tup",                                  TRUE},
    {'l',       "\tright",                               TRUE},
    {'y',       "\tup & left",                           TRUE},
    {'u',       "\tup & right",                          TRUE},
    {'b',       "\tdown & left",                         TRUE},
    {'n',       "\tdown & right",                        TRUE},
    {'H',       "\trun left",                            FALSE},
    {'J',       "\trun down",                            FALSE},
    {'K',       "\trun up",                              FALSE},
    {'L',       "\trun right",                           FALSE},
    {'Y',       "\trun up & left",                       FALSE},
    {'U',       "\trun up & right",                      FALSE},
    {'B',       "\trun down & left",                     FALSE},
    {'N',       "\trun down & right",                    FALSE},
    {CTRL('H'), "\trun left until adjacent",             FALSE},
    {CTRL('J'), "\trun down until adjacent",             FALSE},
    {CTRL('K'), "\trun up until adjacent",               FALSE},
    {CTRL('L'), "\trun right until adjacent",            FALSE},
    {CTRL('Y'), "\trun up & left until adjacent",        FALSE},
    {CTRL('U'), "\trun up & right until adjacent",       FALSE},
    {CTRL('B'), "\trun down & left until adjacent",      FALSE},
    {CTRL('N'), "\trun down & right until adjacent",     FALSE},
    {'\0',      "\t<SHIFT><dir>: run that way",          TRUE},
    {'\0',      "\t<CTRL><dir>: run till adjacent",      TRUE},
    {'f',       "<dir>\tfight till death or near death", TRUE},
    {'t',       "<dir>\tthrow something",                TRUE},
    {'m',       "<dir>\tmove onto without picking up",   TRUE},
    {'z',       "<dir>\tzap a wand in a direction",      TRUE},
    {'^',       "<dir>\tidentify trap type",             TRUE},
    {'s',       "\tsearch for trap/secret door",         TRUE},
    {'>',       "\tgo down a staircase",                 TRUE},
    {'<',       "\tgo up a staircase",                   TRUE},
    {'.',       "\trest for a turn",                     TRUE},
    {',',       "\tpick something up",                   TRUE},
    {'i',       "\tinventory",                           TRUE},
    {'I',       "\tinventory single item",               TRUE},
    {'q',       "\tquaff potion",                        TRUE},
    {'r',       "\tread scroll",                         TRUE},
    {'e',       "\teat food",                            TRUE},
    {'w',       "\twield a weapon",                      TRUE},
    {'W',       "\twear armor",                          TRUE},
    {'T',       "\ttake armor off",                      TRUE},
    {'P',       "\tput on ring",                         TRUE},
    {'R',       "\tremove ring",                         TRUE},
    {'d',       "\tdrop object",                         TRUE},
    {'c',       "\tcall object",                         TRUE},
    {'a',       "\trepeat last command",                 TRUE},
    {')',       "\tprint current weapon",                TRUE},
    {']',       "\tprint current armor",                 TRUE},
    {'=',       "\tprint current rings",                 TRUE},
    {'@',       "\tprint current stats",                 TRUE},
    {'D',       "\trecall what's been discovered",       TRUE},
    {'o',       "\texamine/set options",                 TRUE},
    {CTRL('R'), "\tredraw screen",                       TRUE},
    {CTRL('P'), "\trepeat last message",                 TRUE},
    {ESCAPE,    "\tcancel command",                      TRUE},
    {'S',       "\tsave game",                           TRUE},
    {'Q',       "\tquit",                                TRUE},
    {'!',       "\tshell escape",                        TRUE},
    {'F',       "<dir>\tfight till either of you dies",  TRUE},
    {'v',       "\tprint version number",                TRUE},
    {0,         NULL}
};
