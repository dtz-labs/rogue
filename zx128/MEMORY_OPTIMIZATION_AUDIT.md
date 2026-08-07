# ZX128 memory optimisation audit

Date: 2026-08-07

This document records a read-only audit of the resident memory and TAP image of
the ZX Spectrum 128K port.  The measurements below came from the coherent build
artifact timestamped around 20:40 on 2026-08-07.  Another change was moving code
between banks at the time, so the baseline must be measured again before any
optimisation is implemented.

No build was run and no source file was changed as part of the audit itself.

## Snapshot baseline

| Area | Used | Capacity | Free |
| --- | ---: | ---: | ---: |
| Fixed image | 17,762 B | 18,111 B | 349 B |
| Bank 0 | 15,942 B | 16,384 B | 442 B |
| Bank 1 | 16,035 B | 16,384 B | 349 B |
| Bank 3 | 15,573 B | 16,384 B | 811 B |
| Bank 4 | 14,503 B | 16,384 B | 1,881 B |
| Bank 6 | 15,504 B | 16,384 B | 880 B |
| Bank 7 | 15,023 B | 16,384 B | 1,361 B |

The fixed limit includes the configured 3 KiB C stack and minimum 3 KiB heap
reserve.  The executable fixed code was about 10,522 B: approximately 6,573 B
from the project, including generated initialisers, and 3,949 B from z88dk/CRT.

The current `+zx -create-app` output contains the BSS tails as zero bytes.  The
fixed binary plus six bank binaries totalled 110,342 B and the TAP 110,446 B.
Consequently, deleting a byte of resident CODE, RODATA, DATA or BSS normally
also removes approximately one byte from this uncompressed TAP.  Merely moving
DATA to BSS does not recover resident RAM and should not be counted as a memory
optimisation without a new measurement.

Whole-bank ZX0/ZX7-style compression followed by decompression into the same
layout can reduce the TAP and loading time, but it recovers no resident RAM.
Data must remain compressed or tokenised during gameplay to increase runtime
headroom.

## Correctness issue: banked `code_crt_init`

SDCC emitted 690 B of startup initialisers into fixed `code_crt_init`:

| Object | Initialiser size | Destination |
| --- | ---: | --- |
| `command.o` | 297 B | bank 0 |
| `rings.o` | 67 B | bank 0 |
| `things.o` | 8 B | bank 1 |
| `misc.o` | 57 B | bank 3 |
| `wizard.o` | 128 B | bank 3 |
| `scrolls.o` | 48 B | bank 4 |
| `io.o` | 85 B | fixed |

Startup runs these routines while bank 0 is paged at `0xc000`.  The bank 1, 3
and 4 initialisers therefore write into bank 0 rather than their declared BSS
or DATA.  This is both a latent corruption bug and a fixed-memory cost.

The target should be zero `code_crt_init` in every banked object.  Constant
function-local tables should become file-scope `static const` data in the owning
bank, zero initial values should use BSS, and exceptional values such as
`maxlen = -1` should be initialised lazily by their owner.  This can recover up
to 605 B of fixed memory from banked modules, or all 690 B after the fixed
`io.o` initialisers are simplified.

After the change, verify with `z88dk-z80nm -a` that banked objects contain no
`code_crt_init` section.

## Highest-value low-risk storage changes

### Remove the unused maze accounting matrix

`rooms.c` declares:

```text
SPOT maze[(NUMLINES / 3) + 1][(NUMCOLS / 3) + 1]
```

On this target, `SPOT` is 20 B and the matrix is 9 x 27 elements, for exactly
4,860 B of bank 7 BSS.  `used` and `nexits` are only cleared, `nexits` is never
incremented, and `exits` is written by `accnt_maze()` but never read.  Removing
the type, matrix, clear loop, two calls and `accnt_maze()` does not change RNG
consumption or maze geometry.

Expected gain: exactly 4,860 B of resident BSS plus the removed code.

Required regression: generate and traverse actual maze rooms on sufficiently
deep levels, rather than testing only level 1.

