# Changelog

Releases of the ZX Spectrum 128K port of Rogue 5.4.4. Every tagged TAP has
cleared the same three gates: a linker-layout check on the fixed image and all
six banks, a static check that no call crosses a bank boundary without paging
first, and a gameplay run in a real ZEsarUX emulator.

Versions here are the port's own (`ZX128_VERSION` in `zx128/Makefile`); the
game itself is Rogue 5.4.4.

## 0.2.0

The whole screen moves to a 4x8 font. Two glyphs share a character cell, so
**64 of the level's 80 columns are visible at once** instead of 32, and the
viewport has 16 columns of travel rather than 48.

### Display

- Messages, the status line, the inventory overlay and every text screen use
  the same font as the map. The mixed 8x8/4x8 arrangement in 0.1.1 existed only
  because the font table carried ASCII 32..90 and prose came out in capitals;
  the full range including lower case was already in z88dk and is now linked.
- Rooms are drawn as closed outlines. Rogue stores a plain `-` in all four
  corners, so corners are recognised from the cells around them rather than
  read from the map.
- Redrawn map glyphs: solid walls, floor as a single centred pixel, corridors
  as a dash rather than a dot, doors as a small square, and a staircase that
  no longer disappears into the floor.
- The startup help is laid out in two columns and carries
  `dtz-labs.github.io/rogue`; the name prompt and the "entering the dungeon"
  notice are centred.

### Speed

- Redraws track changed **columns**, not whole rows. An ordinary step costs 2-5
  cells where it used to cost 32-96, and a vertical step no longer costs twice
  a horizontal one.
- A full repaint went from 1345 ms to 577 ms: the draw loop was unpacking two
  scanlines from each packed font byte one at a time, and was recomputing the
  eight scanline addresses of a row once per cell.
- Blank spans are cleared with `memset` instead of drawn glyph by glyph.

### Fixed

- Full-screen overlays wiped the map behind them. The scratch window and the
  real screen are the same cell buffer on this target, unlike real curses, so
  `wclear` on the scratch window erased the dungeon and nothing put it back.
  Affected the potion of magic detection, the scroll of food detection and the
  `?` help screen.
- Text on the map rows was drawn with map glyphs, so the hyphen in the help
  screen's URL rendered as a wall segment and full stops as floor pixels.
- A door directly under a room corner suppressed the corner.

### Known

- `add_line`'s Overwrite and Slow inventory styles, reachable from the options
  screen, still take a code path with the overlay bug described above.

## 0.1.2

A bug fix and a memory cleanup. No gameplay changes.

- The `v` command printed text from another memory bank instead of the version
  number: the version string lived in a paged bank while the pointer to it was
  resident.
- The C library console driver and its unused 768-byte font, the unused printf
  conversions, the save-file encryption strings and the atexit stack left the
  build. Free resident RAM went from 77 to 2702 bytes and the TAP from 112898
  to 110195 bytes.

## 0.1.1

Fixes for things that were visibly wrong while playing 0.1.0.

- Seven functions lived in a bank but were declared without the annotation that
  makes callers page it in, so the call ran whatever code sat at that address
  in the mapped bank. Quaffing a potion answered "there is nothing on it to
  read"; eating did nothing; both could drop the player into 128 BASIC.
- Prompts and hallucinated colour names printed garbage, because callers passed
  string literals living in their own bank across a bank switch.
- The status line ran out mid-Hp at 32 columns. It became the first row drawn
  in the 4x8 font, fitting 64 characters in 32 cells.
- CI gained a static check pairing the linker map against the headers, which
  reports all seven paging defects on 0.1.0 and is clean from here on.

## 0.1.0

First tagged build. Procedural rooms, passages and mazes, monsters, combat,
objects, dungeon progression, full-screen inventory and help, death and victory
screens, and a cold in-program restart. Saving, the score file, shell escape
and Unix account integration are deliberately absent on this target.
