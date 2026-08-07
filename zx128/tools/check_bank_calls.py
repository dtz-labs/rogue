#!/usr/bin/env python3
"""Reject cross-bank calls that bypass the __banked paging trampoline.

Every function compiled into a 16K bank is linked at the same 0xC000 window,
so a plain CALL to its address only lands on the intended code while that bank
happens to be paged in.  ZX_BANKED_n makes the declaration __banked for callers
outside bank n, which routes the call through a trampoline that pages first.

The annotation deliberately collapses to nothing inside the owning bank, so a
missing ZX_BANKED_n is a defect only when some caller lives elsewhere.  This
check pairs the linker map (where the code actually ended up) with the headers
(what callers were told) and fails when the two disagree.

Left unchecked the failure is silent and delayed: the CALL runs whatever code
occupies that address in the currently paged bank, usually mid-function, so the
stack frame is wrong and the crash surfaces much later somewhere unrelated.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

MAP_SYMBOL_RE = re.compile(
    r"^(\S+)\s*=\s*\$([0-9A-Fa-f]+)\s*;\s*addr,\s*public,\s*,\s*(\S*),\s*(\S*),"
)
DECL_RE = re.compile(
    r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{]*\)\s*(ZX_BANKED_(\d)|ZX_FIXED)?\s*;"
)
CALL_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
STRING_RE = re.compile(r'"(?:[^"\\]|\\.)*"')
NOT_A_CALL = {
    "if", "while", "for", "switch", "return", "sizeof", "defined", "case",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--map", required=True, type=Path, dest="map_path")
    parser.add_argument("--root", required=True, type=Path,
                        help="repository root holding the C sources and headers")
    return parser.parse_args()


def source_banks(makefile: Path) -> dict[str, str]:
    """Map each compiled translation unit stem to the bank it is built into."""
    text = makefile.read_text()
    banks: dict[str, str] = {}
    for match in re.finditer(r"^BANK(\d+)_NAMES\s*:=\s*(.*)$", text, re.M):
        for stem in match.group(2).split():
            banks[stem] = match.group(1)
    for match in re.finditer(r"^FIXED(?:_LOCAL)?_NAMES\s*:=\s*(.*)$", text, re.M):
        for stem in match.group(1).split():
            banks[stem] = "FIXED"
    return banks


def code_banks(map_path: Path) -> dict[str, str]:
    """Map each public function symbol to the bank its code was linked into."""
    banks: dict[str, str] = {}
    for line in map_path.read_text(errors="replace").splitlines():
        match = MAP_SYMBOL_RE.match(line)
        if match and match.group(4).startswith("CODE_"):
            banks[match.group(1).lstrip("_")] = match.group(4).split("_", 1)[1]
    return banks


def declared_banks(headers: list[Path]) -> dict[str, str | None]:
    """Map each declared function to its annotation, or None when it has none."""
    declarations: dict[str, str | None] = {}
    for header in headers:
        for line in header.read_text(errors="replace").splitlines():
            stripped = line.strip()
            if (stripped.startswith(("#", "/*", "*"))
                    or "(" not in stripped or not stripped.endswith(";")):
                continue
            match = DECL_RE.search(stripped)
            if not match:
                continue
            annotation, bank = match.group(2), match.group(3)
            declarations[match.group(1)] = bank or ("FIXED" if annotation else None)
    return declarations


def call_sites(root: Path, banks: dict[str, str]) -> dict[str, set[str]]:
    """Map each called name to the `stem:bank` of every unit calling it."""
    sources = list(root.glob("*.c")) + list((root / "zx128").glob("*.c"))
    callers: dict[str, set[str]] = {}
    for source in sources:
        bank = banks.get(source.stem)
        if bank is None:
            continue  # not part of the ZX build
        text = source.read_text(errors="replace")
        text = STRING_RE.sub('""', COMMENT_RE.sub(" ", text))
        for match in CALL_RE.finditer(text):
            name = match.group(1)
            if name not in NOT_A_CALL:
                callers.setdefault(name, set()).add(f"{source.stem}:{bank}")
    return callers


def main() -> int:
    args = parse_args()
    root = args.root
    banks = source_banks(root / "zx128" / "Makefile")
    linked = code_banks(args.map_path)
    declared = declared_banks([root / "rogue.h", root / "extern.h"])
    callers = call_sites(root, banks)

    problems: list[tuple[str, str, list[str]]] = []
    for name, home in sorted(linked.items()):
        if declared.get(name) is not None:
            continue  # annotated, or deliberately fixed
        if name not in declared:
            continue  # static or locally declared; not reachable cross-bank
        outside = sorted(c for c in callers.get(name, set())
                         if c.rsplit(":", 1)[1] != home)
        if outside:
            problems.append((name, home, outside))

    mismatched = sorted(
        (name, home, declared[name])
        for name, home in linked.items()
        if declared.get(name) not in (None, "FIXED") and declared.get(name) != home
    )

    for name, home, wrong in mismatched:
        print(f"error: {name}() is in bank {home} but declared ZX_BANKED_{wrong}",
              file=sys.stderr)
    for name, home, outside in problems:
        print(f"error: {name}() is in bank {home} and needs ZX_BANKED_{home}; "
              f"called from {', '.join(outside)}", file=sys.stderr)

    if mismatched or problems:
        print(f"\n{len(mismatched) + len(problems)} cross-bank call(s) would "
              f"bypass the paging trampoline.", file=sys.stderr)
        return 1

    print(f"cross-bank calls: {len(linked)} banked functions checked, all "
          f"reachable callers page correctly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
