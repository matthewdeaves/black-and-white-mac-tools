#!/bin/sh
# Regression test. Needs your own copies of the game executables, which are not
# and will not be in this repository.
#
#   tests/run-tests.sh <folder-with-game-binaries>
#
# Copies are made first, so your originals are never touched.

set -u
SRC="${1:-}"
if [ -z "$SRC" ] || [ ! -d "$SRC" ]; then
    echo "usage: $0 <folder containing Black & White / Creature Isle executables>" >&2
    exit 2
fi
BIN=./oldmacpatch
[ -x "$BIN" ] || { echo "build first: make" >&2; exit 2; }

WORK=$(mktemp -d /tmp/omgp-test.XXXXXX) || exit 2
trap 'rm -rf "$WORK"' EXIT

# before-md5  after-md5  label
KNOWN="
cfdaf24d863e477ba2d0a47e16b552f5 afe1454b0801cb5c96a4130dc41f922a B&W_1.1.9_PlatinumDVD
cb76781c9bdb2cdd85aa6d9b00024c18 204c83ea1c49ae8cbde16f6d2abe2ff6 B&W_1.1.9_noCD
bf037971f339f5297d952c5b28f3e75f 1c08838b6c2d956b691878ea2dd98c8f CreatureIsle_1.1.9_PlatinumDVD
6990d3a0ee9bfb254ad2ce5b03f219cd 3390bf7aca29c0906dec2380496a2f4c CreatureIsle_1.1.9_noCD
"

md5of() { md5 -q "$1" 2>/dev/null || md5sum "$1" | cut -d' ' -f1; }

pass=0; fail=0; skip=0
find "$SRC" -type f -print | while read -r f; do
    head -c 12 "$f" 2>/dev/null | grep -q 'Joy!peffpwpc' || continue
    before=$(md5of "$f")
    match=$(echo "$KNOWN" | awk -v b="$before" '$1==b {print $2" "$3}')
    if [ -z "$match" ]; then
        echo "SKIP  unknown binary $before  ($f)"
        continue
    fi
    want_after=$(echo "$match" | cut -d' ' -f1)
    label=$(echo "$match" | cut -d' ' -f2)

    cp "$f" "$WORK/t" || { echo "FAIL  copy $label"; continue; }

    $BIN patch "$WORK/t" >/dev/null 2>&1
    got_after=$(md5of "$WORK/t")
    if [ "$got_after" = "$want_after" ]; then
        echo "PASS  patch  $label"
    else
        echo "FAIL  patch  $label: got $got_after want $want_after"
    fi

    $BIN revert "$WORK/t" >/dev/null 2>&1
    got_back=$(md5of "$WORK/t")
    if [ "$got_back" = "$before" ]; then
        echo "PASS  revert $label"
    else
        echo "FAIL  revert $label: got $got_back want $before"
    fi
    rm -f "$WORK/t" "$WORK/t.omgpbak"
done
echo
echo "done"