### Store only the 24 playable map rows

The four map chunks currently store `80 * MAXLINES` cells with
`MAXLINES == 32`.  A `PLACE` is 4 B, so the map consumes 10,240 B despite the
game using `NUMLINES == 24`.

Changing the ZX-only stride to 24 reduces it to 7,680 B, an exact saving of
2,560 B:

| Bank chunk | Saving |
| --- | ---: |
| Bank 0 | 800 B |
| Bank 1 | 608 B |
| Bank 3 | 672 B |
| Bank 4 | 480 B |

The array sizes, clear counts and `place_index()` must change together.  The
current `x << 5` index cannot be retained with smaller arrays.

Together, the dead maze matrix and the 24-row map recover exactly 7,420 B of
persistent bank storage before counting removed code.

## Fixed-memory library reductions

### Remove the unused generic console and font

The game uses its own renderer and the Spectrum ROM font at `0x3d00`, but two
invalid-input paths in `options.c` call `putchar(BEL)`.  Those two calls pull in
the FILE frontend, generic console, control-code parser and a 768 B 4x8 font.

The gross linked footprint of this stdout path is approximately 1.8 KiB:

- `fputc_callee`: about 141 B;
- generic console driver: about 356 B;
- full `fputc_cons_generic`: about 335 B;
- console tables, state and startup: about 180 B;
- `CRT_FONT_64`: exactly 768 B.

Replace the two calls with a ZX beep routine or a deliberate no-op.  Then
disable unused stdio setup and provide a tiny console sink, because merely
setting `CRT_ENABLE_STDIO=0` does not by itself suppress z88dk's default generic
`fputc_cons` alias.

`CRT_ENABLE_STDIO=0` separately removes exactly 100 B of FILE BSS and about
15 B of startup code.  Do not remove `fgetc_cons`; keyboard input still uses it.

Expected combined target: about 1.6-1.9 KiB of fixed RAM and TAP.  Confirm the
absence of `CRT_FONT_64`, `generic_console_*`, `fputc_cons_generic` and
`__sgoioblk` in the map.

### Restrict the printf implementation

The default z88dk mask links handlers for integer formats that the ZX build does
not use, including unsigned, hexadecimal, octal, pointer, `%n`, binary and
64-bit paths.  The linked source uses only:

- `%d` and one `%ld`;
- `%s` and `%c`;
- `%%`;
- width and flags such as `%*d`, `%-5d`, `%2d` and `%3d`.

A candidate linker configuration is:

```make
-pragma-define:CLIB_OPT_PRINTF=0x40001601
-pragma-define:CLIB_OPT_PRINTF_2=0
```

Bit 30 must remain enabled for width, `*` and `-`.  The expected gain is roughly
350-475 B fixed, but this must be measured after linking.

Regression coverage must include positive and negative `%d`/`%ld`, `%s`, `%c`,
`%%`, fixed width, left alignment and dynamic `*` width.  The stale, unlinked
`build/fixed/miniformat.o` is larger than the present formatter and should not
be restored without a new comparison.

### Remove the unused exit stack

No linked application code registers an `atexit()` callback.  Setting:

```make
-pragma-define:CLIB_EXIT_STACK_SIZE=0
```

recovers exactly 64 B.  The defensive `exit(1)` path should still be exercised
in a smoke test.

## Desktop remnants and right-sized buffers

Do not globally reduce `MAXSTR`; split each buffer according to its actual
contract.

| Candidate | Current | Proposed | Saving | Location | Notes |
| --- | ---: | ---: | ---: | --- | --- |
| `huh` | 160 B | 56 B | 104 B | fixed | ZX message buffer is 56 B |
| `fruit` | 160 B | 51 B | 109 B | fixed | option input is limited to 50 chars |
| `prbuf` | 320 B | 160 B | 160 B | fixed | test longest labels and inventory names |
| `get_str` scratch | 160 B | 51 B | 109 B | bank 1 | input limit is 50 chars |
| `inv_temp` | 160 B | 8 B | 152 B | bank 1 | holds only `%s` or `%c) %%s` |
| two combat name buffers | 320 B | 40 B | 280 B | bank 6 | use caller-owned fixed-stack buffer across banks |
| `whoami` | 160 B | 51 B or none | 109-160 B | bank 1 | currently not displayed by ZX death/win code |
| `file_name` | 160 B | none | 160 B | bank 1 | saving is unavailable |
| `home` | 160 B | none | 160 B | bank 1 | only supports desktop `~` expansion |

