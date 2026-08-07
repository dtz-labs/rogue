# Rogue 5.4.4 for ZX Spectrum 128K

[![ZX128 build and smoke](https://github.com/dtz-labs/rogue/actions/workflows/zx128.yml/badge.svg?branch=codex%2Fzx128-port)](https://github.com/dtz-labs/rogue/actions/workflows/zx128.yml)
[![License](https://img.shields.io/badge/license-BSD-blue.svg)](LICENSE.TXT)

## [▶ Play Rogue ZX128 in your browser](https://dtz-labs.github.io/rogue/)

This branch ports the classic Rogue 5.4.4 dungeon crawler to the ZX Spectrum
128K. The browser player always receives the TAP that passed the bank-layout
checks and the real-emulator smoke test in CI. Click the player's **▶** button,
press **SPACE**, enter your hero's name, and press **Enter**.

The generated TAP can also be downloaded directly from
[dtz-labs.github.io/rogue/rogue-zx128.tap](https://dtz-labs.github.io/rogue/rogue-zx128.tap)
and run on a Spectrum 128K or a compatible emulator.

## The port

The original 80x24 dungeon is retained in banked RAM and displayed through a
32-column Spectrum viewport. Code, mutable data, the dungeon map, inventory,
and renderer are split across the 128K machine's pageable RAM banks. The port
uses the Spectrum ROM keyboard and font, and produces a standard multi-block
TAP with a BASIC loader.

Current highlights include:

- procedural rooms, passages, mazes, monsters, combat, objects, and dungeon
  progression;
- an eight-column stepped viewport which avoids unnecessary redraws inside
  rooms;
- full-screen inventory, help, option, death, victory, and restart flows;
- a cold in-program restart after death, victory, or quit;
- pinned z88dk builds, linker-map guards, and an automated ZEsarUX gameplay
  smoke test in GitHub Actions.

Save files, persistent score files, shell escape, and Unix account integration
are intentionally unavailable on the Spectrum build.

## Controls

The startup screen contains an on-machine reference. The essential keys are:

- `h j k l`, `y u b n` — move in eight directions;
- `SHIFT` + direction — run;
- `i` — inventory, `,` — pick up, `d` — drop;
- `q` — quaff, `r` — read, `e` — eat;
- `w` — wield, `W` — wear, `T` — take armour off;
- `>` / `<` — stairs, `.` — rest, `s` — search;
- `?` — complete command help, `o` — options, `Q` — quit.

## Build and test

A z88dk checkout or installation is required. Build the TAP and verify the
fixed and banked memory layout with:

```sh
make -C zx128
make -C zx128 layout
```

If z88dk is in a sibling checkout:

```sh
make -C zx128 Z88DK=../z88dk
```

Run it locally in ZEsarUX:

```sh
make -C zx128 run-zesarux-zx128
```

Run the automated emulator smoke test:

```sh
make -C zx128 smoke-zesarux Z88DK=../z88dk
```

See [zx128/README.md](zx128/README.md) for the bank layout, build overrides,
testing details, and patch-stack policy.

## Upstream relationship

This is a deliberately small ZX-only patch stack over the `modern-rogue`
baseline at commit `546829b745ab8b1bca39b14f8bac5defd6c13fac`.

The Unix build documentation from that exact baseline remains available in the
[upstream README pinned to commit 546829b](https://github.com/Davidslv/rogue/blob/546829b745ab8b1bca39b14f8bac5defd6c13fac/README.md).
The complete pinned source tree is available
[upstream](https://github.com/Davidslv/rogue/tree/546829b745ab8b1bca39b14f8bac5defd6c13fac).

ZX-specific implementation and tooling live under `zx128/`; shared source
changes are guarded by `ZX128` wherever practical so the normal Unix target
remains a regression target.

## Browser emulator

The web player embeds
[JSSpeccy 3.2](https://github.com/gasman/jsspeccy3/releases/tag/v3.2),
licensed separately under GPL-3.0. The emulator, its licence and corresponding
source link are packaged alongside the BSD-licensed Rogue TAP during the Pages
deployment; emulator binaries are not vendored in this repository.

## License and credits

Rogue was written by Michael Toy, Ken Arnold, and Glenn Wichman. This source and
the generated TAP are distributed under the BSD-style terms in
[LICENSE.TXT](LICENSE.TXT).

[☕ Support dtz-labs and new ZX Spectrum software](https://buymeacoffee.com/mpasternak)
