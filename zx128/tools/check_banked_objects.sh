#!/bin/sh

set -eu

nm_tool=${1:-z88dk-z80nm}
build_dir=${2:-build}
errors=0
objects=0

fail()
{
    echo "banked-objects: ERROR: $*" >&2
    errors=$((errors + 1))
}

object_output()
{
    if ! object_text=$($nm_tool -a "$1"); then
        fail "$nm_tool failed for $1"
        object_text=
    fi
}

section_size()
{
    printf '%s\n' "$object_text" | sed -n \
        "s/^[[:space:]]*Section $1: \([0-9][0-9]*\) bytes$/\1/p"
}

check_bss()
{
    object=$1
    section=$2
    expected=$3

    if [ ! -f "$object" ]; then
        fail "missing object: $object"
        return
    fi
    object_output "$object"
    actual=$(section_size "$section")
    if [ "$actual" != "$expected" ]; then
        fail "$object: $section is ${actual:-missing} bytes, expected $expected"
    fi
}

for bank in 0 1 3 4 6 7; do
    for object in "$build_dir/bank$bank"/*.o; do
        [ -f "$object" ] || continue
        objects=$((objects + 1))
        object_output "$object"
        init_size=$(section_size code_crt_init)
        if [ -n "$init_size" ] && [ "$init_size" != 0 ]; then
            fail "$object: code_crt_init is $init_size bytes, expected 0"
        fi
    done
done

if [ "$objects" -eq 0 ]; then
    fail "no banked objects found below $build_dir"
fi

rooms="$build_dir/bank7/rooms.o"
if [ ! -f "$rooms" ]; then
    fail "missing object: $rooms"
else
    object_output "$rooms"
    if printf '%s\n' "$object_text" | grep -Eq \
        '(^|[^[:alnum:]_])(_maze|_accnt_maze)([^[:alnum:]_]|$)'; then
        fail "$rooms: dead maze accounting symbol is still present"
    fi
fi

check_bss "$build_dir/bank0/bank_storage.o" BSS_0 2400
check_bss "$build_dir/bank1/bank_storage.o" BSS_1 1824
check_bss "$build_dir/bank3/bank_storage.o" BSS_3 3936
check_bss "$build_dir/bank4/bank_storage.o" BSS_4 1440
check_bss "$build_dir/bank4/restart_storage.o" BSS_4 1842
check_bss "$build_dir/bank7/screen_snapshot.o" BSS_7 1920
check_bss "$build_dir/bank7/restart_snapshot.o" BSS_7 2979

if [ "$errors" -ne 0 ]; then
    exit 1
fi

echo "banked-objects: OK (no banked CRT initializers, no maze matrix, 24-row map chunks)"
