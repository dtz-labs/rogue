#!/usr/bin/env python3
"""Boot the ZX128 TAP in ZEsarUX and verify one real Rogue turn."""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
import time
from pathlib import Path


PROMPT = b"command> "
SYMBOL_RE = re.compile(r"^(\S+)\s*=\s*\$([0-9A-Fa-f]+)\b", re.MULTILINE)
THING_POSITION_OFFSET = 4


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zesarux", required=True, type=Path)
    parser.add_argument("--tap", required=True, type=Path)
    parser.add_argument("--map", required=True, type=Path, dest="map_path")
    parser.add_argument("--screenshot", required=True, type=Path)
    parser.add_argument("--timeout", type=float, default=30.0)
    return parser.parse_args()


def symbols_from_map(path: Path) -> dict[str, int]:
    text = path.read_text(encoding="utf-8", errors="replace")
    return {name: int(value, 16) for name, value in SYMBOL_RE.findall(text)}


def required_symbol(symbols: dict[str, int], name: str) -> int:
    for candidate in (name, f"_{name}"):
        if candidate in symbols:
            return symbols[candidate]
    raise RuntimeError(f"symbol {name!r} is missing from the linker map")


def free_local_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def receive_prompt(sock: socket.socket, timeout: float = 3.0) -> str:
    deadline = time.monotonic() + timeout
    response = bytearray()
    while not response.endswith(PROMPT):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError(
                "timed out waiting for the ZEsarUX command prompt; "
                f"partial response: {response.decode('latin-1', 'replace')!r}"
            )
        sock.settimeout(remaining)
        chunk = sock.recv(4096)
        if not chunk:
            raise RuntimeError("ZEsarUX closed the remote protocol connection")
        response.extend(chunk)
    return response.decode("latin-1", "replace")


def command(sock: socket.socket, text: str, timeout: float = 3.0) -> str:
    sock.sendall((text + "\n").encode("latin-1"))
    return receive_prompt(sock, timeout)


def connect(proc: subprocess.Popen[bytes], port: int, timeout: float) -> socket.socket:
    deadline = time.monotonic() + timeout
    last_error: OSError | None = None
    while time.monotonic() < deadline:
        status = proc.poll()
        if status is not None:
            raise RuntimeError(f"ZEsarUX exited before ZRCP was ready (status {status})")
        try:
            sock = socket.create_connection(("127.0.0.1", port), timeout=0.5)
            receive_prompt(sock, min(3.0, max(0.5, deadline - time.monotonic())))
            return sock
        except OSError as exc:
            last_error = exc
            time.sleep(0.1)
    raise RuntimeError(f"could not connect to ZEsarUX ZRCP: {last_error}")


def read_byte(sock: socket.socket, address: int) -> int:
    response = command(sock, f"read-memory {address} 1")
    for line in response.splitlines():
        payload = line.strip()
        if re.fullmatch(r"[0-9A-Fa-f]{2}", payload):
            return int(payload, 16)
    raise RuntimeError(f"could not parse byte at 0x{address:04X}: {response!r}")


def read_word(sock: socket.socket, address: int) -> int:
    return read_byte(sock, address) | (read_byte(sock, address + 1) << 8)


def write_bytes(sock: socket.socket, address: int, values: list[int]) -> None:
    payload = " ".join(str(value) for value in values)
    command(sock, f"write-memory {address} {payload}")


def read_coord(sock: socket.socket, address: int) -> tuple[int, int]:
    x = read_word(sock, address)
    y = read_word(sock, address + 2)
    return x, y


def wait_for_byte(
    sock: socket.socket, address: int, expected: int, timeout: float, label: str
) -> None:
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        last = read_byte(sock, address)
        if last == expected:
            return
        time.sleep(0.05)
    shown = "unread" if last is None else f"0x{last:02X}"
    raise RuntimeError(f"{label} did not reach 0x{expected:02X}; last value was {shown}")


def wait_for_byte_change(
    sock: socket.socket, address: int, previous: int, timeout: float, label: str
) -> int:
    deadline = time.monotonic() + timeout
    last = previous
    while time.monotonic() < deadline:
        last = read_byte(sock, address)
        if last != previous:
            return last
        time.sleep(0.05)
    raise RuntimeError(f"{label} remained at 0x{last:02X}")


