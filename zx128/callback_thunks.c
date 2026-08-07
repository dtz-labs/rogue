#include <curses.h>
#include "rogue.h"

void zx_cb_runners(int arg) { (void)arg; runners(); }
void zx_cb_doctor(int arg) { (void)arg; doctor(); }
void zx_cb_swander(int arg) { (void)arg; swander(); }
void zx_cb_stomach(int arg) { (void)arg; stomach(); }
void zx_cb_rollwand(int arg) { (void)arg; rollwand(); }
void zx_cb_nohaste(int arg) { (void)arg; nohaste(); }
void zx_cb_unconfuse(int arg) { (void)arg; unconfuse(); }
void zx_cb_unsee(int arg) { (void)arg; unsee(); }
void zx_cb_sight(int arg) { (void)arg; sight(); }
void zx_cb_visuals(int arg) { (void)arg; visuals(); }
void zx_cb_come_down(int arg) { (void)arg; come_down(); }
void zx_cb_land(int arg) { (void)arg; land(); }
void zx_cb_turn_see(int arg) { (void)turn_see((bool)arg); }
