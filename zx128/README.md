# Rogue 5.4.4 — ZX Spectrum 128K bring-up

This directory is the deliberately conservative first stage of a z88dk port.
It builds the original game modules into Spectrum 128K RAM banks and produces
a linker map before gameplay or renderer optimisation starts.

Current scope:

- upstream baseline: `modern-rogue` at commit
  `546829b745ab8b1bca39b14f8bac5defd6c13fac`;
- z88dk classic `+zx` CRT with the SDCC frontend;
- pageable code/data in banks 0, 1, 3, 4, 6 and 7;
- banks 2 and 5 remain the Spectrum's fixed RAM pages;
- save/restore, score files, shell escape, signals and Unix account handling
  are replaced by a small ZX platform layer;
- `curses_stub.c` keeps an 80x24 logical screen but has no pixel renderer yet.

The generated TAP is a memory-layout/bring-up artifact. It is not described as
playable until cross-bank calls, callbacks and bank-lifetime rules for pointers
have all been audited and a real renderer is connected.

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

Override `Z88DK=/path/to/z88dk` when the checkout is elsewhere.

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

Launching does not imply that the current sizing image is playable: `make
layout` still documents the fixed-memory and bank-lifetime blockers.