def wait_for_rendered_game(sock: socket.socket, timeout: float) -> str:
    """Wait until ZEsarUX has converted the freshly written ULA frame to OCR."""
    deadline = time.monotonic() + timeout
    last = ""
    while time.monotonic() < deadline:
        last = command(sock, "get-ocr")
        if "Level:" in last and "@" in last:
            return last
        time.sleep(0.1)
    raise RuntimeError(f"Rogue map/status not visible in OCR: {last!r}")


def validate_screenshot(path: Path) -> None:
    deadline = time.monotonic() + 2.0
    while time.monotonic() < deadline and not path.is_file():
        time.sleep(0.05)
    if not path.is_file():
        raise RuntimeError(f"ZEsarUX did not create screenshot: {path}")
    data = path.read_bytes()
    if not data.startswith(b"P4\n256 192\n") or len(data) < 6155:
        raise RuntimeError(f"invalid 256x192 PBM screenshot: {path}")


def main() -> int:
    args = parse_args()
    for label, path in (("ZEsarUX", args.zesarux), ("TAP", args.tap), ("map", args.map_path)):
        if not path.is_file():
            raise RuntimeError(f"{label} does not exist: {path}")

    symbols = symbols_from_map(args.map_path)
    boot_stage = required_symbol(symbols, "zx_boot_stage")
    turn_count = required_symbol(symbols, "zx_turn_count")
    last_comm = required_symbol(symbols, "last_comm")
    rendered_rows = required_symbol(symbols, "zx_rendered_rows")
    refresh_count = required_symbol(symbols, "zx_refresh_count")
    refresh_turn = required_symbol(symbols, "zx_refresh_turn")
    movement_scratch = required_symbol(symbols, "nh")
    random_move_scratch = required_symbol(symbols, "rndmove_ret")
    monster_list = required_symbol(symbols, "mlist")
    # THING starts with two 16-bit list pointers on this target.
    player_position = required_symbol(symbols, "player") + THING_POSITION_OFFSET
    origin = required_symbol(symbols, "CRT_ORG_CODE")
    for label, address in (
        ("boot marker", boot_stage),
        ("turn counter", turn_count),
        ("last command", last_comm),
        ("rendered-row counter", rendered_rows),
        ("refresh counter", refresh_count),
        ("refresh-turn snapshot", refresh_turn),
        ("movement scratch", movement_scratch),
        ("random-move scratch", random_move_scratch),
        ("monster-list head", monster_list),
        ("player position", player_position),
    ):
        if address >= 0xC000:
            raise RuntimeError(f"{label} is bank-dependent at 0x{address:04X}")

    args.screenshot.parent.mkdir(parents=True, exist_ok=True)
    args.screenshot.unlink(missing_ok=True)
    port = free_local_port()
    emulator_args = [
        str(args.zesarux.resolve()),
        "--noconfigfile",
        "--machine",
        "128k",
        "--tape",
        str(args.tap.resolve()),
        "--vo",
        "null",
        "--ao",
        "null",
        "--nosplash",
        "--enable-remoteprotocol",
        "--remoteprotocol-port",
        str(port),
        "--quickexit",
        "--fastautoload",
    ]

    proc = subprocess.Popen(
        emulator_args, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT
    )
    sock: socket.socket | None = None
    exited_cleanly = False
    try:
        sock = connect(proc, port, min(args.timeout, 10.0))
        wait_for_byte(sock, boot_stage, 0x52, args.timeout, "command loop")
        print(f"PASS boot reached command loop (stage=0x52 at 0x{boot_stage:04X})")

        basic = command(sock, "view-basic")
        loader = re.compile(
            rf'CLEAR\s+VAL\s+"{origin - 1}".*'
            rf'LOAD\s+""\s+CODE.*'
            rf'RANDOMIZE\s+USR\s+VAL\s+"{origin}"',
            re.DOTALL,
        )
        if not loader.search(basic):
            raise RuntimeError(f"BASIC loader was corrupted: {basic!r}")
        print("PASS BASIC loader survived bank loading")

        wait_for_rendered_game(sock, min(args.timeout, 5.0))
        print("PASS renderer shows the Rogue map, hero and status line")

        registers = command(sock, "get-registers")
        if not re.search(r"\bIY=5C3A\b", registers, re.IGNORECASE):
            raise RuntimeError(f"ROM system-variable base was not preserved: {registers!r}")
        print("PASS IY preserves the Spectrum ROM system-variable base")

        if read_byte(sock, refresh_count) == 0:
            wait_for_byte_change(
                sock, refresh_count, 0, args.timeout, "initial screen refresh"
            )
        before_position = read_coord(sock, player_position)
        before_physical = read_byte(sock, turn_count)
        command(sock, "send-keys-event 108 1")
        time.sleep(0.1)
        command(sock, "send-keys-event 108 0")
        wait_for_byte(sock, last_comm, ord("l"), args.timeout, "physical L key")
        after_physical = (before_physical + 1) & 0xFF
        wait_for_byte(
            sock,
            turn_count,
            after_physical,
            args.timeout,
            "physical L turn",
        )
        wait_for_byte(
            sock, refresh_turn, after_physical, args.timeout, "post-L refresh"
        )
        after_position = read_coord(sock, player_position)
        expected_position = (before_position[0] + 1, before_position[1])
        if after_position != expected_position:
            raise RuntimeError(
                f"physical L did not move right: {before_position} -> {after_position}"
            )
        print(
            "PASS emulated keyboard event decodes as ASCII 'l' and moves the hero "
            f"{before_position} -> {after_position}"
        )

        before = read_byte(sock, turn_count)
        command(sock, "send-keys-ascii 200 46")
        wait_for_byte(sock, last_comm, ord("."), args.timeout, "wait command")
        after = (before + 1) & 0xFF
        wait_for_byte(
            sock,
            turn_count,
            after,
            args.timeout,
            "turn counter",
        )
        wait_for_byte(sock, refresh_turn, after, args.timeout, "post-wait refresh")
        print(f"PASS '.' executed one turn ({before} -> {after})")

        rows = read_byte(sock, rendered_rows)
        if rows == 0 or rows >= 24:
            raise RuntimeError(f"partial refresh drew {rows} rows after one turn")
        print(f"PASS partial refresh drew {rows}/24 rows after one turn")

        # Remove the level's original monsters so the wandering-monster fuse
        # can run without combat cancelling the wait.
        write_bytes(sock, monster_list, [0, 0])
        if read_word(sock, monster_list) != 0:
            raise RuntimeError("could not detach initial monsters")

        wandering_monster = 0
        wanderer_turns = 0
        while wanderer_turns < 96:
            before = read_byte(sock, turn_count)
            # One ZRCP command inserts release events between identical keys,
            # avoiding the Spectrum ROM's repeated-key suppression.
            command(sock, "send-keys-ascii 80 46 46 46 46")
            time.sleep(0.12)
            after = read_byte(sock, turn_count)
            if after == before:
                after = wait_for_byte_change(
                    sock, turn_count, before, args.timeout, "wanderer wait batch"
                )
            advanced = (after - before) & 0xFF
            if advanced > 4:
                raise RuntimeError(
                    f"wanderer wait batch advanced {advanced} turns, expected at most 4"
                )
            wanderer_turns += advanced
            wandering_monster = read_word(sock, monster_list)
            if wandering_monster:
                break
        else:
            raise RuntimeError(
                f"wandering monster did not appear within {wanderer_turns} turns"
            )

        if wandering_monster >= 0xC000:
            raise RuntimeError(
                "wandering monster is not in fixed memory: "
                f"0x{wandering_monster:04X}"
            )
        wandering_position = read_coord(
            sock, wandering_monster + THING_POSITION_OFFSET
        )
        if not (
            0 <= wandering_position[0] < 80
            and 0 < wandering_position[1] < 23
        ):
            raise RuntimeError(
                "wandering monster has invalid coordinates: "
                f"{wandering_position} at 0x{wandering_monster:04X}"
            )
        ocr = command(sock, "get-ocr")
        if "bizarre place" in ocr.lower():
            raise RuntimeError(f"wanderer triggered roomin corruption: {ocr!r}")
        print(
            "PASS wandering monster spawned at valid coordinates "
            f"{wandering_position} without banked scratch corruption"
        )

        command(sock, f"save-screen {args.screenshot.resolve()}")
        validate_screenshot(args.screenshot)
        print(f"PASS captured 256x192 screen: {args.screenshot}")

        sock.sendall(b"exit-emulator\n")
        sock.close()
        sock = None
        proc.wait(timeout=5.0)
        if proc.returncode != 0:
            raise RuntimeError(f"ZEsarUX exited with status {proc.returncode}")
        exited_cleanly = True
        print("PASS ZEsarUX exited cleanly")
        return 0
    finally:
        if sock is not None:
            try:
                sock.sendall(b"exit-emulator\n")
            except OSError:
                pass
            sock.close()
        if not exited_cleanly and proc.poll() is None:
            try:
                proc.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                proc.terminate()
                try:
                    proc.wait(timeout=2.0)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"smoke: ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
