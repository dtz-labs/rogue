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
STATUS_CHAR_ROW = 23
THING_POSITION_OFFSET = 4
THING_TURN_OFFSET = 8
THING_DEST_OFFSET = 12
THING_FLAGS_OFFSET = 14
THING_STATS_EXP_OFFSET = 18
THING_STATS_LEVEL_OFFSET = 22
THING_STATS_ARMOR_OFFSET = 24
THING_STATS_HP_OFFSET = 26
THING_STATS_DAMAGE_OFFSET = 28
THING_STATS_MAX_HP_OFFSET = 41
THING_ROOM_OFFSET = 43
THING_PACK_OFFSET = 45
OBJECT_TYPE_OFFSET = 4
OBJECT_POSITION_OFFSET = 6
OBJECT_COUNT_OFFSET = 31
OBJECT_WHICH_OFFSET = 33
OBJECT_GOLD_VALUE_OFFSET = 39
OBJECT_FLAGS_OFFSET = 41
OBJECT_GROUP_OFFSET = 43
OBJECT_LABEL_OFFSET = 45
PLACE_ROWS = 24
PLACE_SIZE = 4
ROOM_SIZE = 66
ROOM_FLAGS_OFFSET = 14
ISMAZE = 0x04
ISRUN = 0x2000
F_REAL = 0x10
FLOOR = ord(".")
PASSAGE = ord("#")
STAIRS = ord("%")
PLACE_BANK_COLUMNS = ((0, 25), (1, 19), (3, 21), (4, 15))


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


def send_physical_key(sock: socket.socket, key: int) -> None:
    command(sock, f"send-keys-event {key} 1")
    time.sleep(0.25)
    command(sock, f"send-keys-event {key} 0")
    time.sleep(0.1)


def send_break(sock: socket.socket) -> None:
    """Press the Spectrum BREAK chord (Caps Shift + Space)."""
    command(sock, "send-keys-event 135 1")
    time.sleep(0.05)
    command(sock, "send-keys-event 128 1")
    time.sleep(0.25)
    command(sock, "send-keys-event 128 0")
    command(sock, "send-keys-event 135 0")
    time.sleep(0.15)


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


def read_bytes(sock: socket.socket, address: int, count: int) -> bytes:
    response = command(sock, f"read-memory {address} {count}")
    expected_length = count * 2
    for line in response.splitlines():
        payload = line.strip()
        if len(payload) == expected_length and re.fullmatch(
            r"[0-9A-Fa-f]+", payload
        ):
            return bytes.fromhex(payload)
    raise RuntimeError(
        f"could not parse {count} bytes at 0x{address:X}: {response!r}"
    )


def read_word(sock: socket.socket, address: int) -> int:
    return read_byte(sock, address) | (read_byte(sock, address + 1) << 8)


def read_dword(sock: socket.socket, address: int) -> int:
    return read_word(sock, address) | (read_word(sock, address + 2) << 16)


def write_bytes(sock: socket.socket, address: int, *values: int) -> None:
    payload = " ".join(str(value & 0xFF) for value in values)
    command(sock, f"write-memory {address} {payload}")


def write_word(sock: socket.socket, address: int, value: int) -> None:
    write_bytes(sock, address, value, value >> 8)


def write_dword(sock: socket.socket, address: int, value: int) -> None:
    write_bytes(sock, address, value, value >> 8, value >> 16, value >> 24)


def read_coord(sock: socket.socket, address: int) -> tuple[int, int]:
    x = read_word(sock, address)
    y = read_word(sock, address + 2)
    return x, y


def write_coord(sock: socket.socket, address: int, value: tuple[int, int]) -> None:
    write_bytes(sock, address, value[0], value[0] >> 8, value[1], value[1] >> 8)


def bank_symbol_ram_address(address: int) -> int:
    bank = address >> 16
    cpu_address = address & 0xFFFF
    if bank not in range(8) or cpu_address < 0xC000:
        raise RuntimeError(f"invalid banked symbol address: 0x{address:X}")
    return bank * 0x4000 + cpu_address - 0xC000


def read_machine_ram(sock: socket.socket, address: int, count: int) -> bytes:
    command(sock, "set-memory-zone 0")
    try:
        return read_bytes(sock, address, count)
    finally:
        command(sock, "set-memory-zone -1")


def write_machine_ram(sock: socket.socket, address: int, *values: int) -> None:
    command(sock, "set-memory-zone 0")
    try:
        write_bytes(sock, address, *values)
    finally:
        command(sock, "set-memory-zone -1")


def map_cell_ram_address(
    place_bases: dict[int, int], x: int, y: int
) -> int:
    if not (0 <= x < 80 and 0 <= y < PLACE_ROWS):
        raise RuntimeError(f"invalid map coordinate ({x}, {y})")
    first_column = 0
    for bank, columns in PLACE_BANK_COLUMNS:
        if x < first_column + columns:
            local_index = (x - first_column) * PLACE_ROWS + y
            return place_bases[bank] + local_index * PLACE_SIZE
        first_column += columns
    raise RuntimeError(f"map coordinate did not resolve to a bank: ({x}, {y})")


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


def wait_for_word(
    sock: socket.socket, address: int, expected: int, timeout: float, label: str
) -> None:
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        last = read_word(sock, address)
        if last == expected:
            return
        time.sleep(0.05)
    shown = "unread" if last is None else f"0x{last:04X}"
    raise RuntimeError(f"{label} did not reach 0x{expected:04X}; last value was {shown}")


