#!/bin/sh
# Fat ppc + i386 build, 10.4 deployment target.
# Run on an Intel Mac with Xcode 4.6 (the last toolchain that targets ppc).
set -e
cd "$(dirname "$0")/.."

SDK=${SDK:-/Developer/SDKs/MacOSX10.6.sdk}
MIN=${MIN:-10.4}
CC=${CC:-gcc-4.2}
OUT=oldmacpatch

[ -d "$SDK" ] || { echo "SDK not found: $SDK" >&2; exit 1; }

set -x
$CC -arch ppc   -isysroot "$SDK" -mmacosx-version-min=$MIN -O2 -Wall \
    -o /tmp/omgp-ppc   src/md5.c src/pefpatch.c src/cli.c
$CC -arch i386  -isysroot "$SDK" -mmacosx-version-min=$MIN -O2 -Wall \
    -o /tmp/omgp-i386  src/md5.c src/pefpatch.c src/cli.c
lipo -create /tmp/omgp-ppc /tmp/omgp-i386 -output "$OUT"
set +x

rm -f /tmp/omgp-ppc /tmp/omgp-i386
lipo -info "$OUT"
echo "built $OUT"
