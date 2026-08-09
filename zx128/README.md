# Rogue 5.4.4 — ZX Spectrum 128K bring-up

This directory is the deliberately conservative first stage of a z88dk port.
It builds the original game modules into Spectrum 128K RAM banks and produces
a bootable TAP plus a checked linker map.

## Play in a browser

**[▶ Play the latest tested ZX128 build](https://dtz-labs.github.io/rogue/)**

The GitHub Pages player embeds JSSpeccy 3.2 as a Spectrum 128K and opens the
same `rogue-zx128.tap` artifact that passed the linker-layout checks and the
ZEsarUX smoke test. Click the player's **▶** button, press **SPACE** on the
in-game key reference, type a hero name, and press **Enter**.

The TAP can be downloaded without the embedded emulator from
[dtz-labs.github.io/rogue/rogue-zx128.tap](https://dtz-labs.github.io/rogue/rogue-zx128.tap).
JSSpeccy is distributed separately under GPL-3.0; its licence and exact source
link are included in the deployed Pages artifact.

Current scope:

- upstream baseline: `modern-rogue` at commit
  `546829b745ab8b1bca39b14f8bac5defd6c13fac`;
- z88dk classic `+zx` CRT with the SDCC frontend;
- SDCC keeps `IY` reserved for the Spectrum ROM keyboard/system variables;
- pageable code/data in banks 0, 1, 3, 4, 6 and 7;
- banks 2 and 5 remain the Spectrum's fixed RAM pages;
- save/restore, score files, shell escape, signals and Unix account handling
  are replaced by a small ZX platform layer;
- `curses_stub.c` keeps an 80x24 logical screen in banked RAM and renders a
  horizontally tracked 64-column viewport in a 4x8 font, two glyphs per
  character cell;
- refreshes redraw only the changed columns of the changed rows, so an
  ordinary step costs a few cells rather than a row; the viewport remains
  stable until the player moves outside its visible 64-column range.

The generated TAP boots to Rogue's first level, draws the dungeon and status
line, accepts keyboard input and executes turns.  This is still a bring-up
milestone rather than a complete port: save/score files are disabled, 64 of
the 80 columns are visible at a time, and the remaining cross-bank pointer
lifetimes and less common gameplay paths still need auditing.

Descending with `>` has been checked in ZEsarUX: level 2 is generated and the
command loop continues.  Maze direction data is kept in banked read-only data,
so it is no longer lost while another RAM page is visible during startup.

The first complete link and its fixed-memory blocker are recorded in
[`MEMORY.md`](MEMORY.md).

## Patch-stack policy

This port remains a patch stack over the original source tree.  ZX-only source
changes are kept small and guarded by `ZX128`; the platform implementation,
build files and tools live in this directory.  The normal Unix build is kept as
a regression target.  There is no imported or rewritten copy of Rogue hidden
under `zx128/`.

The apparent prototype changes in `rogue.h` add bank-call suffixes which expand
to nothing in a normal Unix build.  The `daemon_cb` typedef only makes the
existing daemon callback contract explicit; fixed-memory thunks adapt those
callbacks for the ZX bank switcher.

Once the logical commits are in place, a standalone patch series can be made
without carrying this fork's Git history:

```sh
git format-patch 546829b745ab8b1bca39b14f8bac5defd6c13fac..codex/zx128-port
```

Those patches can be replayed on a clean checkout with `git am`:

```sh
git checkout 546829b745ab8b1bca39b14f8bac5defd6c13fac
git am /path/to/patches/*.patch
```

The same commits can instead be rebased onto a newer `upstream/modern-rogue`
and reviewed as ordinary source patches.

Build with:

```sh
make -C zx128
make -C zx128 layout
```

Builds use all available logical CPUs by default. Override the automatic job
count with `JOBS=8`, or pass an explicit GNU Make limit such as `-j4`.

Override `Z88DK=/path/to/z88dk` when the checkout is elsewhere.  Relative
`Z88DK` values are resolved from the repository root rather than from the
`zx128` directory entered by `make -C`, so a sibling checkout works as expected:

```sh
make -C zx128 run-zesarux Z88DK=../z88dk
```

Launch the generated tape as a 128K Spectrum in ZEsarUX with either target:

```sh
make -C zx128 run-zesarux-zx128
make -C zx128 run-zesarux
```

The second name is an alias for the first.  The Makefile looks for `zesarux` on
`PATH`, then for the standard macOS application bundle.  Override the executable
or add emulator options when needed:

```sh
make -C zx128 run-zesarux ZESARUX=/path/to/zesarux
make -C zx128 run-zesarux ZESARUX_FLAGS="--zoom 2"
```

Run the automated real-emulator smoke test with:

```sh
make -C zx128 smoke-zesarux Z88DK=../z88dk
```

It boots the TAP headlessly, waits for Rogue's command loop, verifies the BASIC
loader and rendered screen through ZEsarUX's remote protocol, checks that the
ROM keyboard state is intact, sends an emulated `L` keyboard-matrix event, and
verifies that the hero actually moves right.  It then sends `.` and checks that
exactly one turn completes.  It also verifies that an ordinary turn redraws
fewer than 24 rows.  Finally, it advances the wandering-monster timer, checks
that the new monster has valid fixed-memory coordinates rather than a banked
scratch pointer, and writes a captured Spectrum screen to
`zx128/build/rogue-zx128-smoke.pbm`.