def send_physical_key_until_word(
    sock: socket.socket,
    key: int,
    address: int,
    expected: int,
    timeout: float,
    label: str,
) -> None:
    """Retry a dropped emulator key only while its state change is still pending."""
    deadline = time.monotonic() + timeout
    last = read_word(sock, address)
    while time.monotonic() < deadline:
        if last == expected:
            return
        send_physical_key(sock, key)
        attempt_deadline = min(deadline, time.monotonic() + 2.0)
        while time.monotonic() < attempt_deadline:
            last = read_word(sock, address)
            if last == expected:
                return
            time.sleep(0.05)
    raise RuntimeError(
        f"{label} did not reach 0x{expected:04X}; last value was 0x{last:04X}"
    )


def send_physical_key_until_byte_change(
    sock: socket.socket,
    key: int,
    address: int,
    previous: int,
    timeout: float,
    label: str,
) -> int:
    """Retry a dropped emulator key until an 8-bit state marker advances."""
    deadline = time.monotonic() + timeout
    last = read_byte(sock, address)
    while time.monotonic() < deadline:
        if last != previous:
            return last
        send_physical_key(sock, key)
        attempt_deadline = min(deadline, time.monotonic() + 2.0)
        while time.monotonic() < attempt_deadline:
            last = read_byte(sock, address)
            if last != previous:
                return last
            time.sleep(0.05)
    raise RuntimeError(f"{label} remained at 0x{last:02X}")


def send_physical_key_until_byte(
    sock: socket.socket,
    key: int,
    address: int,
    expected: int,
    timeout: float,
    label: str,
) -> None:
    """Retry a dropped emulator key until an 8-bit marker matches."""
    deadline = time.monotonic() + timeout
    last = read_byte(sock, address)
    while time.monotonic() < deadline:
        if last == expected:
            return
        send_physical_key(sock, key)
        attempt_deadline = min(deadline, time.monotonic() + 2.0)
        while time.monotonic() < attempt_deadline:
            last = read_byte(sock, address)
            if last == expected:
                return
            time.sleep(0.05)
    raise RuntimeError(
        f"{label} did not reach 0x{expected:02X}; last value was 0x{last:02X}"
    )


def send_physical_key_until_coord(
    sock: socket.socket,
    key: int,
    address: int,
    expected: tuple[int, int],
    timeout: float,
    label: str,
) -> None:
    """Retry a dropped key only while the expected movement is still pending."""
    deadline = time.monotonic() + timeout
    last = read_coord(sock, address)
    while time.monotonic() < deadline:
        if last == expected:
            return
        send_physical_key(sock, key)
        attempt_deadline = min(deadline, time.monotonic() + 2.0)
        while time.monotonic() < attempt_deadline:
            last = read_coord(sock, address)
            if last == expected:
                return
            time.sleep(0.05)
    raise RuntimeError(f"{label} did not reach {expected}; last value was {last}")


def send_physical_key_until_ocr(
    sock: socket.socket,
    key: int,
    needles: tuple[str, ...],
    timeout: float,
    label: str,
) -> str:
    """Retry a dropped physical key until the expected screen state appears."""
    deadline = time.monotonic() + timeout
    last = ""
    while time.monotonic() < deadline:
        last = command(sock, "get-ocr")
        if all(needle in last for needle in needles):
            return last
        send_physical_key(sock, key)
        attempt_deadline = min(deadline, time.monotonic() + 2.0)
        while time.monotonic() < attempt_deadline:
            last = command(sock, "get-ocr")
            if all(needle in last for needle in needles):
                return last
            time.sleep(0.05)
    raise RuntimeError(f"{label} did not show {needles!r}: {last!r}")


def read_place_chunks(
    sock: socket.socket, place_bases: dict[int, int]
) -> dict[int, bytes]:
    command(sock, "set-memory-zone 0")
    try:
        return {
            bank: read_bytes(
                sock, place_bases[bank], columns * PLACE_ROWS * PLACE_SIZE
            )
            for bank, columns in PLACE_BANK_COLUMNS
        }
    finally:
        command(sock, "set-memory-zone -1")


def place_from_chunks(
    chunks: dict[int, bytes], x: int, y: int
) -> tuple[int, int, int]:
    first_column = 0
    for bank, columns in PLACE_BANK_COLUMNS:
        if x < first_column + columns:
            offset = ((x - first_column) * PLACE_ROWS + y) * PLACE_SIZE
            data = chunks[bank][offset : offset + PLACE_SIZE]
            return data[0], data[1], data[2] | (data[3] << 8)
        first_column += columns
    raise RuntimeError(f"map coordinate did not resolve to a bank: ({x}, {y})")


def maze_step(
    room_data: bytes, chunks: dict[int, bytes]
) -> tuple[tuple[int, int], tuple[int, int], int] | None:
    room_x = room_data[0] | (room_data[1] << 8)
    room_y = room_data[2] | (room_data[3] << 8)
    room_width = room_data[4] | (room_data[5] << 8)
    room_height = room_data[6] | (room_data[7] << 8)
    directions = ((-1, 0, ord("h")), (1, 0, ord("l")), (0, -1, ord("k")), (0, 1, ord("j")))

    for y in range(max(1, room_y), min(22, room_y + room_height) + 1):
        for x in range(max(0, room_x), min(79, room_x + room_width) + 1):
            ch, _, monster = place_from_chunks(chunks, x, y)
            if ch != PASSAGE or monster:
                continue
            for dx, dy, key in directions:
                target = (x + dx, y + dy)
                if not (0 <= target[0] < 80 and 1 <= target[1] <= 22):
                    continue
                target_ch, _, target_monster = place_from_chunks(
                    chunks, target[0], target[1]
                )
                if target_ch == PASSAGE and not target_monster:
                    return (x, y), target, key
    return None


