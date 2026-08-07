# First ZX 128K memory report

This is the result of the first complete SDCC `SO2` link, made with z88dk
`zcc v23854-4d530b6eb7-20251002`.  It is a sizing baseline, not yet a playable
image: the linker can emit the files, but the layout checker correctly rejects
the fixed image.

## Fixed memory

The current fixed window starts at `0x6000` and contains 24,576 raw bytes before
the paging boundary.  Reserving the configured 3 KiB stack and a minimum 3 KiB
heap leaves an 18,432-byte static-image budget ending at `0xa800`.

| Section | Bytes |
| --- | ---: |
| CODE | 10,288 |
| RODATA | 3,758 |
| DATA | 4,689 |
| BSS | 15,681 |
| **Fixed image** | **34,664 / 24,576** |

The fixed image ends at `0xe768`, so it exceeds the paging boundary by 10,088
bytes and the runnable static-image budget by 16,232 bytes.

The largest single allocation explains most of the result:

- `places[MAXLINES * MAXCOLS]`: 32 * 80 * 4 = 10,240 bytes;
- the temporary mini-curses cell screen: 24 * 80 = 1,920 bytes.

`places` is shared game state and is touched directly through the `chat`,
`flat`, `moat` and `INDEX` macros by many original modules.  It therefore
cannot simply be assigned to a pageable bank while those modules execute from
other banks.  That would require a real far-data/accessor design, not a linker
flag disguised as an optimisation.

## Pageable banks

All current bank images fit their 16 KiB windows:

| Bank | Bytes | Free |
| ---: | ---: | ---: |
| 0 | 11,515 | 4,869 |
| 1 | 12,436 | 3,948 |
| 3 | 9,996 | 6,388 |
| 4 | 13,262 | 3,122 |
| 6 | 13,773 | 2,611 |
| 7 | 14,065 | 2,319 |

Banks 2 and 5 are deliberately not used as paging targets because they form
the Spectrum's fixed 32 KiB address space.

## What this rules out

Shrinking ordinary code by a few percent will not solve the present overflow.
The first architectural choice must address shared storage.  Candidates for a
later measured patch include:

1. a paged representation or explicit access layer for `places`;
2. using otherwise hidden display RAM, which also commits the loader and video
   architecture;
3. reducing `MAXLINES` from the compatibility value 32 to the 24 rows actually
   displayed (2,560 bytes), which is insufficient on its own;
4. moving or eliminating the 1,920-byte temporary screen buffer, also
   insufficient on its own;
5. moving more fixed helpers into banks, after auditing every string and
   function pointer that crosses a bank switch.

No one of the small reductions should be presented as the solution before the
shared-map decision is made.

## Recommended next memory patch

Keep the SDCC frontend for the first runnable version.  Its `__banked` support
works for calls, but it does not provide a safe far-data declaration: `__far`
and `extern __banked` data are rejected, while a named address space switches
the page without restoring the caller's bank.  Switching the whole port to
the sccz80 frontend is not a small workaround; the current sources then need
separate fixes around static `sizeof` expressions and varargs.

The smallest explicit SDCC design is therefore to free bank 3 and make it a
data-only page accessed by fixed-memory functions:

```text
0xc000..0xe7ff  places[80 * 32]        10,240 bytes
0xe800..0xef7f  screen_cells[80 * 24]   1,920 bytes
0xef80..0xffff  reserve                  4,224 bytes
```

The current bank-3 modules can be repacked into the available space in banks
0, 1, 4 and 6.  Moving `daemon.c`, `list.c` and their shared state to bank 7 is
also feasible, but every such move must be accompanied by another cross-bank
call and pointer-lifetime audit.

The fixed accessors must save the complete current `BANKM` value, page bank 3,
copy a value, and restore the old value.  They must never return a pointer into
`0xc000..0xffff`.  On the game side this means replacing writable uses of the
`chat`, `flat`, `moat` and `INDEX` macros with explicit get/set operations; the
normal Unix definitions remain unchanged under `#ifndef ZX128`.

Merely moving `places` is still not enough for a runnable target.  With a 3 KiB
machine stack and at least 3 KiB of heap, a useful first gate is:

```text
__BSS_END_tail <= 0xa800
```

That makes the effective fixed-image target 18,432 bytes, so the present
34,664-byte image ultimately needs to lose at least 16,232 fixed bytes.  A 4
KiB heap (`__BSS_END_tail <= 0xa400`) would be safer: the current SDCC layout
reports `sizeof(THING) == 49`, before allocator overhead.

## Runtime blockers separate from size

The link proves section placement, not correct execution.  Before the TAP can
be called playable, the next patches must also address:

- `get_item()` calls whose purpose string originates in another bank;
- the bank-6 to bank-4 `fire_bolt(..., "flame")` string;
- long-lived color, stone and material pointers selected by `init.c` from
  bank-local read-only data;
- raw function pointers passed for `ring_num` and `charge_str`;
- functions such as `vowelstr`, `pick_color`, `set_mname` and `num` which can
  return a pointer into bank-local data;
- worst-case stack use of the recursive maze generator in `rooms.c`.

These need fixed thunks or explicit copies.  A bank number cannot be encoded in
an ordinary 16-bit C pointer, and paging back invalidates a returned pointer.
