#!/usr/bin/env python3
"""Validate the fixed image and 128K bank images emitted by z88dk."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


BANK_WINDOW_ORIGIN = 0xC000
BANK_SIZE = 0x4000
FORBIDDEN_BANKS = {2, 5}
RESTART_DATA_SIZES = {
    "fixed": 0x0B2B,
    0: 0x0108,
    1: 0x01AD,
    3: 0x026E,
    4: 0x00CD,
    6: 0x00A2,
    7: 0x00A0,
}
FORBIDDEN_PAGEABLE_SCRATCH_PREFIXES = (
    "_do_zap_bolt_",
    "_fire_bolt_pos_",
    "_hit_monster_mp_",
    "_read_scroll_mp_",
    "_teleport_c_",
    "_treas_room_mp_",
    "_wanderer_cp_",
)

SYMBOL_RE = re.compile(r"^(\S+)\s*=\s*\$([0-9A-Fa-f]+)\b")
BANK_FILE_RE_TEMPLATE = r"^{stem}_BANK_(\d+)\.bin$"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Check a z88dk +zx fixed image and its bank binaries."
    )
    parser.add_argument("--map", required=True, type=Path, dest="map_path")
    parser.add_argument("--main-bin", required=True, type=Path, dest="main_bin")
    parser.add_argument(
        "--stack-reserve",
        type=int,
        default=0,
        help="bytes reserved below the paging window for the machine stack",
    )
    parser.add_argument(
        "--heap-reserve",
        type=int,
        default=0,
        help="minimum free bytes reserved below the stack for the C heap",
    )
    return parser.parse_args()


def read_symbols(map_path: Path) -> dict[str, int]:
    symbols: dict[str, int] = {}
    for line in map_path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = SYMBOL_RE.match(line)
        if match:
            symbols[match.group(1)] = int(match.group(2), 16)
    return symbols


def bank_map_size(symbols: dict[str, int], bank: int) -> int | None:
    end = symbols.get(f"__BANK_{bank}_END_tail")
    if end is None:
        return None
    return end - ((bank << 16) + BANK_WINDOW_ORIGIN)


def bank_has_mapped_sections(symbols: dict[str, int], bank: int) -> bool:
    prefixes = ("CODE", "RODATA", "DATA", "BSS")
    return any(symbols.get(f"__{prefix}_{bank}_size", 0) > 0 for prefix in prefixes)


def fixed_section_size(symbols: dict[str, int], section: str) -> int | None:
    head = symbols.get(f"__{section}_head")
    tail = symbols.get(f"__{section}_END_tail")
    if head is None or tail is None:
        return None
    return tail - head


def main() -> int:
    args = parse_args()
    errors: list[str] = []

    if args.stack_reserve < 0:
        errors.append("stack reserve cannot be negative")
    if args.heap_reserve < 0:
        errors.append("heap reserve cannot be negative")

    if not args.map_path.is_file():
        errors.append(f"map file does not exist: {args.map_path}")
    if not args.main_bin.is_file():
        errors.append(f"main binary does not exist: {args.main_bin}")
    if errors:
        for error in errors:
            print(f"layout: ERROR: {error}", file=sys.stderr)
        return 1

    symbols = read_symbols(args.map_path)
    for name, address in symbols.items():
        if (
            name.startswith(FORBIDDEN_PAGEABLE_SCRATCH_PREFIXES)
            and (address & 0xFFFF) >= BANK_WINDOW_ORIGIN
        ):
            errors.append(
                f"cross-bank scratch {name} is static in the pageable window "
                f"at 0x{address & 0xFFFF:04X}"
            )
    stack_top = symbols.get("REGISTER_SP", BANK_WINDOW_ORIGIN)
    mapped_stack_reserve = symbols.get("__crt_stack_size")
    if (
        mapped_stack_reserve is not None
        and args.stack_reserve
        and mapped_stack_reserve != args.stack_reserve
    ):
        errors.append(
            "stack reserve differs between command line and map: "
            f"cli={args.stack_reserve}, map={mapped_stack_reserve}"
        )
    stack_reserve = (
        mapped_stack_reserve
        if mapped_stack_reserve is not None
        else args.stack_reserve
    )
    banking_stack_reserve = symbols.get("CLIB_BANKING_STACK_SIZE", 0)
    exit_stack_entries = symbols.get("__clib_exit_stack_size", 0)
    exit_stack_reserve = exit_stack_entries * 2
    fixed_limit = (
        stack_top
        - banking_stack_reserve
        - exit_stack_reserve
        - stack_reserve
        - args.heap_reserve
    )
    main_size = args.main_bin.stat().st_size
    if main_size == 0:
        errors.append(f"main binary is empty: {args.main_bin}")
    else:
        # zx_restart_game jumps to origin + 3: immediately after the CRT's
        # initial CALL that loads the pageable images from tape.  What the
        # restart entry depends on is the shape -- a three-byte CALL, then the
        # real CRT entry -- not the address being called.  That address is the
        # tape loader's, and it moves whenever the CRT gains or loses a byte of
        # startup code, so pinning it made every unrelated size change look
        # like a broken restart entry.
        crt_prefix = args.main_bin.read_bytes()[:7]
        call_target = int.from_bytes(crt_prefix[1:3], "little")
        if (
            crt_prefix[0] != 0xCD
            or not 0x6000 <= call_target < BANK_WINDOW_ORIGIN
            or crt_prefix[3:7] != bytes.fromhex("fd213a5c")
        ):
            errors.append(
                "CRT no longer starts with a CALL into the fixed image "
                "followed by LD IY,0x5c3a; "
                "the restart entry at CRT_ORG_CODE + 3 must be reviewed"
            )

    fixed_data_head = symbols.get("__data_clib_head")
    fixed_data_tail = symbols.get("__data_compiler_tail")
    if fixed_data_head is None or fixed_data_tail is None:
        errors.append("restart fixed-DATA boundary symbols are missing")
    elif fixed_data_tail - fixed_data_head != RESTART_DATA_SIZES["fixed"]:
        errors.append(
            "restart fixed-DATA snapshot size is stale: "
            f"map={fixed_data_tail - fixed_data_head}, "
            f"expected={RESTART_DATA_SIZES['fixed']}"
        )
    elif (
        symbols.get("__DATA_head") != fixed_data_head
        or symbols.get("__DATA_END_tail") != fixed_data_tail
    ):
        errors.append(
            "restart fixed-DATA snapshot no longer covers the complete "
            "fixed DATA image"
        )
    for bank, expected in RESTART_DATA_SIZES.items():
        if bank == "fixed":
            continue
        actual = symbols.get(f"__DATA_{bank}_size")
        if actual is None:
            errors.append(f"restart DATA_{bank} size symbol is missing")
        elif actual != expected:
            errors.append(
                f"restart DATA_{bank} snapshot size is stale: "
                f"map={actual}, expected={expected}"
            )

    unassigned = args.main_bin.with_name(
        f"{args.main_bin.stem}_UNASSIGNED.bin"
    )
    if unassigned.is_file() and unassigned.stat().st_size > 0:
        errors.append(
            f"non-empty unassigned image indicates sections outside the memory map: {unassigned}"
        )

    origin = symbols.get("CRT_ORG_CODE")
    if origin is None:
        errors.append("CRT_ORG_CODE is missing from the map")
        binary_tail = None
    else:
        if origin != 0x6000:
            errors.append(
                f"restart entry requires CRT_ORG_CODE=0x6000, got 0x{origin:04X}"
            )
        binary_tail = origin + main_size
        if fixed_limit <= origin:
            errors.append(
                "stack and heap reserves leave no fixed-image space: "
                f"origin=0x{origin:04X}, limit=0x{fixed_limit:04X}"
            )

    fixed_tail_symbols = (
        "__CODE_END_tail",
        "__RODATA_END_tail",
        "__DATA_END_tail",
        "__BSS_END_tail",
    )
    known_fixed_tails = [
        symbols[name] for name in fixed_tail_symbols if name in symbols
    ]
    if not known_fixed_tails:
        errors.append("no fixed-image tail symbol was found in the map")

    tail_candidates = list(known_fixed_tails)
    if binary_tail is not None:
        tail_candidates.append(binary_tail)
    fixed_tail = max(tail_candidates) if tail_candidates else None

    # Tail symbols and origin + file size are exclusive end addresses, so an
    # image ending exactly at the limit does not overlap the reserved area.
    if fixed_tail is not None and fixed_tail > fixed_limit:
        errors.append(
            f"fixed image tail 0x{fixed_tail:04X} exceeds static-data limit "
            f"0x{fixed_limit:04X} (stack reserve={stack_reserve}, "
            f"heap reserve={args.heap_reserve})"
        )

    bank_file_re = re.compile(
        BANK_FILE_RE_TEMPLATE.format(stem=re.escape(args.main_bin.stem))
    )
    bank_files: dict[int, Path] = {}
    for path in args.main_bin.parent.glob(f"{args.main_bin.stem}_BANK_*.bin"):
        match = bank_file_re.match(path.name)
        if not match:
            continue
        bank = int(match.group(1))
        if bank in bank_files:
            errors.append(f"duplicate binary for bank {bank}: {path}")
            continue
        bank_files[bank] = path

    for bank in range(8):
        mapped_size = bank_map_size(symbols, bank)
        mapped_sections = bank_has_mapped_sections(symbols, bank)
        path = bank_files.get(bank)
        file_size = path.stat().st_size if path is not None else None

        if mapped_size is not None and mapped_size < 0:
            errors.append(f"bank {bank} has a negative mapped size: {mapped_size}")
        if mapped_size is not None and mapped_size > BANK_SIZE:
            errors.append(
                f"bank {bank} map size {mapped_size} exceeds {BANK_SIZE} bytes"
            )
        if file_size is not None and file_size > BANK_SIZE:
            errors.append(
                f"bank {bank} binary size {file_size} exceeds {BANK_SIZE} bytes: {path}"
            )

        used = (
            (mapped_size is not None and mapped_size > 0)
            or mapped_sections
            or (file_size is not None and file_size > 0)
        )
        if used and bank in FORBIDDEN_BANKS:
            errors.append(
                f"bank {bank} is reserved by the fixed ZX 128K memory map and must not be used"
            )

        if mapped_size is not None and mapped_size > 0 and path is None:
            errors.append(f"bank {bank} is non-empty in the map but its binary is missing")
        if mapped_sections and mapped_size is None:
            errors.append(
                f"bank {bank} has mapped sections but __BANK_{bank}_END_tail is missing"
            )
        if path is not None and mapped_size is None:
            errors.append(f"bank {bank} binary exists but its map end symbol is missing")
        if (
            path is not None
            and mapped_size is not None
            and file_size != mapped_size
        ):
            errors.append(
                f"bank {bank} size differs: map={mapped_size}, binary={file_size}"
            )

    unexpected_banks = sorted(set(bank_files) - set(range(8)))
    for bank in unexpected_banks:
        errors.append(f"bank number outside the ZX 128K range: {bank}")

    if origin is not None and fixed_tail is not None:
        print(
            "layout: fixed "
            f"origin=0x{origin:04X} tail=0x{fixed_tail:04X} "
            f"limit=0x{fixed_limit:04X} size={main_size}/{fixed_limit - origin} "
            f"reserves(banking={banking_stack_reserve},"
            f"atexit={exit_stack_reserve},stack={stack_reserve},"
            f"heap={args.heap_reserve})"
        )
        section_sizes = {
            section: fixed_section_size(symbols, section)
            for section in ("CODE", "RODATA", "DATA", "BSS")
        }
        if all(size is not None for size in section_sizes.values()):
            print(
                "layout: fixed sections "
                + " ".join(
                    f"{section.lower()}={size}"
                    for section, size in section_sizes.items()
                )
            )
    for bank, path in sorted(bank_files.items()):
        print(f"layout: bank {bank} size={path.stat().st_size}/{BANK_SIZE}")

    if errors:
        for error in errors:
            print(f"layout: ERROR: {error}", file=sys.stderr)
        return 1

    print("layout: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