def leave_name_prompt(
    sock: socket.socket, boot_stage: int, timeout: float, label: str
) -> None:
    """Send ENTER until the name prompt lets go, then wait out level setup.

    A single ENTER here is dropped often enough to need resending, but building
    the first level then takes seconds on its own. Retrying against one deadline
    conflates the two, so a slower startup looks like a lost keypress and the
    retries run out. Resend only while the prompt is still up -- the boot marker
    leaves 'N' as soon as the key lands -- and time the setup separately.
    """
    deadline = time.monotonic() + timeout
    while read_byte(sock, boot_stage) == ord("N"):
        if time.monotonic() >= deadline:
            raise RuntimeError(f"{label}: name prompt never accepted ENTER")
        command(sock, "send-keys-ascii 200 13")
        time.sleep(1.0)
    wait_for_byte(sock, boot_stage, 0x52, timeout, label)


def status_row_ink(sock: socket.socket) -> int:
    """Count set pixels on the status row, straight out of the display file.

    The status row is drawn with the 4x8 font so that 64 characters fit, and
    ZEsarUX's OCR only recognises the 8x8 ROM glyphs -- it reads that row as
    blank. Checking the pixels still catches the failure that matters, which is
    the row not being drawn at all.
    """
    total = 0
    for scanline in range(8):
        y = (STATUS_CHAR_ROW << 3) + scanline
        address = (0x4000 + ((y & 0xC0) << 5) + ((y & 0x07) << 8)
                   + ((y & 0x38) << 2))
        total += sum(bin(b).count("1") for b in read_bytes(sock, address, 32))
    return total


def wait_for_status_row(sock: socket.socket, timeout: float, label: str) -> None:
    deadline = time.monotonic() + timeout
    ink = 0
    while time.monotonic() < deadline:
        ink = status_row_ink(sock)
        if ink:
            return
        time.sleep(0.1)
    raise RuntimeError(f"{label}: status row drew no pixels (ink={ink})")


def wait_for_rendered_game(sock: socket.socket, timeout: float) -> str:
    """Wait until ZEsarUX has converted the freshly written ULA frame to OCR."""
    deadline = time.monotonic() + timeout
    last = ""
    while time.monotonic() < deadline:
        last = command(sock, "get-ocr")
        if "@" in last and ("|" in last or "-" in last):
            return last
        time.sleep(0.1)
    raise RuntimeError(f"Rogue map not visible in OCR: {last!r}")


def wait_for_ocr(sock: socket.socket, needles: tuple[str, ...], timeout: float) -> str:
    deadline = time.monotonic() + timeout
    last = ""
    while time.monotonic() < deadline:
        last = command(sock, "get-ocr")
        if all(needle in last for needle in needles):
            return last
        time.sleep(0.1)
    raise RuntimeError(f"OCR did not contain {needles!r}: {last!r}")