The ZX platform never calls the desktop `parse_opts()` parser, but the entire
function remains in `options.o`.  Compiling it out for ZX saves approximately
827 B of bank 1 code.  Remove the save-file option, `~` expansion and ineffective
ZX tombstone option at the same time.

`vers.c` contributes 108 B of fixed writable data for `release`, `encstr`,
`statlist` and `version`.  The ZX build only reads `release`.  A fixed
`const char release[] = "5.4.4"` needs 6 B, for an exact 102 B fixed saving.

Moving `io.c`'s 160 B `doadd` scratch buffer to the stack would recover 160 B
fixed, but adds a 160 B stack peak around `vsnprintf` and banked message calls.
Do this only after adding a stack high-water measurement.

## Text and help compression

The actually linked C string records occupy approximately:

| Area | Literal bytes |
| --- | ---: |
| Fixed | 1,537 B |
| Bank 0 | 3,467 B |
| Bank 1 | 1,490 B |
| Bank 3 | 1,988 B |
| Bank 4 | 1,988 B |
| Bank 6 | 1,220 B |
| Bank 7 | 48 B |
| **Total** | **11,738 B** |

Pageable RODATA is almost entirely text.  A byte-pair token simulation on the
actual records produced these gross data-model savings before decoder and call
site costs:

| Dictionary size | Gross saving |
| ---: | ---: |
| 16 tokens | 1,966 B |
| 32 tokens | 2,779 B |
| 64 tokens | 3,537 B |

The help table is a safer first target.  It consists of 66 four-byte records,
so its pointer table alone is 264 B.  A sequential format containing one key
byte with the print flag in its high bit followed by a NUL-terminated
description reduces this to 66 B, an exact gross saving of 198 B before reader
code.  Tokenising the repeated direction, `run` and `until adjacent` fragments
has a further gross ceiling around 317 B.

For general text compression:

- keep dictionaries local to a bank or make the decoder explicitly page-safe;
- never pass a raw pointer to compressed pageable data through a bank switch;
- decode before calling ordinary `waddstr`, because token bytes above ASCII 127
  are rendered as `?`;
- initially leave formatted strings uncompressed, or make `%` arguments part
  of a deliberate opcode format;
- prefer a small fixed streaming decoder that reads the caller's currently
  paged bank.

A whole-bank compression loader is a TAP optimisation, not a resident-memory
solution.  Tape is sequential and unsuitable as general swap.  Disk overlays
would require a separate target and backend and are not justified for the seed
screen or restart code.

## Medium-sized structural candidates

These changes are useful after the low-risk work but need broader behavioural
tests.

### Passages and rooms

- Replace `passages.c`'s 171 B `rdes` connectivity scratch with bitmasks of
  about 20 B: exact saving approximately 151 B.
- `rooms[9]` and `passages[13]` consume 1,452 B fixed and are addressed by raw
  pointers.  They cannot simply be paged or overlaid.
- Making `r_flags` and `r_nexits` bytes saves 44 B.
- Packing only the twelve exit coordinates saves 528 B; together with byte
  flags/counts, 572 B fixed.
- Converting all ZX coordinates to explicit signed bytes can save about 704 B
  in room storage plus 2 B per `THING`, but changes the cross-bank ABI and is a
  later refactor.

### Map representation

After the 24-row stride, a private three-byte map cell containing character,
flags and an 8-bit monster ID plus a fixed pointer table could save another
1,792 B.  It needs strict stale-ID and exhaustion handling; do not assume 64
monster slots without a runtime assertion.

### Monsters, damage and heap objects

