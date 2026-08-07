#!/usr/bin/env python3
"""Validate ZX banked object invariants that the linker map cannot express."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


SECTION_RE = re.compile(r"^\s*Section (\S+): (\d+) bytes$", re.MULTILINE)
EXPECTED_STORAGE_BSS = {
    0: 25 * 24 * 4,
    1: 19 * 24 * 4,
    3: (21 * 24 * 4) + (24 * 80),
    4: 15 * 24 * 4,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nm", default="z88dk-z80nm")
    parser.add_argument("--build-dir", required=True, type=Path)
    return parser.parse_args()


def inspect_object(nm: str, path: Path) -> tuple[str, dict[str, int]]:
    result = subprocess.run(
        [nm, "-a", str(path)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"{nm} failed for {path} with status {result.returncode}: "
            f"{result.stderr.strip()}"
        )
    return result.stdout, {
        name: int(size) for name, size in SECTION_RE.findall(result.stdout)
    }


def main() -> int:
    args = parse_args()
    errors: list[str] = []
    objects: list[Path] = []

    for bank in (0, 1, 3, 4, 6, 7):
        bank_dir = args.build_dir / f"bank{bank}"
        objects.extend(sorted(bank_dir.glob("*.o")))

    if not objects:
        errors.append(f"no banked objects found below {args.build_dir}")

    inspected: dict[Path, tuple[str, dict[str, int]]] = {}
    for path in objects:
        try:
            inspected[path] = inspect_object(args.nm, path)
        except RuntimeError as exc:
            errors.append(str(exc))
            continue
        sections = inspected[path][1]
        init_size = sections.get("code_crt_init", 0)
        if init_size:
            errors.append(f"{path}: code_crt_init is {init_size} bytes, expected 0")

    rooms = args.build_dir / "bank7" / "rooms.o"
    if rooms in inspected:
        output = inspected[rooms][0]
        for symbol in ("_maze", "_accnt_maze"):
            if re.search(rf"\b{re.escape(symbol)}\b", output):
                errors.append(f"{rooms}: dead symbol {symbol} is still present")

    for bank, expected in EXPECTED_STORAGE_BSS.items():
        storage = args.build_dir / f"bank{bank}" / "bank_storage.o"
        if storage not in inspected:
            errors.append(f"missing bank storage object: {storage}")
            continue
        actual = inspected[storage][1].get(f"BSS_{bank}")
        if actual != expected:
            errors.append(
                f"{storage}: BSS_{bank} is {actual!r} bytes, expected {expected}"
            )

    snapshot = args.build_dir / "bank7" / "screen_snapshot.o"
    if snapshot not in inspected:
        errors.append(f"missing screen snapshot object: {snapshot}")
    else:
        actual = inspected[snapshot][1].get("BSS_7")
        if actual != 24 * 80:
            errors.append(
                f"{snapshot}: BSS_7 is {actual!r} bytes, expected {24 * 80}"
            )

    if errors:
        for error in errors:
            print(f"banked-objects: ERROR: {error}", file=sys.stderr)
        return 1

    print(
        "banked-objects: OK "
        "(no banked CRT initializers, no maze matrix, 24-row map chunks)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