def wait_for_compact_ocr(sock: socket.socket, needle: str, timeout: float) -> str:
    """Match text even when the 32-column renderer splits a word across rows."""
    deadline = time.monotonic() + timeout
    last = ""
    while time.monotonic() < deadline:
        last = command(sock, "get-ocr")
        if needle in "".join(last.split()):
            return last
        time.sleep(0.1)
    raise RuntimeError(f"compact OCR did not contain {needle!r}: {last!r}")


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
    viewport_first_col = required_symbol(symbols, "zx_viewport_first_col")
    previous_message = required_symbol(symbols, "huh")
    movement_scratch = required_symbol(symbols, "nh")
    random_move_scratch = required_symbol(symbols, "rndmove_ret")
    monster_list = required_symbol(symbols, "mlist")
    level_objects = required_symbol(symbols, "lvl_obj")
    purse = required_symbol(symbols, "purse")
    playing = required_symbol(symbols, "playing")
    passages = required_symbol(symbols, "passages")
    dungeon_level = required_symbol(symbols, "level")
    random_seed = required_symbol(symbols, "seed")
    wanderer_between = bank_symbol_ram_address(required_symbol(symbols, "between"))
    dungeon_number = required_symbol(symbols, "dnum")
    food_remaining = required_symbol(symbols, "food_left")
    rooms = required_symbol(symbols, "rooms")
    hero_name = required_symbol(symbols, "whoami")
    place_bases = {
        bank: bank_symbol_ram_address(required_symbol(symbols, f"zx_places_bank{bank}"))
        for bank, _ in PLACE_BANK_COLUMNS
    }
    screen_bank3 = bank_symbol_ram_address(required_symbol(symbols, "zx_screen_bank3"))
    # THING starts with two 16-bit list pointers on this target.
    player_position = required_symbol(symbols, "player") + THING_POSITION_OFFSET
    player_pack = required_symbol(symbols, "player") + THING_PACK_OFFSET
    player_exp = required_symbol(symbols, "player") + THING_STATS_EXP_OFFSET
    status_as_message = required_symbol(symbols, "stat_msg")
    origin = required_symbol(symbols, "CRT_ORG_CODE")
    for label, address in (
        ("boot marker", boot_stage),
        ("turn counter", turn_count),
        ("last command", last_comm),
        ("rendered-row counter", rendered_rows),
        ("refresh counter", refresh_count),
        ("refresh-turn snapshot", refresh_turn),
        ("viewport column", viewport_first_col),
        ("previous-message buffer", previous_message),
        ("movement scratch", movement_scratch),
        ("random-move scratch", random_move_scratch),
        ("monster-list head", monster_list),
        ("level-object list head", level_objects),
        ("gold purse", purse),
        ("playing flag", playing),
        ("passage table", passages),
        ("dungeon level", dungeon_level),
        ("random seed", random_seed),
        ("dungeon number", dungeon_number),
        ("food counter", food_remaining),
        ("room table", rooms),
        ("player position", player_position),
        ("player pack", player_pack),
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
        wait_for_byte(sock, boot_stage, ord("H"), args.timeout, "startup help")
        startup_help = wait_for_ocr(
            sock, ("ROGUE ZX128 - KEYS", "SPACE - enter your name"), args.timeout
        )
        if "--More--" in startup_help:
            raise RuntimeError("startup help unexpectedly used --More--")
        send_physical_key_until_byte(
            sock, ord(" "), boot_stage, ord("N"), args.timeout, "name prompt"
        )
        wait_for_ocr(sock, ("Name your hero", "ENTER keeps the name Rogue"), args.timeout)
        leave_name_prompt(sock, boot_stage, args.timeout, "command loop")
        print("PASS startup quick help and default-name prompt")
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
        initial_pack = read_word(sock, player_pack)
        initial_game_seed = read_dword(sock, random_seed)
        wait_for_byte(sock, viewport_first_col, 48, args.timeout, "room viewport")
        print("PASS room-aware viewport shows the complete starting room at column 48")

        registers = command(sock, "get-registers")
        if not re.search(r"\bIY=5C3A\b", registers, re.IGNORECASE):
            raise RuntimeError(f"ROM system-variable base was not preserved: {registers!r}")
        print("PASS IY preserves the Spectrum ROM system-variable base")

        initial_monster = read_word(sock, monster_list)
        if not initial_monster or initial_monster >= 0xC000:
            raise RuntimeError(
                f"initial monster list has invalid head: 0x{initial_monster:04X}"
            )

        # Exercise the real two-message combat path before detaching the level
        # monsters.  The runner's forced counter-miss is 31 characters, so
        # the second message only fits after the renderer's full second line
        # is admitted by endmsg().
        hero_x, hero_y = read_coord(sock, player_position)
        if hero_x + 1 < 80:
            combat_target = (hero_x + 1, hero_y)
            combat_key = ord("l")
        else:
            combat_target = (hero_x - 1, hero_y)
            combat_key = ord("h")
        old_monster_position = read_coord(
            sock, initial_monster + THING_POSITION_OFFSET
        )
        touched_cells = {old_monster_position, combat_target}
        for x in range(combat_target[0] - 1, combat_target[0] + 2):
            for y in range(combat_target[1] - 1, combat_target[1] + 2):
                if (
                    0 <= x < 80
                    and 0 <= y < PLACE_ROWS
                    and (x, y) != (hero_x, hero_y)
                ):
                    touched_cells.add((x, y))
        saved_cells = {
            coordinate: read_machine_ram(
                sock, map_cell_ram_address(place_bases, *coordinate), PLACE_SIZE
            )
            for coordinate in touched_cells
        }
        for coordinate, saved_cell in saved_cells.items():
            cell = bytearray(saved_cell)
            cell[2:4] = b"\0\0"
            if coordinate != old_monster_position and coordinate != combat_target:
                cell[0] = ord(" ")
            write_machine_ram(
                sock, map_cell_ram_address(place_bases, *coordinate), *cell
            )
        write_machine_ram(
            sock,
            map_cell_ram_address(place_bases, *combat_target),
            FLOOR,
            F_REAL,
            initial_monster,
            initial_monster >> 8,
        )

        player_room = read_word(
            sock, player_position - THING_POSITION_OFFSET + THING_ROOM_OFFSET
        )
        write_word(sock, monster_list, initial_monster)
        write_bytes(sock, initial_monster, 0, 0, 0, 0)
        write_coord(
            sock, initial_monster + THING_POSITION_OFFSET, combat_target
        )
        write_bytes(
            sock,
            initial_monster + THING_TURN_OFFSET,
            1,
            ord("H"),
            ord("H"),
            FLOOR,
        )
        write_word(sock, initial_monster + THING_DEST_OFFSET, player_position)
        write_word(sock, initial_monster + THING_FLAGS_OFFSET, ISRUN)
        write_word(sock, initial_monster + THING_STATS_LEVEL_OFFSET, -100)
        write_word(sock, initial_monster + THING_STATS_ARMOR_OFFSET, -100)
        write_word(sock, initial_monster + THING_STATS_HP_OFFSET, 300)
        write_bytes(
            sock,
            initial_monster + THING_STATS_DAMAGE_OFFSET,
            ord("1"),
            ord("x"),
            ord("1"),
            0,
        )
        write_word(sock, initial_monster + THING_STATS_MAX_HP_OFFSET, 300)
        write_word(sock, initial_monster + THING_ROOM_OFFSET, player_room)

        seed_before_combat = read_dword(sock, random_seed)
        write_dword(sock, random_seed, 8)
        write_bytes(sock, last_comm, 0)
        send_physical_key_until_byte(
            sock,
            combat_key,
            last_comm,
            combat_key,
            args.timeout,
            "deterministic hobgoblin combat",
        )
        combat_ocr = wait_for_ocr(
            sock,
            (
                "You miss the hobgoblin",
                "The hobgoblin barely misses you",
            ),
            args.timeout,
        )
        if "--More--" in combat_ocr:
            raise RuntimeError("two short combat messages unexpectedly used --More--")
        if read_coord(sock, player_position) != (hero_x, hero_y):
            raise RuntimeError("combat regression moved the hero into the monster")
        write_dword(sock, random_seed, seed_before_combat)
        for coordinate, saved_cell in saved_cells.items():
            write_machine_ram(
                sock, map_cell_ram_address(place_bases, *coordinate), *saved_cell
            )
        print("PASS two natural combat messages use both rows without --More--")

        # Detach the initial monsters in emulated RAM so waiting for the
        # wandering-monster fuse is deterministic and cannot kill the hero.
        write_bytes(sock, monster_list, 0, 0)
        if read_word(sock, monster_list) != 0:
            raise RuntimeError("could not detach initial monsters")
        print("PASS detached initial monsters for wanderer regression")

        # Exercise the bank-1 pickup path against find_obj() in bank 3.  A
        # missing banked-call annotation used to jump into unrelated bank-1
        # code here and return the program to BASIC as soon as an item was
        # collected.  Reuse a generated object and place it under the hero so
        # the repeat command can invoke the ordinary pickup implementation.
        gold = read_word(sock, level_objects)
        if not gold or gold >= 0xC000:
            raise RuntimeError(f"level object has invalid address: 0x{gold:04X}")
        next_object = read_word(sock, gold)
        hero_x, hero_y = read_coord(sock, player_position)
        write_bytes(sock, gold + OBJECT_TYPE_OFFSET, ord("*"), 0)
        write_bytes(
            sock,
            gold + OBJECT_POSITION_OFFSET,
            hero_x,
            hero_x >> 8,
            hero_y,
            hero_y >> 8,
        )
        gold_value = 37
        write_bytes(sock, gold + OBJECT_GOLD_VALUE_OFFSET, gold_value, 0)
        before_pickup = read_byte(sock, turn_count)
        write_bytes(sock, last_comm, ord(","))
        send_physical_key_until_word(
            sock,
            ord("a"),
            purse,
            gold_value,
            args.timeout,
            "gold pickup purse",
        )
        wait_for_word(sock, purse, gold_value, args.timeout, "gold pickup purse")
        wait_for_word(
            sock, level_objects, next_object, args.timeout, "gold pickup object removal"
        )
        wait_for_ocr(sock, ("gold pieces",), args.timeout)
        if read_byte(sock, playing) != 1:
            raise RuntimeError("gold pickup stopped the game loop")
        wait_for_byte_change(
            sock, turn_count, before_pickup, args.timeout, "gold pickup command"
        )
        time.sleep(0.3)
        command(sock, "send-keys-ascii 200 118")
        wait_for_byte(sock, last_comm, ord("v"), args.timeout, "post-pickup command")
        wait_for_ocr(sock, ("Version",), args.timeout)
        if read_byte(sock, playing) != 1:
            raise RuntimeError("game loop stopped after the command following pickup")
        print("PASS gold pickup returns from bank 3 and accepts the next command")

        # Reuse the next floor object as an unidentified potion.  Potion names
        # are selected in bank 3 but rendered by inv_name() in bank 1, so this
        # catches both an unbanked helper call and stale pointers into bank 3.
        potion = read_word(sock, level_objects)
        if not potion or potion >= 0xC000:
            raise RuntimeError(f"level object has invalid address: 0x{potion:04X}")
        next_object = read_word(sock, potion)
        write_bytes(sock, potion + OBJECT_TYPE_OFFSET, ord("!"), 0)
        write_bytes(
            sock,
            potion + OBJECT_POSITION_OFFSET,
            hero_x,
            hero_x >> 8,
            hero_y,
            hero_y >> 8,
        )
        write_bytes(sock, potion + OBJECT_COUNT_OFFSET, 1, 0)
        write_bytes(sock, potion + OBJECT_WHICH_OFFSET, 0, 0)
        write_bytes(sock, potion + OBJECT_FLAGS_OFFSET, 0, 0)
        write_bytes(sock, potion + OBJECT_GROUP_OFFSET, 0, 0)
        write_bytes(sock, potion + OBJECT_LABEL_OFFSET, 0, 0)
        before_pickup = read_byte(sock, turn_count)
        write_bytes(sock, last_comm, ord(","))
        send_physical_key_until_word(
            sock,
            ord("a"),
            level_objects,
            next_object,
            args.timeout,
            "potion pickup object removal",
        )
        wait_for_word(
            sock, level_objects, next_object, args.timeout, "potion pickup object removal"
        )
        wait_for_compact_ocr(sock, "potion", args.timeout)
        wait_for_byte_change(
            sock, turn_count, before_pickup, args.timeout, "potion pickup command"
        )
        if read_byte(sock, playing) != 1 or read_byte(sock, boot_stage) != 0x52:
            raise RuntimeError("potion pickup reset or stopped the game")

        # Match the reported sequence exactly: collect '!', then press S.
        time.sleep(0.3)
        command(sock, "send-keys-ascii 200 83")
        wait_for_byte(sock, last_comm, ord("S"), args.timeout, "save command")
        wait_for_ocr(
            sock,
            ("Saving is not available in this", "build."),
            args.timeout,
        )
        if read_byte(sock, playing) != 1 or read_byte(sock, boot_stage) != 0x52:
            raise RuntimeError("save command after potion pickup reset or stopped the game")
        time.sleep(0.3)
        send_physical_key(sock, ord("v"))
        wait_for_byte(sock, last_comm, ord("v"), args.timeout, "post-save command")
        wait_for_ocr(sock, ("Version",), args.timeout)
        print("PASS potion pickup and following S command stay in the game")

        # Modal option editing must use an unshifted 32-column view and restore
        # the complete logical dungeon afterwards.  BREAK is Caps Shift+Space
        # on a real Spectrum, so exercise that chord instead of injecting ESC.
        dungeon_rows_before_options = read_machine_ram(
            sock, screen_bank3 + 80, 22 * 80
        )
        write_bytes(sock, viewport_first_col, 48)
        write_bytes(sock, last_comm, 0)
        send_physical_key_until_byte(
            sock, ord("o"), last_comm, ord("o"), args.timeout, "options command"
        )
        options_ocr = wait_for_ocr(
            sock, ("terse: [False]", "flush:", "jump:"), args.timeout
        )
        if any(line.strip() in ("ue", "rue") for line in options_ocr.splitlines()):
            raise RuntimeError(f"options screen used the dungeon viewport: {options_ocr!r}")
        send_physical_key_until_ocr(
            sock,
            ord("t"),
            ("terse: True", "flush: [False]"),
            args.timeout,
            "advance from terse option",
        )
        for attempt in range(3):
            command(sock, "send-keys-ascii 200 45")
            try:
                wait_for_ocr(
                    sock, ("terse: [True]", "flush: False"), 2.0
                )
                break
            except RuntimeError:
                if attempt == 2:
                    raise
        send_physical_key_until_ocr(
            sock,
            ord("f"),
            ("terse: False", "flush: [False]"),
            args.timeout,
            "restore terse option",
        )
        for attempt in range(2):
            send_break(sock)
            try:
                wait_for_ocr(sock, ("--Press space to continue--",), 2.0)
                break
            except RuntimeError:
                if attempt:
                    raise
        for attempt in range(2):
            send_physical_key(sock, ord(" "))
            try:
                wait_for_status_row(sock, 2.0, "status row after overlay")
                break
            except RuntimeError:
                if attempt:
                    raise
        dungeon_rows_after_options = read_machine_ram(
            sock, screen_bank3 + 80, 22 * 80
        )
        if dungeon_rows_after_options != dungeon_rows_before_options:
            raise RuntimeError("options did not restore the complete logical dungeon")
        wait_for_byte(sock, viewport_first_col, 48, args.timeout, "options viewport restore")
        write_bytes(sock, last_comm, 0)
        send_physical_key_until_byte(
            sock, ord("v"), last_comm, ord("v"), args.timeout, "command after BREAK"
        )
        wait_for_ocr(sock, ("Version",), args.timeout)
        print("PASS compact options, BREAK cancel, and full dungeon restore")

        # Inventory is a physical overlay: all entries and its prompt are
        # visible together, while the logical dungeon remains untouched.
        dungeon_rows_before_inventory = read_machine_ram(
            sock, screen_bank3 + 80, 22 * 80
        )
        write_bytes(sock, last_comm, 0)
        send_physical_key_until_byte(
            sock, ord("i"), last_comm, ord("i"), args.timeout, "inventory command"
        )
        inventory_ocr = wait_for_ocr(
            sock, ("a)", "b)", "c)", "--Press space to continue--"), args.timeout
        )
        if "--More--" in inventory_ocr:
            raise RuntimeError("full-screen inventory unexpectedly used --More--")
        for attempt in range(2):
            send_physical_key(sock, ord(" "))
            try:
                wait_for_status_row(sock, 2.0, "status row after overlay")
                break
            except RuntimeError:
                if attempt:
                    raise
        dungeon_rows_after_inventory = read_machine_ram(
            sock, screen_bank3 + 80, 22 * 80
        )
        if dungeon_rows_after_inventory != dungeon_rows_before_inventory:
            raise RuntimeError("inventory modified the logical dungeon")
        wait_for_byte(
            sock, viewport_first_col, 48, args.timeout, "inventory viewport restore"
        )
        print("PASS full-screen inventory overlay and complete dungeon redraw")

        if read_byte(sock, refresh_count) == 0:
            wait_for_byte_change(
                sock, refresh_count, 0, args.timeout, "initial screen refresh"
            )

        player_room = player_position - THING_POSITION_OFFSET + THING_ROOM_OFFSET
        starting_room = read_word(sock, player_room)
        if not starting_room or starting_room >= 0xC000:
            raise RuntimeError(
                f"starting room pointer is invalid: 0x{starting_room:04X}"
            )
        # Passage descriptors carry ISGONE.  With the hero at x=60, a view at
        # 32 must advance by exactly one eight-column step to restore the
        # four-column corridor dead-zone.
        write_bytes(sock, player_room, passages & 0xFF, passages >> 8)
        write_bytes(sock, viewport_first_col, 32)
        write_bytes(sock, last_comm, 0)
        send_physical_key_until_byte(
            sock,
            ord(" "),
            last_comm,
            ord(" "),
            args.timeout,
            "viewport test command",
        )
        wait_for_byte(sock, viewport_first_col, 40, args.timeout, "corridor viewport")
        write_bytes(sock, player_room, starting_room & 0xFF, starting_room >> 8)
        print("PASS corridor viewport advances by an eight-column step")

        time.sleep(0.3)
        short_message = b"Saving is not available in this build."
        write_bytes(sock, previous_message, *short_message, 0)
        write_bytes(sock, last_comm, 16)
        for attempt in range(2):
            send_physical_key(sock, 97)
            try:
                save_message = wait_for_ocr(
                    sock,
                    ("Saving is not available in this", "build."),
                    1.0,
                )
                break
            except RuntimeError:
                if attempt:
                    raise
        wait_for_byte(
            sock, viewport_first_col, 48, args.timeout, "restored room viewport"
        )
        if "--More--" in save_message:
            raise RuntimeError("two-line save message unexpectedly requested --More--")
        print("PASS 38-character message uses two rows without --More--")

        time.sleep(0.3)
        long_message = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789TAIL"
        write_bytes(sock, previous_message, *long_message, 0)
        write_bytes(sock, last_comm, 16)
        for attempt in range(2):
            send_physical_key(sock, 97)
            try:
                wait_for_ocr(sock, ("--More--",), 1.0)
                break
            except RuntimeError:
                if attempt:
                    raise
        for attempt in range(2):
            send_physical_key(sock, 32)
            try:
                paged_status = wait_for_ocr(sock, ("TAIL",), 1.0)
                break
            except RuntimeError:
                if attempt:
                    raise
        if "--More--" in paged_status:
            raise RuntimeError("--More-- did not clear after Space")
        top_line = paged_status.splitlines()[0] if paged_status.splitlines() else ""
        if "TAIL" not in top_line:
            raise RuntimeError(f"message continuation was lost: {paged_status!r}")
        wait_for_byte(sock, viewport_first_col, 48, args.timeout, "restored room viewport")
        print("PASS long message paginates through visible --More-- without loss")

        time.sleep(0.3)
        before_position = read_coord(sock, player_position)
        before_physical = read_byte(sock, turn_count)
        command(sock, "send-keys-event 108 1")
        time.sleep(0.25)
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
        if read_byte(sock, viewport_first_col) != 48:
            raise RuntimeError("viewport moved while the hero remained in one room")
        print("PASS viewport stays fixed while the hero remains inside the room")

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

        wandering_monster = 0
        wanderer_turns = 0
        while wanderer_turns < 96:
            before = read_byte(sock, turn_count)
            # Once the swander fuse starts rollwand(), make its first eligible
            # d6 roll deterministic instead of accepting a one-in-six CI flake.
            write_machine_ram(sock, wanderer_between, 3, 0)
            # BEFORE itself consumes one RNG value; seed 5 makes rollwand's
            # following d6 value equal four as soon as its daemon is active.
            write_dword(sock, random_seed, 5)
            command(sock, "send-keys-ascii 80 46")
            time.sleep(0.08)
            after = read_byte(sock, turn_count)
            if after == before:
                after = wait_for_byte_change(
                    sock, turn_count, before, args.timeout, "wanderer wait"
                )
            advanced = (after - before) & 0xFF
            if advanced > 1:
                raise RuntimeError(
                    f"wanderer wait advanced {advanced} turns, expected at most one"
                )
            wanderer_turns += advanced
            wandering_monster = read_word(sock, monster_list)
            if wandering_monster:
                break
        else:
            raise RuntimeError(
                f"wandering monster did not appear within {wanderer_turns} turns"
            )

        if not wandering_monster or wandering_monster >= 0xC000:
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

        # Probe the first and last logical rows on both sides of every map-bank
        # boundary.  Each injected staircase is read through the production
        # zx_place_get() path before a complete clear and level regeneration.
        boundary_coordinates = (
            (0, 0),
            (24, 23),
            (25, 0),
            (43, 23),
            (44, 0),
            (64, 23),
            (65, 0),
            (79, 23),
        )
        traversed_maze = False
        for attempt, coordinate in enumerate(boundary_coordinates):
            previous_turn = read_byte(sock, turn_count)
            write_word(sock, dungeon_level, 10)
            write_dword(sock, random_seed, 0x13579BDF + attempt * 0x1021)
            write_machine_ram(
                sock,
                map_cell_ram_address(place_bases, *coordinate),
                STAIRS,
            )
            write_coord(sock, player_position, coordinate)
            write_bytes(sock, last_comm, ord(">"))
            send_physical_key_until_word(
                sock,
                ord("a"),
                dungeon_level,
                11,
                args.timeout,
                f"map boundary descent at {coordinate}",
            )
            # level is incremented at the beginning of new_level().  Complete
            # one real turn and wait for the following command-loop refresh so
            # the next RAM injection cannot race the rest of level generation.
            completed_turn = send_physical_key_until_byte_change(
                sock,
                ord("."),
                turn_count,
                previous_turn,
                args.timeout,
                f"turn after map boundary {coordinate}",
            )
            wait_for_byte(
                sock,
                refresh_turn,
                completed_turn,
                args.timeout,
                f"command-loop refresh after map boundary {coordinate}",
            )
            if read_byte(sock, playing) != 1 or read_byte(sock, boot_stage) != 0x52:
                raise RuntimeError(
                    f"map boundary descent at {coordinate} left the game"
                )
            print(f"PASS map boundary descent at {coordinate}")

            room_table = read_bytes(sock, rooms, ROOM_SIZE * 9)
            maze_rooms = [
                index
                for index in range(9)
                if (
                    room_table[
                        index * ROOM_SIZE + ROOM_FLAGS_OFFSET
                    ]
                    | (
                        room_table[
                            index * ROOM_SIZE + ROOM_FLAGS_OFFSET + 1
                        ]
                        << 8
                    )
                )
                & ISMAZE
            ]
            if maze_rooms and not traversed_maze:
                chunks = read_place_chunks(sock, place_bases)
                for room_index in maze_rooms:
                    room_data = room_table[
                        room_index * ROOM_SIZE : (room_index + 1) * ROOM_SIZE
                    ]
                    step = maze_step(room_data, chunks)
                    if step is None:
                        continue
                    start, target, key = step
                    write_coord(sock, player_position, start)
                    write_bytes(sock, last_comm, 0)
                    send_physical_key_until_coord(
                        sock,
                        key,
                        player_position,
                        target,
                        args.timeout,
                        "maze traversal",
                    )
                    if (
                        read_byte(sock, playing) != 1
                        or read_byte(sock, boot_stage) != 0x52
                    ):
                        raise RuntimeError("maze traversal left the game")
                    traversed_maze = True
                    print(
                        "PASS generated and traversed a deep-level maze room "
                        f"from {start} to {target}"
                    )
                    break

        print("PASS 24-row map crosses all four bank boundaries without aliasing")
        if not traversed_maze:
            raise RuntimeError("fixed deep-level seeds did not produce a traversable maze")

        # The status line holds the build's only %ld, and ordinary play keeps
        # experience far below 65535, so nothing else here would notice a
        # formatter that lost the high word.  Drive it past that and read the
        # line back.  Under stat_msg the status goes to the message row in the
        # ROM font, which OCR can read; the 4x8 status row cannot be read that
        # way.  check_level only runs when experience is actually awarded, so
        # writing the field here does not trigger a level-up message.
        write_bytes(sock, status_as_message, 1)
        write_dword(sock, player_exp, 123456)
        exp_message = send_physical_key_until_ocr(
            sock, ord("."), ("Exp:1/123456",), args.timeout, "status line experience"
        )
        if "Exp:1/57920" in exp_message:
            raise RuntimeError(
                "experience printed as a 16-bit value: the printf mask lost %ld"
            )
        write_dword(sock, player_exp, 0)
        write_bytes(sock, status_as_message, 0)
        print("PASS status line formats a 32-bit experience total with %ld")

        # 'v' formats `release` from bank 0.  When vers.c was built into bank 3
        # the pointer survived the switch but the string it named did not, so
        # the line read back whatever bank 0 happened to hold at that address.
        version_message = send_physical_key_until_ocr(
            sock, ord("v"), ("5.4.4",), args.timeout, "version message"
        )
        if "trap" in version_message:
            raise RuntimeError(
                f"version message shows another bank's text: {version_message!r}"
            )
        print("PASS 'v' reads the release string from resident memory")

        # A death must cold-restart the already loaded program, not enter the
        # Spectrum ROM.  Type a real name on the second boot to also verify
        # that key timing replaces the deterministic empty-name seed.
        write_word(sock, food_remaining, (-851) & 0xFFFF)
        send_physical_key_until_ocr(
            sock,
            ord("."),
            ("Killed by starvation", "Press R to restart"),
            args.timeout,
            "starvation death screen",
        )
        send_physical_key_until_byte(
            sock, ord("r"), boot_stage, ord("H"), args.timeout, "cold restart help"
        )
        wait_for_ocr(sock, ("ROGUE ZX128 - KEYS",), args.timeout)
        send_physical_key_until_byte(
            sock, ord(" "), boot_stage, ord("N"), args.timeout, "restart name prompt"
        )
        wait_for_ocr(sock, ("Name your hero",), args.timeout)
        send_physical_key_until_ocr(
            sock, ord("a"), ("> a",), args.timeout, "first hero-name letter"
        )
        send_physical_key_until_ocr(
            sock, ord("d"), ("> ad",), args.timeout, "second hero-name letter"
        )
        send_physical_key_until_ocr(
            sock, ord("a"), ("> ada",), args.timeout, "third hero-name letter"
        )
        leave_name_prompt(sock, boot_stage, args.timeout, "restarted command loop")
        wait_for_rendered_game(sock, min(args.timeout, 5.0))
        stored_name = read_machine_ram(
            sock, bank_symbol_ram_address(hero_name), 4
        )
        if stored_name != b"ada\0":
            raise RuntimeError(f"restarted hero name is corrupt: {stored_name!r}")
        if read_word(sock, player_pack) != initial_pack:
            raise RuntimeError("cold restart did not reset the C heap")
        if read_word(sock, dungeon_level) != 1 or read_word(sock, purse) != 0:
            raise RuntimeError("cold restart retained the previous game state")
        if read_word(sock, dungeon_number) == 1:
            raise RuntimeError("typed-key timing did not select a new dungeon seed")
        if read_dword(sock, random_seed) == initial_game_seed:
            raise RuntimeError("typed-key timing reproduced the deterministic seed")
        write_bytes(sock, last_comm, 0)
        send_physical_key_until_byte(
            sock, ord("v"), last_comm, ord("v"), args.timeout, "post-restart command"
        )
        wait_for_ocr(sock, ("Version",), args.timeout)
        print("PASS death cold-restarts the game and timed name input selects a new seed")

        for attempt in range(3):
            command(sock, "send-keys-ascii 200 81")
            try:
                wait_for_ocr(sock, ("Really quit?",), 2.0)
                break
            except RuntimeError:
                if attempt == 2:
                    raise
        send_physical_key_until_ocr(
            sock,
            ord("y"),
            ("You quit with", "Press R to restart"),
            args.timeout,
            "quit restart prompt",
        )
        send_physical_key_until_byte(
            sock, ord("r"), boot_stage, ord("H"), args.timeout, "quit cold restart"
        )
        send_physical_key_until_byte(
            sock, ord(" "), boot_stage, ord("N"), args.timeout, "quit restart name"
        )
        leave_name_prompt(sock, boot_stage, args.timeout, "post-quit command loop")
        wait_for_rendered_game(sock, min(args.timeout, 5.0))
        print("PASS confirmed quit also cold-restarts the game")

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