- `monster[26]` is 858 B fixed.  Making only `m_carry` a byte saves 26 B.
- A compact 15 B monster prototype expanded at spawn time would save 468 B.
- Encoding damage dice instead of keeping ASCII strings can remove repeated
  strings and eliminate runtime `atoi`/`strchr` parsing.
- Removing save-only `_t_reserved` changes `THING` from 49 B to 47 B.
- Compact coordinates and both monster/weapon damage representations can
  eventually reduce `THING` from 49 B to about 36 B, saving 13 B for every live
  object or monster.
- The 18 generated scroll names can consume up to 738 B plus allocator headers.
  Storing generated syllable IDs in at most 16 B per name uses 288 B, removes
  separate allocations and reduces heap fragmentation.
- `discard()` currently does not free an object's allocated label, and
  `new_item()` can dereference NULL after allocation failure.  Both become more
  important with a 3 KiB heap.

### Other small data reductions

- `fight`'s two 32-entry integer tables to signed bytes: 64 B.
- `rings` usage table to bytes: 14 B.
- armor class table to bytes: 8 B.
- `obj_info.oi_prob` to a byte for 85 records: 85 B.
- `pack_used` to a bitset: 22 B.
- initialisation scratch arrays to bitsets: up to 47 B net.
- wand drain victim list replaced by two safe list passes: 80 B and removal of
  its current 40-entry overflow edge case.

The fixed `p_colors`, `r_stones`, `ws_made` and `ws_type` pointer arrays consume
112 B and currently hold raw pointers into bank 3 RODATA.  That is a pointer
lifetime bug across bank switches.  Byte IDs plus a material-type bitset need
about 44 B, save about 68 B fixed and permit a copy-to-fixed accessor.

## Screen buffer

The 24 x 80 logical character screen occupies 1,920 B.  It is used by
`inch`/`mvinch`, visibility, menus and viewport panning; it is not equivalent to
the physical bitmap and cannot simply be removed or overlaid with the map.

- Ragged message/status rows save only 64 B.
- Seven-bit packing saves about 240 B but taxes every access.
- A 24 x 32 visible-only buffer saves 1,152 B but requires regeneration of
  off-screen state during panning and changes renderer semantics.

Treat screen compression as a final-stage renderer refactor, not an immediate
source of seed/restart space.

## Effect on seed selection and restart

The low-risk fixed-memory work alone should recover roughly 2.7-3.1 KiB before
right-sizing buffers.  The dead maze and 24-row map add exactly 7,420 B of bank
storage.  This is ample space for a banked seed-selection screen without tape
or disk overlays.

The same savings make a mini-CRT restart more practical.  BSS can be cleared;
only pristine initial DATA needs an immutable restore template.  A build-time
compressed template stored in newly freed bank space is preferable to runtime
disk swap or re-reading a sequential TAP.  Its exact size must be recomputed
after removing desktop DATA, generated initialisers and pointer tables.

## Recommended implementation order

1. Wait for the concurrent bank-layout work to finish and save a fresh map,
   binary and TAP baseline.
2. Eliminate banked `code_crt_init` and verify its complete absence.
3. Remove the dead maze matrix and test real deep-level maze rooms.
4. Change the ZX map stride from 32 to 24 and test every playable row.
5. Remove the two `putchar` dependencies, unused stdio, generic console and
   bundled font.
6. Restrict the z88dk printf mask and run explicit formatter regressions.
7. Remove desktop options/save/version remnants and right-size obvious buffers.
8. Apply small bitset and narrow-integer changes.
9. Fix cross-bank name/material pointers and scroll-name heap usage.
10. Only then consider tokenised text, compact map IDs, damage/monster formats,
    global coordinate changes or a visible-only screen.

For every independent patch:

- preserve the previous `.map`, `.bin` and `.tap` measurements;
- run `make -C zx128 layout`;
- run the full ZEsarUX smoke suite;
- add a targeted regression for the feature being changed;
- compare fixed tail, every bank tail, TAP size, heap use and stack high-water;
- assert the expected obsolete symbols are absent from the map.

